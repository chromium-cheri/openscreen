// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_service_base.h"

#include "openssl/evp.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

// static
QuicAgentCertificate& QuicServiceBase::GetAgentCertificate() {
  static QuicAgentCertificate agent_certificate;
  return agent_certificate;
}

QuicServiceBase::QuicServiceBase(
    const ServiceConfig& config,
    std::unique_ptr<QuicConnectionFactoryBase> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    InstanceRequestIds::Role role,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner,
    size_t buffer_limit)
    : instance_request_ids_(role),
      demuxer_(now_function, buffer_limit),
      connection_factory_(std::move(connection_factory)),
      connection_endpoints_(config.connection_endpoints),
      observer_(observer),
      cleanup_alarm_(now_function, task_runner) {}

QuicServiceBase::~QuicServiceBase() {
  CloseAllConnections();
}

uint64_t QuicServiceBase::OnCryptoHandshakeComplete(
    std::string_view instance_name) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return 0;
  }

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    return 0;
  }

  PendingConnectionData pending_data = std::move(pending_entry->second);
  pending_connections_.erase(pending_entry);
  uint64_t instance_id = next_instance_id_++;
  instance_map_.emplace(instance_name, instance_id);
  pending_data.data.stream_manager->set_quic_connection(
      pending_data.data.connection.get());

  auto& auth_handshake_watch = pending_data.data.auth_handshake_watch;
  if (!auth_handshake_watch) {
    auth_handshake_watch = demuxer_.WatchMessageType(
        instance_id, msgs::Type::kAuthSpake2Handshake, this);
  }

  bool is_server = pending_data.callbacks.empty();
  if (is_server) {
    auto& auth_status_watch = pending_data.data.auth_status_watch;
    if (!auth_status_watch) {
      auth_status_watch =
          demuxer_.WatchMessageType(instance_id, msgs::Type::kAuthStatus, this);
    }
  } else {
    auto& auth_confirmation_watch = pending_data.data.auth_confirmation_watch;
    if (!auth_confirmation_watch) {
      auth_confirmation_watch = demuxer_.WatchMessageType(
          instance_id, msgs::Type::kAuthSpake2Confirmation, this);
    }
  }

  pending_authentications_.emplace(instance_id, std::move(pending_data));
  // The QuicServer initiates the authentication process.
  if (is_server) {
    StartAuthentication(instance_id);
  }

  return instance_id;
}

void QuicServiceBase::OnIncomingStream(uint64_t instance_id,
                                       QuicStream* stream) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  // The first incoming stream is used for receiving authentication related
  // messages.
  auto authentication_entry = pending_authentications_.find(instance_id);
  if (authentication_entry != pending_authentications_.end()) {
    authentication_entry->second.data.receiver =
        authentication_entry->second.data.stream_manager->OnIncomingStream(
            stream);
    return;
  }

  // The incoming stream after authentication is used by embedder.
  auto connection_entry = connections_.find(instance_id);
  if (connection_entry != connections_.end()) {
    std::unique_ptr<QuicProtocolConnection> connection =
        connection_entry->second.stream_manager->OnIncomingStream(stream);
    observer_.OnIncomingConnection(std::move(connection));
  }
}

void QuicServiceBase::OnConnectionClosed(uint64_t instance_id) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  auto authentication_entry = pending_authentications_.find(instance_id);
  auto connection_entry = connections_.find(instance_id);
  if (authentication_entry == pending_authentications_.end() &&
      connection_entry == connections_.end()) {
    return;
  }

  connection_factory_->OnConnectionClosed(
      connection_entry->second.connection.get());
  delete_connections_.push_back(instance_id);
  instance_request_ids_.ResetRequestId(instance_id);
}

QuicStream::Delegate& QuicServiceBase::GetStreamDelegate(uint64_t instance_id) {
  auto authentication_entry = pending_authentications_.find(instance_id);
  if (authentication_entry != pending_authentications_.end()) {
    auto& stream_manager = authentication_entry->second.data.stream_manager;
    OSP_CHECK(stream_manager);
    return *stream_manager;
  }

  auto connection_entry = connections_.find(instance_id);
  OSP_CHECK(connection_entry != connections_.end());
  auto& stream_manager = connection_entry->second.stream_manager;
  OSP_CHECK(stream_manager);
  return *stream_manager;
}

void QuicServiceBase::OnClientCertificates(
    std::string_view instance_name,
    const std::vector<std::string>& certs) {
  OSP_NOTREACHED();
}

void QuicServiceBase::OnConnectionDestroyed(
    QuicProtocolConnection& connection) {
  auto authentication_entry =
      pending_authentications_.find(connection.GetInstanceID());
  auto connection_entry = connections_.find(connection.GetInstanceID());
  if (authentication_entry == pending_authentications_.end() &&
      connection_entry == connections_.end()) {
    return;
  }

  connection_entry->second.stream_manager->DropProtocolConnection(connection);
}

void QuicServiceBase::OnDataReceived(uint64_t instance_id,
                                     uint64_t protocol_connection_id,
                                     ByteView bytes) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  demuxer_.OnStreamData(instance_id, protocol_connection_id, bytes.data(),
                        bytes.size());
}

void QuicServiceBase::OnClose(uint64_t instance_id,
                              uint64_t protocol_connection_id) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  demuxer_.OnStreamClose(instance_id, protocol_connection_id);
}

uint64_t QuicServiceBase::CompleteConnectionForTest(
    std::string_view instance_name) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return 0;
  }

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    return 0;
  }

  ServiceConnectionData connection_data = std::move(pending_entry->second.data);
  auto callbacks = std::move(pending_entry->second.callbacks);
  pending_connections_.erase(pending_entry);
  uint64_t instance_id = next_instance_id_++;
  instance_map_.emplace(instance_name, instance_id);
  connection_data.stream_manager->set_quic_connection(
      connection_data.connection.get());
  connections_.emplace(instance_id, std::move(connection_data));

  // `callbacks` is empty for QuicServer, so this only works for QuicClient.
  for (auto& request : callbacks) {
    request.second->OnConnectSucceed(request.first, instance_name, instance_id);
  }

  return instance_id;
}

QuicServiceBase::ServiceConnectionData::ServiceConnectionData(
    std::unique_ptr<QuicConnection> connection,
    std::unique_ptr<QuicStreamManager> manager)
    : connection(std::move(connection)), stream_manager(std::move(manager)) {}

QuicServiceBase::ServiceConnectionData::ServiceConnectionData(
    ServiceConnectionData&&) noexcept = default;

QuicServiceBase::ServiceConnectionData&
QuicServiceBase::ServiceConnectionData::operator=(
    ServiceConnectionData&&) noexcept = default;

QuicServiceBase::ServiceConnectionData::~ServiceConnectionData() = default;

QuicServiceBase::PendingConnectionData::PendingConnectionData(
    ServiceConnectionData&& data)
    : data(std::move(data)) {}

QuicServiceBase::PendingConnectionData::PendingConnectionData(
    PendingConnectionData&&) noexcept = default;

QuicServiceBase::PendingConnectionData&
QuicServiceBase::PendingConnectionData::operator=(
    PendingConnectionData&&) noexcept = default;

QuicServiceBase::PendingConnectionData::~PendingConnectionData() = default;

bool QuicServiceBase::StartImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kStopped) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kRunning;
  Cleanup();  // Start periodic clean-ups.
  observer_.OnRunning();
  return true;
}

bool QuicServiceBase::StopImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning &&
      state_ != ProtocolConnectionEndpoint::State::kSuspended) {
    return false;
  }

  CloseAllConnections();
  state_ = ProtocolConnectionEndpoint::State::kStopped;
  Cleanup();  // Final clean-up.
  observer_.OnStopped();
  return true;
}

bool QuicServiceBase::SuspendImpl() {
  // TODO(btolsch): QuicStreams should either buffer or reject writes.
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kSuspended;
  observer_.OnSuspended();
  return true;
}

bool QuicServiceBase::ResumeImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kSuspended) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kRunning;
  observer_.OnRunning();
  return true;
}

std::unique_ptr<ProtocolConnection>
QuicServiceBase::CreateProtocolConnectionImpl(uint64_t instance_id) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return nullptr;
  }

  auto connection_entry = connections_.find(instance_id);
  if (connection_entry == connections_.end()) {
    return nullptr;
  }

  return QuicProtocolConnection::FromExisting(
      *this, *connection_entry->second.connection,
      *connection_entry->second.stream_manager, instance_id);
}

std::vector<uint8_t> QuicServiceBase::ComputePublicValue(
    const std::vector<uint8_t>& self_private_key) {
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  OSP_CHECK(key);

  BIGNUM* private_bn =
      BN_bin2bn(self_private_key.data(), self_private_key.size(), nullptr);
  OSP_CHECK(private_bn);
  OSP_CHECK(EC_KEY_set_private_key(key, private_bn));

  EC_POINT* point = EC_POINT_new(EC_KEY_get0_group(key));
  OSP_CHECK(point);

  if (!EC_POINT_mul(EC_KEY_get0_group(key), point, private_bn, nullptr, nullptr,
                    nullptr)) {
    // Handle error.
    EC_POINT_free(point);
    BN_free(private_bn);
    EC_KEY_free(key);
    return {};
  }

  OSP_CHECK(EC_KEY_set_public_key(key, point));
  size_t length = i2o_ECPublicKey(key, nullptr);
  OSP_CHECK_GT(length, 0);
  std::vector<uint8_t> public_value(length);
  unsigned char* buf = public_value.data();
  size_t written_length = i2o_ECPublicKey(key, &buf);
  OSP_CHECK_EQ(length, written_length);

  // Release resources.
  EC_POINT_free(point);
  BN_free(private_bn);
  EC_KEY_free(key);

  return public_value;
}

std::array<uint8_t, 64> QuicServiceBase::ComputeSharedKey(
    const std::vector<uint8_t>& self_private_key,
    const std::vector<uint8_t>& peer_public_value,
    const std::string& password) {
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  OSP_CHECK(key);

  BIGNUM* private_bn =
      BN_bin2bn(self_private_key.data(), self_private_key.size(), nullptr);
  OSP_CHECK(private_bn);
  OSP_CHECK(EC_KEY_set_private_key(key, private_bn));

  const unsigned char* buf = peer_public_value.data();
  OSP_CHECK(o2i_ECPublicKey(&key, &buf, peer_public_value.size()));

  std::array<uint8_t, 32> shared_key_data;
  size_t secret_length =
      ECDH_compute_key(shared_key_data.data(), shared_key_data.size(),
                       EC_KEY_get0_public_key(key), key, nullptr);
  OSP_CHECK_GT(secret_length, 0);

  SHA512_CTX sha512;
  SHA512_Init(&sha512);
  SHA512_Update(&sha512, shared_key_data.data(), secret_length);
  SHA512_Update(&sha512, password.data(), password.size());
  std::array<uint8_t, 64> shared_key;
  SHA512_Final(shared_key.data(), &sha512);

  // Release resources.
  BN_free(private_bn);
  EC_KEY_free(key);

  return shared_key;
}

std::string_view QuicServiceBase::FindInstanceNameById(uint64_t instance_id) {
  auto instance_entry = std::find_if(
      instance_map_.begin(), instance_map_.end(),
      [instance_id](const std::pair<std::string, uint64_t>& instance) {
        return instance.second == instance_id;
      });

  if (instance_entry != instance_map_.end()) {
    return instance_entry->first;
  } else {
    return std::string_view();
  }
}

void QuicServiceBase::StartAuthentication(uint64_t instance_id) {
  OSP_NOTREACHED();
}

void QuicServiceBase::CloseAllConnections() {
  for (auto& conn : pending_connections_) {
    conn.second.data.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.data.connection.get());
    // `callbacks` is empty for QuicServer, so this only works for QuicClient.
    for (auto& item : conn.second.callbacks) {
      item.second->OnConnectFailed(item.first, conn.first);
    }
  }
  pending_connections_.clear();

  for (auto& conn : pending_authentications_) {
    conn.second.data.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.data.connection.get());
    // `callbacks` is empty for QuicServer, so this only works for QuicClient.
    for (auto& item : conn.second.callbacks) {
      item.second->OnConnectFailed(item.first,
                                   FindInstanceNameById(conn.first));
    }
  }
  pending_authentications_.clear();

  for (auto& conn : connections_) {
    conn.second.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.connection.get());
  }
  connections_.clear();

  instance_map_.clear();
  next_instance_id_ = 1;
  instance_request_ids_.Reset();
}

void QuicServiceBase::Cleanup() {
  for (uint64_t instance_id : delete_connections_) {
    auto authentication_entry = pending_authentications_.find(instance_id);
    if (authentication_entry != pending_authentications_.end()) {
      pending_authentications_.erase(authentication_entry);
    }

    auto connection_entry = connections_.find(instance_id);
    if (connection_entry != connections_.end()) {
      connections_.erase(connection_entry);
    }
  }
  delete_connections_.clear();

  constexpr Clock::duration kQuicCleanupPeriod = std::chrono::milliseconds(500);
  if (state_ != ProtocolConnectionEndpoint::State::kStopped) {
    cleanup_alarm_.ScheduleFromNow([this] { Cleanup(); }, kQuicCleanupPeriod);
  }
}

}  // namespace openscreen::osp
