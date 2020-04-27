// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_agent.h"

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
ErrorOr<TlsCredentials> CreateCredentials(const IPEndpoint& endpoint,
                                          bssl::UniquePtr<EVP_PKEY>* pkey,
                                          std::vector<uint8_t>* root_cert_bytes) {
  *pkey = nullptr;
  ErrorOr<bssl::UniquePtr<EVP_PKEY>> private_key =
      ImportRSAPrivateKey(kPrivateKeyDer.data(), kPrivateKeyDer.size());
  OSP_CHECK(private_key);

  ErrorOr<bssl::UniquePtr<X509>> root_cert = CreateSelfSignedX509Certificate(
      "Cast Generated Root Certificate", kCertificateDuration,
      *private_key.value(), GetWallTimeSinceUnixEpoch(), true);
  if (!root_cert) {
    return root_cert.error();
  }
  auto root_cert_bytes_or_error = ExportX509CertificateToDer(*root_cert.value());
  if (root_cert_bytes_or_error) {
    *root_cert_bytes = std::move(root_cert_bytes_or_error.value());
  }

  ErrorOr<bssl::UniquePtr<X509>> intermediate_cert = CreateSelfSignedX509Certificate(
      "Cast Generated Intermediate Certificate", kCertificateDuration, *private_key.value(),
      GetWallTimeSinceUnixEpoch(), true, root_cert.value().get(),
      private_key.value().get());
  if (!intermediate_cert) {
    return intermediate_cert.error();
  }

  ErrorOr<bssl::UniquePtr<X509>> cert = CreateSelfSignedX509Certificate(
      endpoint.ToString(), kCertificateDuration, *private_key.value(),
      GetWallTimeSinceUnixEpoch(), true, intermediate_cert.value().get(),
      private_key.value().get());
  if (!cert) {
    return cert.error();
  }

  auto cert_bytes = ExportX509CertificateToDer(*cert.value());
  if (!cert_bytes) {
    return cert_bytes.error();
  }

  // TODO(jophba): either refactor the TLS server socket to use the public key
  // and add a valid key here, or remove from the TlsCredentials struct.
  *pkey = std::move(private_key.value());
  return TlsCredentials(
      std::vector<uint8_t>(kPrivateKeyDer.begin(), kPrivateKeyDer.end()),
      std::vector<uint8_t>{}, std::move(cert_bytes.value()));
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
  ErrorOr<TlsCredentials> credentials =
      CreateCredentials(receive_endpoint_, &pkey, &root_cert_bytes_);
  if (!credentials) {
    return credentials.error();
  }
  credentials_provider_.tls_cert_der = credentials.value().der_x509_cert;
  credentials_provider_.device_creds.private_key = std::move(pkey);
  credentials_provider_.device_creds.certs.emplace_back(
      std::string(credentials.value().der_x509_cert.begin(),
                  credentials.value().der_x509_cert.end()));

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
  message_port_.SetSocket(socket.get());
  router_->TakeSocket(this, std::move(socket));
  controller_ =
      std::make_unique<StreamingPlaybackController>(task_runner_, this);
  current_session_ = std::make_unique<ReceiverSession>(
      controller_.get(), environment_.get(), &message_port_,
      ReceiverSession::Preferences{});
}

void CastAgent::OnError(ReceiverSocketFactory* factory, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
  current_session_.reset();
}

void CastAgent::OnClose(CastSocket* cast_socket) {
  OSP_VLOG << "Cast agent socket closed.";
  current_session_.reset();
}

void CastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
  current_session_.reset();
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
  current_session_.reset();
}

void CastAgent::OnPlaybackError(StreamingPlaybackController* controller,
                                Error error) {
  OSP_LOG_ERROR << "Cast agent received playback error: " << error;
  current_session_.reset();
}

}  // namespace cast
}  // namespace openscreen
