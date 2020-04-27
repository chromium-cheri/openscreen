// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_agent.h"

#include <openssl/mem.h>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "cast/common/channel/message_util.h"
#include "cast/standalone_receiver/cast_socket_message_port.h"
#include "cast/standalone_receiver/private_key_der.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "util/crypto/certificate_utils.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr int kDefaultMaxBacklogSize = 64;
const TlsListenOptions kDefaultListenOptions{kDefaultMaxBacklogSize};

constexpr int kThreeDaysInSeconds = 3 * 24 * 60 * 60;
constexpr auto kCertificateDuration = std::chrono::seconds(kThreeDaysInSeconds);

// Generates a valid set of credentials for use with the TLS Server socket,
// including a generated X509 certificate generated from the static private key
// stored in private_key_der.h. The certificate is valid for
// kCertificateDuration from when this function is called.
ErrorOr<TlsCredentials> CreateCredentials(
    const IPEndpoint& endpoint,
    StaticCredentialsProvider* credentials_provider,
    std::vector<uint8_t>* root_cert_der) {
  bssl::UniquePtr<EVP_PKEY> root_key = GenerateRsaKeyPair();
  bssl::UniquePtr<EVP_PKEY> intermediate_key = GenerateRsaKeyPair();
  bssl::UniquePtr<EVP_PKEY> device_key = GenerateRsaKeyPair();
  OSP_DCHECK(root_key);
  OSP_DCHECK(intermediate_key);
  OSP_DCHECK(device_key);

  ErrorOr<bssl::UniquePtr<X509>> root_cert_or_error =
      CreateSelfSignedX509Certificate("Cast Root CA", kCertificateDuration,
                                      *root_key, GetWallTimeSinceUnixEpoch(),
                                      true);
  OSP_DCHECK(root_cert_or_error);
  bssl::UniquePtr<X509> root_cert = std::move(root_cert_or_error.value());

  ErrorOr<bssl::UniquePtr<X509>> intermediate_cert_or_error =
      CreateSelfSignedX509Certificate(
          "Cast Intermediate", kCertificateDuration, *intermediate_key,
          GetWallTimeSinceUnixEpoch(), true, root_cert.get(), root_key.get());
  OSP_DCHECK(intermediate_cert_or_error);
  bssl::UniquePtr<X509> intermediate_cert =
      std::move(intermediate_cert_or_error.value());

  ErrorOr<bssl::UniquePtr<X509>> device_cert_or_error =
      CreateSelfSignedX509Certificate(endpoint.ToString(), kCertificateDuration,
                                      *device_key, GetWallTimeSinceUnixEpoch(),
                                      false, intermediate_cert.get(),
                                      intermediate_key.get());
  OSP_DCHECK(device_cert_or_error);
  bssl::UniquePtr<X509> device_cert = std::move(device_cert_or_error.value());

  // NOTE: Device cert chain plumbing + serialization.
  credentials_provider->device_creds.private_key = std::move(device_key);

  int cert_length = i2d_X509(device_cert.get(), nullptr);
  std::string cert_serial(cert_length, 0);
  uint8_t* out = reinterpret_cast<uint8_t*>(&cert_serial[0]);
  i2d_X509(device_cert.get(), &out);
  credentials_provider->device_creds.certs.emplace_back(std::move(cert_serial));

  cert_length = i2d_X509(intermediate_cert.get(), nullptr);
  cert_serial.resize(cert_length);
  out = reinterpret_cast<uint8_t*>(&cert_serial[0]);
  i2d_X509(intermediate_cert.get(), &out);
  credentials_provider->device_creds.certs.emplace_back(std::move(cert_serial));

  cert_length = i2d_X509(root_cert.get(), nullptr);
  std::vector<uint8_t> trust_anchor_der(cert_length);
  out = &trust_anchor_der[0];
  i2d_X509(root_cert.get(), &out);
  *root_cert_der = std::move(trust_anchor_der);

  // NOTE: TLS key pair + certificate generation.
  bssl::UniquePtr<EVP_PKEY> tls_key = GenerateRsaKeyPair();
  OSP_DCHECK_EQ(EVP_PKEY_id(tls_key.get()), EVP_PKEY_RSA);
  ErrorOr<bssl::UniquePtr<X509>> tls_cert_or_error =
      CreateSelfSignedX509Certificate("Test Device TLS", kCertificateDuration,
                                      *tls_key, GetWallTimeSinceUnixEpoch());
  OSP_DCHECK(tls_cert_or_error);
  bssl::UniquePtr<X509> tls_cert = std::move(tls_cert_or_error.value());

  // NOTE: TLS private key serialization.
  RSA* rsa_key = EVP_PKEY_get0_RSA(tls_key.get());
  size_t pkey_len = 0;
  uint8_t* pkey_bytes = nullptr;
  OSP_DCHECK(RSA_private_key_to_bytes(&pkey_bytes, &pkey_len, rsa_key));
  OSP_DCHECK_GT(pkey_len, 0u);
  std::vector<uint8_t> tls_key_serial(pkey_bytes, pkey_bytes + pkey_len);
  OPENSSL_free(pkey_bytes);

  // NOTE: TLS public key serialization.
  pkey_len = 0;
  pkey_bytes = nullptr;
  OSP_DCHECK(RSA_public_key_to_bytes(&pkey_bytes, &pkey_len, rsa_key));
  OSP_DCHECK_GT(pkey_len, 0u);
  std::vector<uint8_t> tls_pub_serial(pkey_bytes, pkey_bytes + pkey_len);
  OPENSSL_free(pkey_bytes);

  // NOTE: TLS cert serialization.
  cert_length = 0;
  cert_length = i2d_X509(tls_cert.get(), nullptr);
  OSP_DCHECK_GT(cert_length, 0);
  std::vector<uint8_t> tls_cert_serial(cert_length);
  out = &tls_cert_serial[0];
  i2d_X509(tls_cert.get(), &out);
  credentials_provider->tls_cert_der = tls_cert_serial;

  return TlsCredentials(std::move(tls_key_serial), std::move(tls_pub_serial),
                        std::move(tls_cert_serial));
}

}  // namespace

CastAgent::CastAgent(TaskRunner* task_runner, InterfaceInfo interface)
    : task_runner_(task_runner) {
  IPAddress address = interface.GetIpAddressV4() ? interface.GetIpAddressV4()
                                                 : interface.GetIpAddressV6();
  OSP_DCHECK(address);
  environment_ = std::make_unique<Environment>(
      &Clock::now, task_runner_,
      IPEndpoint{address, kDefaultCastStreamingPort});
  receive_endpoint_ = IPEndpoint{address, kDefaultCastPort};
}

CastAgent::~CastAgent() = default;

Error CastAgent::Start() {
  OSP_DCHECK(!current_session_);

  this->wake_lock_ = ScopedWakeLock::Create(task_runner_);

  // TODO(jophba): add command line argument for setting the private key.
  bssl::UniquePtr<EVP_PKEY> pkey;
  ErrorOr<TlsCredentials> credentials = CreateCredentials(
      receive_endpoint_, &credentials_provider_, &root_cert_bytes_);
  if (!credentials) {
    return credentials.error();
  }

  auth_handler_ = MakeSerialDelete<DeviceAuthNamespaceHandler>(
      task_runner_, &credentials_provider_);
  router_ = MakeSerialDelete<VirtualConnectionRouter>(task_runner_,
                                                      &connection_manager_);
  router_->AddHandlerForLocalId(kPlatformReceiverId, auth_handler_.get());
  socket_factory_ = MakeSerialDelete<ReceiverSocketFactory>(task_runner_, this,
                                                            router_.get());
  connection_factory_ = SerialDeletePtr<TlsConnectionFactory>(
      task_runner_,
      TlsConnectionFactory::CreateFactory(socket_factory_.get(), task_runner_)
          .release());

  task_runner_->PostTask([this, creds = std::move(credentials.value())] {
    connection_factory_->SetListenCredentials(creds);
    connection_factory_->Listen(receive_endpoint_, kDefaultListenOptions);
  });

  OSP_LOG_INFO << "Listening for connections at: " << receive_endpoint_;
  return Error::None();
}

Error CastAgent::Stop() {
  task_runner_->PostTask([this] {
    router_.reset();
    connection_factory_.reset();
    controller_.reset();
    current_session_.reset();
    socket_factory_.reset();
    wake_lock_.reset();
  });
  return Error::None();
}

void CastAgent::OnConnected(ReceiverSocketFactory* factory,
                            const IPEndpoint& endpoint,
                            std::unique_ptr<CastSocket> socket) {
  OSP_DCHECK(factory);

  if (current_session_) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }

  OSP_LOG_INFO << "Received connection from peer at: " << endpoint;
  message_port_.SetSocket(socket->GetWeakPtr());
  router_->TakeSocket(this, std::move(socket));
  controller_ =
      std::make_unique<StreamingPlaybackController>(task_runner_, this);
  current_session_ = std::make_unique<ReceiverSession>(
      controller_.get(), environment_.get(), &message_port_,
      ReceiverSession::Preferences{});
}

void CastAgent::OnError(ReceiverSocketFactory* factory, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
  StopCurrentSession();
}

void CastAgent::OnClose(CastSocket* cast_socket) {
  OSP_VLOG << "Cast agent socket closed.";
  StopCurrentSession();
}

void CastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
  StopCurrentSession();
}

// Currently we don't do anything with the receiver output--the session
// is automatically linked to the playback controller when it is constructed, so
// we don't actually have to interface with the receivers. If we end up caring
// about the receiver configurations we will have to handle OnNegotiated here.
void CastAgent::OnNegotiated(const ReceiverSession* session,
                             ReceiverSession::ConfiguredReceivers receivers) {
  OSP_VLOG << "Successfully negotiated with sender.";
}

void CastAgent::OnConfiguredReceiversDestroyed(const ReceiverSession* session) {
  OSP_VLOG << "Receiver instances destroyed.";
}

// Currently, we just kill the session if an error is encountered.
void CastAgent::OnError(const ReceiverSession* session, Error error) {
  OSP_LOG_ERROR << "Cast agent received receiver session error: " << error;
  StopCurrentSession();
}

void CastAgent::OnPlaybackError(StreamingPlaybackController* controller,
                                Error error) {
  OSP_LOG_ERROR << "Cast agent received playback error: " << error;
  StopCurrentSession();
}

void CastAgent::StopCurrentSession() {
  controller_.reset();
  current_session_.reset();
  router_->CloseSocket(message_port_.GetSocketId());
  message_port_.SetSocket(nullptr);
}

}  // namespace cast
}  // namespace openscreen
