// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_client.h"

#include <algorithm>
#include <functional>

#include "osp/public/connect_request.h"
#include "util/base64.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

QuicClient::QuicClient(
    const ServiceConfig& config,
    MessageDemuxer& demuxer,
    std::unique_ptr<QuicConnectionFactoryClient> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner)
    : QuicServiceBase(config,
                      demuxer,
                      std::move(connection_factory),
                      observer,
                      InstanceRequestIds::Role::kClient,
                      now_function,
                      task_runner) {}

QuicClient::~QuicClient() = default;

bool QuicClient::Start() {
  return StartImpl();
}

bool QuicClient::Stop() {
  return StopImpl();
}

// NOTE: Currently we do not support Suspend()/Resume() for the connection
// client.  Add those if we can define behavior for the OSP protocol and QUIC
// for those operations.
// See: https://github.com/webscreens/openscreenprotocol/issues/108
bool QuicClient::Suspend() {
  OSP_NOTREACHED();
}

bool QuicClient::Resume() {
  OSP_NOTREACHED();
}

ProtocolConnectionEndpoint::State QuicClient::GetState() {
  return state_;
}

MessageDemuxer& QuicClient::GetMessageDemuxer() {
  return demuxer_;
}

InstanceRequestIds& QuicClient::GetInstanceRequestIds() {
  return instance_request_ids_;
}

std::unique_ptr<ProtocolConnection> QuicClient::CreateProtocolConnection(
    uint64_t instance_id) {
  return CreateProtocolConnectionImpl(instance_id);
}

bool QuicClient::Connect(std::string_view instance_name,
                         std::string_view password,
                         ConnectRequest& request,
                         ConnectRequestCallback* request_callback) {
  if (state_ != State::kRunning) {
    request_callback->OnConnectFailed(0);
    OSP_LOG_ERROR << "QuicClient connect failed: QuicClient is not running.";
    return false;
  }

  auto instance_entry = instance_map_.find(instance_name);
  // If there is a `instance_entry` for `instance_name`, it means there is an
  // available connection that has already completed QUIC handshake and
  // authentication or the connection is in process of authentication.
  // Otherwise, it means there is no available connection or the connection is
  // still in the process of QUIC handshake.
  if (instance_entry != instance_map_.end()) {
    auto pending_authentication =
        pending_authentications_.find(instance_entry->second);
    if (pending_authentication != pending_authentications_.end()) {
      // Case 1: there is a connection in process of authentication.
      uint64_t request_id = next_request_id_++;
      pending_authentication->second.callbacks.emplace_back(request_id,
                                                            request_callback);
      request = ConnectRequest(this, request_id);
      return true;
    } else {
      // Case 2: there is a connection that has already completed QUIC
      // handshake and authentication.
      uint64_t request_id = next_request_id_++;
      request = ConnectRequest(this, request_id);
      request_callback->OnConnectSucceed(request_id, instance_entry->second);
      return true;
    }
  } else {
    auto pending_connection = pending_connections_.find(instance_name);
    if (pending_connection != pending_connections_.end()) {
      // Case 3: there is a connection in process of QUIC handshake.
      uint64_t request_id = next_request_id_++;
      pending_connection->second.callbacks.emplace_back(request_id,
                                                        request_callback);
      request = ConnectRequest(this, request_id);
      return true;
    } else {
      // Case 4: there is no available connection.
      return StartConnectionRequest(instance_name, password, request,
                                    request_callback);
    }
  }
}

ErrorOr<size_t> QuicClient::OnStreamMessage(uint64_t instance_id,
                                            uint64_t connection_id,
                                            msgs::Type message_type,
                                            const uint8_t* buffer,
                                            size_t buffer_size,
                                            Clock::time_point now) {
  auto authentication_entry = pending_authentications_.find(instance_id);
  if (authentication_entry == pending_authentications_.end()) {
    return Error::Code::kNoActiveConnection;
  }

  auto instance_entry = std::find_if(
      instance_map_.begin(), instance_map_.end(),
      [instance_id](const std::pair<std::string, uint64_t>& instance) {
        return instance.second == instance_id;
      });
  if (instance_entry == instance_map_.end()) {
    HandleAuthenticationFailure(instance_id);
    return Error::Code::kItemNotFound;
  }

  auto instance_infos_entry = instance_infos_.find(instance_entry->first);
  if (instance_infos_entry == instance_infos_.end()) {
    HandleAuthenticationFailure(instance_id);
    return Error::Code::kItemNotFound;
  }

  auto& sender = authentication_entry->second.data.sender;
  if (!sender) {
    sender = QuicProtocolConnection::FromExisting(
        *this, *authentication_entry->second.data.connection,
        *authentication_entry->second.data.stream_manager, instance_id);
  }

  switch (message_type) {
    case msgs::Type::kAuthSpake2Handshake: {
      msgs::AuthSpake2Handshake handshake;
      ssize_t result =
          msgs::DecodeAuthSpake2Handshake(buffer, buffer_size, handshake);
      if (result < 0) {
        OSP_LOG_WARN << "parse error: " << result;
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        HandleAuthenticationFailure(instance_id);
        return Error::Code::kCborParsing;
      } else {
        OSP_CHECK(handshake.initiation_token.has_token);
        OSP_CHECK_EQ(handshake.psk_status,
                     msgs::AuthSpake2PskStatus::kPskShown);
        if (handshake.initiation_token.token ==
            instance_infos_entry->second.auth_token) {
          std::string fingerprint = GetAgentCertificate().GetAgentFingerprint();
          std::vector<uint8_t> decoded_fingerprint;
          base64::Decode(fingerprint, &decoded_fingerprint);
          // Compute and save the shared key for verification later.
          authentication_entry->second.data.shared_key =
              ComputeSharedKey(decoded_fingerprint, handshake.public_value,
                               instance_infos_entry->second.password);
          msgs::AuthSpake2Handshake message = {
              .initiation_token =
                  msgs::AuthInitiationToken{
                      .has_token = true,
                      .token = instance_infos_entry->second.auth_token,
                  },
              .psk_status = msgs::AuthSpake2PskStatus::kPskInput,
              .public_value = ComputePublicValue(decoded_fingerprint)};
          sender->WriteMessage(message, &msgs::EncodeAuthSpake2Handshake);
        } else {
          OSP_LOG_WARN << "Authentication failed: initiation token mismatch";
          HandleAuthenticationFailure(instance_id);
        }
        return result;
      }
    }
    case msgs::Type::kAuthSpake2Confirmation: {
      msgs::AuthSpake2Confirmation confirmation;
      ssize_t result =
          msgs::DecodeAuthSpake2Confirmation(buffer, buffer_size, confirmation);
      if (result < 0) {
        OSP_LOG_WARN << "parse error: " << result;
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        HandleAuthenticationFailure(instance_id);
        return Error::Code::kCborParsing;
      } else {
        if (std::equal(authentication_entry->second.data.shared_key.begin(),
                       authentication_entry->second.data.shared_key.end(),
                       confirmation.confirmation_value.begin())) {
          msgs::AuthStatus status = {
              .result = msgs::AuthStatusResult::kAuthenticated};
          sender->WriteMessage(status, &msgs::EncodeAuthStatus);
          connections_.emplace(instance_id,
                               std::move(authentication_entry->second.data));
          for (auto& request : authentication_entry->second.callbacks) {
            request.second->OnConnectSucceed(request.first, instance_id);
          }
          pending_authentications_.erase(authentication_entry);
        } else {
          OSP_LOG_WARN << "Authentication failed: shared key mismatch";
          msgs::AuthStatus status = {.result =
                                         msgs::AuthStatusResult::kProofInvalid};
          sender->WriteMessage(status, &msgs::EncodeAuthStatus);
          HandleAuthenticationFailure(instance_id);
        }
        return result;
      }
    }
    default: {
      OSP_LOG_WARN << "QuicClient receives message with unprocessable type.";
      break;
    }
  }
  return Error::Code::kCborParsing;
}

void QuicClient::OnStarted() {}
void QuicClient::OnStopped() {}
void QuicClient::OnSuspended() {}
void QuicClient::OnSearching() {}

void QuicClient::OnReceiverAdded(const ServiceInfo& info) {
  instance_infos_.insert(std::make_pair(
      info.instance_name, InstanceInfo{info.fingerprint, info.auth_token,
                                       info.v4_endpoint, info.v6_endpoint}));
}

void QuicClient::OnReceiverChanged(const ServiceInfo& info) {
  instance_infos_[info.instance_name] = InstanceInfo{
      info.fingerprint, info.auth_token, info.v4_endpoint, info.v6_endpoint};
}

void QuicClient::OnReceiverRemoved(const ServiceInfo& info) {
  instance_infos_.erase(info.instance_name);
}

void QuicClient::OnAllReceiversRemoved() {
  instance_infos_.clear();
}

void QuicClient::OnError(const Error&) {}
void QuicClient::OnMetrics(ServiceListener::Metrics) {}

bool QuicClient::StartConnectionRequest(
    std::string_view instance_name,
    std::string_view password,
    ConnectRequest& request,
    ConnectRequestCallback* request_callback) {
  auto instance_entry = instance_infos_.find(instance_name);
  if (instance_entry == instance_infos_.end()) {
    request_callback->OnConnectFailed(0);
    OSP_LOG_ERROR << "QuicClient connect failed: can't find information for "
                  << instance_name;
    return false;
  }

  IPEndpoint endpoint = instance_entry->second.v4_endpoint
                            ? instance_entry->second.v4_endpoint
                            : instance_entry->second.v6_endpoint;
  QuicConnectionFactoryClient::ConnectData connect_data = {
      .instance_name = std::string(instance_name),
      .fingerprint = instance_entry->second.fingerprint};
  ErrorOr<std::unique_ptr<QuicConnection>> connection =
      static_cast<QuicConnectionFactoryClient*>(connection_factory_.get())
          ->Connect(connection_endpoints_[0], endpoint, connect_data, this);
  if (!connection) {
    request_callback->OnConnectFailed(0);
    OSP_LOG_ERROR << "Factory connect failed: " << connection.error();
    return false;
  }

  // Save the password and it will be used in authentication later.
  instance_entry->second.password = password;
  auto pending_result = pending_connections_.emplace(
      instance_name, PendingConnectionData(ServiceConnectionData(
                         std::move(connection.value()),
                         std::make_unique<QuicStreamManager>(*this))));
  uint64_t request_id = next_request_id_++;
  request = ConnectRequest(this, request_id);
  pending_result.first->second.callbacks.emplace_back(request_id,
                                                      request_callback);
  return true;
}

void QuicClient::CancelConnectRequest(uint64_t request_id) {
  // Remove request from `pending_connections_`.
  for (auto it = pending_connections_.begin(); it != pending_connections_.end();
       ++it) {
    auto& callbacks = it->second.callbacks;
    auto size_before_delete = callbacks.size();
    callbacks.erase(
        std::remove_if(
            callbacks.begin(), callbacks.end(),
            [request_id](
                const std::pair<uint64_t, ConnectRequestCallback*>& callback) {
              return request_id == callback.first;
            }),
        callbacks.end());

    if (callbacks.empty()) {
      pending_connections_.erase(it);
      return;
    }

    // If the size of the callbacks vector has changed, we have found the entry
    // and can break out of the loop.
    if (size_before_delete > callbacks.size()) {
      return;
    }
  }

  // Remove request from `pending_authentications_`.
  for (auto it = pending_authentications_.begin();
       it != pending_authentications_.end(); ++it) {
    auto& callbacks = it->second.callbacks;
    auto size_before_delete = callbacks.size();
    callbacks.erase(
        std::remove_if(
            callbacks.begin(), callbacks.end(),
            [request_id](
                const std::pair<uint64_t, ConnectRequestCallback*>& callback) {
              return request_id == callback.first;
            }),
        callbacks.end());

    if (callbacks.empty()) {
      pending_authentications_.erase(it);
      return;
    }

    // If the size of the callbacks vector has changed, we have found the entry
    // and can break out of the loop.
    if (size_before_delete > callbacks.size()) {
      return;
    }
  }
}

void QuicClient::HandleAuthenticationFailure(uint64_t instance_id) {
  auto authentication_entry = pending_authentications_.find(instance_id);
  if (authentication_entry == pending_authentications_.end()) {
    return;
  }

  for (auto& request : authentication_entry->second.callbacks) {
    request.second->OnConnectFailed(0);
  }
  pending_authentications_.erase(authentication_entry);
}

}  // namespace openscreen::osp
