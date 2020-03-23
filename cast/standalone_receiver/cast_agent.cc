// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_agent.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "cast/standalone_receiver/cast_socket_message_port.h"
#include "cast/standalone_receiver/simple_message_port.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver_session.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "util/crypto/certificate_utils.h"
#include "util/json/json_serialization.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

////////////////////////////////////////////////////////////////////////////////
// Receiver Configuration
//
// The values defined here are constants that correspond to the Sender Demo app.
// In a production environment, these should ABSOLUTELY NOT be fixed! Instead a
// sender↔receiver OFFER/ANSWER exchange should establish them.

// In a production environment, this would start-out at some initial value
// appropriate to the networking environment, and then be adjusted by the
// application as: 1) the TYPE of the content changes (interactive, low-latency
// versus smooth, higher-latency buffered video watching); and 2) the networking
// environment reliability changes.
constexpr std::chrono::milliseconds kTargetPlayoutDelay =
    kDefaultTargetPlayoutDelay;

const Offer kDemoOffer{
    /* .cast_mode = */ CastMode{},
    /* .supports_wifi_status_reporting = */ false,
    {AudioStream{Stream{/* .index = */ 0,
                        /* .type = */ Stream::Type::kAudioSource,
                        /* .channels = */ 2,
                        /* .codec_name = */ "opus",
                        /* .rtp_payload_type = */ RtpPayloadType::kAudioOpus,
                        /* .ssrc = */ 1,
                        /* .target_delay */ kTargetPlayoutDelay,
                        /* .aes_key = */
                        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                         0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
                        /* .aes_iv_mask = */
                        {0xf0, 0xe0, 0xd0, 0xc0, 0xb0, 0xa0, 0x90, 0x80, 0x70,
                         0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00},
                        /* .receiver_rtcp_event_log = */ false,
                        /* .receiver_rtcp_dscp = */ "",
                        // In a production environment, this would be set to the
                        // sampling rate of the audio capture.
                        /* .rtp_timebase = */ 48000},
                 /* .bit_rate = */ 0}},
    {VideoStream{
        Stream{/* .index = */ 1,
               /* .type = */ Stream::Type::kVideoSource,
               /* .channels = */ 1,
               /* .codec_name = */ "vp8",
               /* .rtp_payload_type = */ RtpPayloadType::kVideoVp8,
               /* .ssrc = */ 50001,
               /* .target_delay */ kTargetPlayoutDelay,
               /* .aes_key = */
               {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f},
               /* .aes_iv_mask = */
               {0xf1, 0xe1, 0xd1, 0xc1, 0xb1, 0xa1, 0x91, 0x81, 0x71, 0x61,
                0x51, 0x41, 0x31, 0x21, 0x11, 0x01},
               /* .receiver_rtcp_event_log = */ false,
               /* .receiver_rtcp_dscp = */ "",
               // In a production environment, this would be set to the sampling
               // rate of the audio capture.
               /* .rtp_timebase = */ static_cast<int>(kVideoTimebase::den)},
        /* .max_frame_rate = */ SimpleFraction{60, 1},
        // Approximate highend bitrate for 1080p 60fps
        /* .max_bit_rate = */ 6960000,
        /* .protection = */ "",
        /* .profile = */ "",
        /* .level = */ "",
        /* .resolutions = */ {},
        /* .error_recovery_mode = */ ""}}};

// End of Receiver Configuration.
////////////////////////////////////////////////////////////////////////////////

constexpr int kDefaultMaxBacklogSize = 64;
const TlsListenOptions kDefaultListenOptions{
    /* .backlog_size = */ kDefaultMaxBacklogSize};

constexpr int kSecondsInAYear = 31556952;
constexpr auto kCertificateDuration = std::chrono::seconds(kSecondsInAYear);

// TODO: put in correct path
constexpr char kCertificateFile[] = "cast/standalone_receiver/private.der";

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
  task_runner_->PostTask(
      [this] { this->wake_lock_ = ScopedWakeLock::Create(); });

  // TODO(jophba): add command line argument for setting the private key.
  ErrorOr<TlsCredentials> credentials =
      CreateCredentials(receive_endpoint_, kCertificateFile);
  if (!credentials) {
    return credentials.error();
  }

  // TODO(jophba, rwkeane): begin discovery process before creating tls
  // connection factory instance.
  socket_factory_ = std::make_unique<ReceiverSocketFactory>(this, this);
  connection_factory_ =
      TlsConnectionFactory::CreateFactory(socket_factory_.get(), task_runner_);

  connection_factory_->SetListenCredentials(credentials.value());
  connection_factory_->Listen(receive_endpoint_, kDefaultListenOptions);

  OSP_LOG_INFO << "Listening for connections at: " << receive_endpoint_;
  return Error::None();
}

Error CastAgent::StartLegacy() {
  task_runner_->PostTask(
      [this] { this->wake_lock_ = ScopedWakeLock::Create(); });

  // TODO(jophba): replace start logic with binding to TLS server socket and
  // instantiating a session from the actual sender offer message.
  auto port = std::make_unique<SimpleMessagePort>();
  auto* raw_port = port.get();

  StreamingPlaybackController client(task_runner_);
  ReceiverSession session(&client, environment_.get(), std::move(port),
                          ReceiverSession::Preferences{});

  OSP_LOG_INFO << "Injecting fake offer message...";
  auto offer_json = kDemoOffer.ToJson();
  OSP_DCHECK(offer_json.is_value());
  Json::Value message;
  message["seqNum"] = 0;
  message["type"] = "OFFER";
  message["offer"] = offer_json.value();
  auto offer_message = json::Stringify(message);
  OSP_DCHECK(offer_message.is_value());
  raw_port->ReceiveMessage(offer_message.value());

  OSP_LOG_INFO << "Awaiting first Cast Streaming packet at "
               << environment_->GetBoundLocalEndpoint() << "...";

  return Error::None();
}

Error CastAgent::Stop() {
  // TODO(jophba): implement stopping.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

// TODO: do we want the cast agent to link to all message ports?? the
// organization here is wonky.
void CastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
}

void CastAgent::OnMessage(CastSocket* socket,
                          ::cast::channel::CastMessage message) {
  // TODO(jophba): implement message handling.
  OSP_UNIMPLEMENTED();
}

void CastAgent::OnConnected(ReceiverSocketFactory* factory,
                            const IPEndpoint& endpoint,
                            std::unique_ptr<CastSocket> socket) {
  OSP_DCHECK(factory != nullptr);
  OSP_LOG_INFO << "Received connection from peer at: " << endpoint;

  auto port = std::make_unique<CastSocketMessagePort>(std::move(socket));

  playback_controller_ =
      std::make_unique<StreamingPlaybackController>(task_runner_);
  current_session_ = std::make_unique<ReceiverSession>(
      playback_controller_.get(), environment_.get(), std::move(port),
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
