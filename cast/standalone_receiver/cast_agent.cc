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
#include "cast/standalone_receiver/cast_socket_message_port.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "util/crypto/certificate_utils.h"
#include "util/json/json_serialization.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr int kDefaultMaxBacklogSize = 64;
const TlsListenOptions kDefaultListenOptions{kDefaultMaxBacklogSize};

constexpr int kSecondsInAYear = 31556952;
constexpr auto kCertificateDuration = std::chrono::seconds(kSecondsInAYear);

// Important note about private keys and security: For example usage purposes,
// we have checked in a default private key here. However, in a production
// environment keys should never be checked into source control, and should be
// securely stored and access controlled.
constexpr char kPrivateKeyPath[] =
    OPENSCREEN_ROOT "cast/standalone_receiver/private.der";

ErrorOr<std::vector<uint8_t>> GetBytes(absl::string_view filename) {
  std::ifstream ifs(std::string(filename).c_str(),
                    std::ios::binary | std::ios::ate);
  if (ifs.fail()) {
    return Error(
        Error::Code::kItemNotFound,
        absl::StrCat("Failed to open stream with file name: ", filename));
  }

  const std::ifstream::pos_type length = ifs.tellg();
  std::vector<uint8_t> out(length);
  ifs.seekg(0, std::ios::beg);
  ifs.read(reinterpret_cast<char*>(out.data()), length);

  return out;
}

ErrorOr<TlsCredentials> CreateCredentials(
    const IPEndpoint& endpoint,
    absl::string_view private_key_filename) {
  std::ostringstream name;
  name << endpoint;

  ErrorOr<std::vector<uint8_t>> private_key = GetBytes(private_key_filename);
  if (!private_key) {
    return private_key.error();
  }

  auto pkey = ImportRSAPrivateKey(private_key.value().data(),
                                  private_key.value().size());
  if (!pkey) {
    return pkey.error();
  }

  auto cert =
      CreateCertificate(name.str(), kCertificateDuration, *pkey.value());
  if (!cert) {
    return cert.error();
  }

  auto cert_bytes = ExportCertificate(*cert.value());
  if (!cert_bytes) {
    return cert_bytes.error();
  }

  // TODO(jophba): no one uses the public key, so remove in a separate patch.
  return TlsCredentials(std::move(private_key.value()), std::vector<uint8_t>{},
                        std::move(cert_bytes.value()));
}

}  // namespace

CastAgent::CastAgent(TaskRunnerImpl* task_runner, InterfaceInfo interface)
    : task_runner_(task_runner) {
  // Create the Environment that holds the required injected dependencies
  // (clock, task runner) used throughout the system, and owns the UDP socket
  // over which all communication occurs with the Sender.
  IPEndpoint receive_endpoint{IPAddress::kV4LoopbackAddress,
                              openscreen::cast::kDefaultCastStreamingPort};
  if (interface.GetIpAddressV4()) {
    receive_endpoint.address = interface.GetIpAddressV4();
  } else if (interface.GetIpAddressV6()) {
    receive_endpoint.address = interface.GetIpAddressV6();
  }
  environment_ = std::make_unique<Environment>(&Clock::now, task_runner_,
                                               receive_endpoint);
  receive_endpoint_ = std::move(receive_endpoint);
}

CastAgent::~CastAgent() = default;

Error CastAgent::Start() {
  OSP_DCHECK(!current_session_);

  task_runner_->PostTask(
      [this] { this->wake_lock_ = ScopedWakeLock::Create(); });

  // TODO(jophba): add command line argument for setting the private key.
  ErrorOr<TlsCredentials> credentials =
      CreateCredentials(receive_endpoint_, kPrivateKeyPath);
  if (!credentials) {
    return credentials.error();
  }

  // TODO(jophba, rwkeane): begin discovery process before creating tls
  // connection factory instance.
  socket_factory_ =
      std::make_unique<ReceiverSocketFactory>(this, &message_port_);
  connection_factory_ =
      TlsConnectionFactory::CreateFactory(socket_factory_.get(), task_runner_);

  task_runner_->PostTask([this, creds = std::move(credentials.value())] {
    connection_factory_->SetListenCredentials(creds);
    connection_factory_->Listen(receive_endpoint_, kDefaultListenOptions);
  });

  OSP_LOG_INFO << "Listening for connections at: " << receive_endpoint_;
  return Error::None();
}

Error CastAgent::Stop() {
  controller_.reset();
  current_session_.reset();
  return Error::None();
}

void CastAgent::OnConnected(ReceiverSocketFactory* factory,
                            const IPEndpoint& endpoint,
                            std::unique_ptr<CastSocket> socket) {
  OSP_DCHECK(factory != nullptr);

  if (current_session_) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }

  OSP_LOG_INFO << "Received connection from peer at: " << endpoint;
  message_port_.SetSocket(std::move(socket));
  controller_ = std::make_unique<StreamingPlaybackController>(task_runner_);
  current_session_ = std::make_unique<ReceiverSession>(
      controller_.get(), environment_.get(), &message_port_,
      ReceiverSession::Preferences{});
}

void CastAgent::OnError(ReceiverSocketFactory* factory, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
}

// Currently we don't do anything with the receiver output--the session
// is automatically linked to the playback controller.
void CastAgent::OnNegotiated(const ReceiverSession* session,
                             ReceiverSession::ConfiguredReceivers receivers) {
  OSP_LOG_INFO << "Successfully negotiated with sender.";
}

void CastAgent::OnReceiversDestroyed(const ReceiverSession* session) {
  OSP_LOG_INFO << "Receiver instances destroyed.";
}

// Currently, we just kill the session if an error is encountered.
void CastAgent::OnError(const ReceiverSession* session, Error error) {
  OSP_LOG_ERROR << "Cast agent received receiver session error: " << error;
  current_session_.reset();
}

}  // namespace cast
}  // namespace openscreen
