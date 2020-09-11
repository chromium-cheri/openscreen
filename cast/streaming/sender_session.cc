// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_session.h"

#include <openssl/rand.h>
#include <stdint.h>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender.h"
#include "util/crypto/random_bytes.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

namespace {

AudioStream CreateStream(int index, const AudioCaptureOption& config) {
  return AudioStream{Stream{index,
                            Stream::Type::kAudioSource,
                            config.channels,
                            CodecToString(config.codec),
                            GetPayloadType(config.codec),
                            GenerateSsrc(true /*high_priority*/),
                            config.target_playout_delay,
                            crypto::GenerateRandomBytes16(),
                            crypto::GenerateRandomBytes16(),
                            false /* receiver_rtcp_event_log */,
                            {} /* receiver_rtcp_dscp */,
                            config.sample_rate},
                     config.bit_rate};
}

Resolution ToResolution(const DisplayResolution& display_resolution) {
  return Resolution{display_resolution.width, display_resolution.height};
}

VideoStream CreateStream(int index, const VideoCaptureOption& config) {
  std::vector<Resolution> resolutions;
  std::transform(config.resolutions.begin(), config.resolutions.end(),
                 std::back_inserter(resolutions), ToResolution);

  constexpr int kVideoStreamChannelCount = 1;
  return VideoStream{
      Stream{index,
             Stream::Type::kVideoSource,
             kVideoStreamChannelCount,
             CodecToString(config.codec),
             GetPayloadType(config.codec),
             GenerateSsrc(false /*high_priority*/),
             config.target_playout_delay,
             crypto::GenerateRandomBytes16(),
             crypto::GenerateRandomBytes16(),
             false /* receiver_rtcp_event_log */,
             {} /* receiver_rtcp_dscp */,
             kRtpTimebase},
      SimpleFraction{config.max_frame_rate.numerator,
                     config.max_frame_rate.denominator},
      config.max_bit_rate,
      config.protection,
      config.profile,
      config.level,
      std::move(resolutions),
      {} /* error_recovery mode, always "castv2" */
  };
}

template <typename S, typename C>
void CreateStreamList(int offset_index,
                      const std::vector<C>& configs,
                      std::vector<S>* out) {
  out->reserve(configs.size());
  for (size_t i = 0; i < configs.size(); ++i) {
    out->emplace_back(CreateStream(i + offset_index, configs[i]));
  }
}

Offer CreateOffer(const std::vector<AudioCaptureOption>& audio_configs,
                  const std::vector<VideoCaptureOption>& video_configs) {
  Offer offer{
      {CastMode::Type::kMirroring},
      false /* supports_wifi_status_reporting */,
      {} /* audio_streams */,
      {} /* video_streams */
  };

  // NOTE here: IDs will always follow the pattern:
  // [0.. audio streams... N - 1][N.. video streams.. K]
  CreateStreamList(0, audio_configs, &offer.audio_streams);
  CreateStreamList(audio_configs.size(), video_configs, &offer.video_streams);

  return offer;
}

std::string GenerateSenderId() {
  int16_t id = 0;
  OSP_DCHECK(RAND_pseudo_bytes(reinterpret_cast<uint8_t*>(&id), 2) != -1);

  return "sender-" + std::to_string(id);
}

bool IsValidAudioCaptureOption(const AudioCaptureOption& config) {
  return config.channels >= 1 && config.bit_rate > 0;
}

bool IsValidResolution(const DisplayResolution& resolution) {
  return resolution.width > kMinVideoWidth &&
         resolution.height > kMinVideoHeight;
}

bool IsValidVideoCaptureOption(const VideoCaptureOption& config) {
  return config.max_frame_rate.numerator > 0 &&
         config.max_frame_rate.denominator > 0 && config.max_bit_rate > 0 &&
         !config.resolutions.empty() &&
         std::all_of(config.resolutions.begin(), config.resolutions.end(),
                     IsValidResolution);
}

bool AreAllValid(const std::vector<AudioCaptureOption>& audio_configs,
                 const std::vector<VideoCaptureOption>& video_configs) {
  return std::all_of(audio_configs.begin(), audio_configs.end(),
                     IsValidAudioCaptureOption) &&
         std::all_of(video_configs.begin(), video_configs.end(),
                     IsValidVideoCaptureOption);
}
}  // namespace

SenderSession::Client::~Client() = default;

SenderSession::SenderSession(IPAddress remote_address,
                             Client* const client,
                             Environment* environment,
                             MessagePort* message_port)
    : sender_id_(GenerateSenderId()),
      remote_address_(remote_address),
      client_(client),
      environment_(environment),
      message_port_(message_port),
      packet_router_(environment_) {
  OSP_DCHECK(client_);
  OSP_DCHECK(message_port_);
  OSP_DCHECK(environment_);

  message_port_->SetClient(this);
}

SenderSession::~SenderSession() {
  message_port_->SetClient(nullptr);
}

Error SenderSession::Negotiate(std::vector<AudioCaptureOption> audio_configs,
                               std::vector<VideoCaptureOption> video_configs) {
  // Negotiating with no streams doesn't make any sense.
  if (audio_configs.empty() && video_configs.empty()) {
    return Error(Error::Code::kParameterInvalid,
                 "Need at least one audio or video config to negotiate.");
  }
  if (!AreAllValid(audio_configs, video_configs)) {
    return Error(Error::Code::kParameterInvalid, "Invalid configs provided.");
  }

  Offer offer = CreateOffer(audio_configs, video_configs);
  ErrorOr<Json::Value> json_offer = offer.ToJson();
  if (json_offer.is_error()) {
    return std::move(json_offer.error());
  }

  current_negotiation_ = std::unique_ptr<Negotiation>(
      new Negotiation{std::move(offer), audio_configs, video_configs});

  Json::Value message_body;
  message_body[kMessageKeyType] = kMessageTypeOffer;
  message_body[kOfferMessageBody] = std::move(json_offer.value());

  Message message;
  message.sender_id = sender_id_;
  message.message_namespace = kCastWebrtcNamespace;
  message.body = std::move(message_body);
  SendMessage(&message);
  return Error::None();
}

void SenderSession::OnMessage(const std::string& sender_id,
                              const std::string& message_namespace,
                              const std::string& message) {
  if (!current_negotiation_) {
    OSP_DLOG_INFO << "Received message but not currently negotiating.";
    return;
  }

  ErrorOr<Json::Value> message_json = json::Parse(message);

  if (!message_json) {
    OSP_DLOG_WARN << "Received an invalid message: " << message;
    return;
  }

  std::string key;
  if (!json::ParseAndValidateString(message_json.value()[kKeyType], &key)) {
    OSP_DLOG_WARN << "Received message with invalid message key, dropping.";
    return;
  }

  OSP_DVLOG << "Received a message: " << message;
  if (key == kMessageTypeAnswer) {
    int sequence_number;
    if (!json::ParseAndValidateInt(message_json.value()[kSequenceNumber],
                                   &sequence_number)) {
      OSP_DLOG_WARN << "Received invalid message sequence number, dropping.";
      return;
    }

    if (sequence_number != current_sequence_number_) {
      OSP_DLOG_WARN
          << "Received message with mismatched sequence number, dropping.";
      return;
    }

    const Json::Value body =
        std::move(message_json.value()[kAnswerMessageBody]);
    if (body.isNull()) {
      client_->OnError(
          this, Error(Error::Code::kJsonParseError, "Failed to parse answer"));
      OSP_DLOG_WARN
          << "Received message with invalid answer message body, dropping.";
      return;
    }
    OnAnswer(body);
  }
  current_negotiation_.reset();
}

void SenderSession::OnError(Error error) {
  OSP_DLOG_WARN << "SenderSession message port error: " << error;
}

void SenderSession::OnAnswer(const Json::Value& message_body) {
  Answer answer;
  if (!Answer::ParseAndValidate(message_body, &answer)) {
    client_->OnError(this, Error(Error::Code::kJsonParseError,
                                 "Received invalid answer message"));
    OSP_DLOG_WARN << "Received invalid answer message";
    return;
  }

  ConfiguredSenders senders = SpawnSenders(answer);
  client_->OnNegotiated(this, std::move(senders),
                        capture_recommendations::GetRecommendations(answer));
}

std::unique_ptr<Sender> SenderSession::CreateSender(Ssrc receiver_ssrc,
                                                    const Stream& stream,
                                                    RtpPayloadType type) {
  SessionConfig config{
      stream.ssrc,         receiver_ssrc,  stream.rtp_timebase, stream.channels,
      stream.target_delay, stream.aes_key, stream.aes_iv_mask};

  return std::make_unique<Sender>(environment_, &packet_router_,
                                  std::move(config), type);
}

ConfiguredSenders SenderSession::SpawnSenders(const Answer& answer) {
  OSP_DCHECK(current_negotiation_);

  // Although we already have a message port set up with the TLS
  // address of the receiver, we don't know how to connect to it
  // over UDP until we get the ANSWER message here.
  environment_->set_remote_endpoint(
      IPEndpoint{remote_address_, static_cast<uint16_t>(answer.udp_port)});

  ConfiguredSenders senders;
  for (size_t i = 0; i < answer.send_indexes.size(); ++i) {
    const Ssrc receiver_ssrc = answer.ssrcs[i];
    const int send_index = answer.send_indexes[i];
    OSP_DCHECK(send_index >= 0);
    const size_t config_index = static_cast<size_t>(send_index);

    if (config_index < current_negotiation_->audio_configs.size()) {
      const AudioCaptureOption& config = current_negotiation_->audio_configs[i];
      const RtpPayloadType payload_type = GetPayloadType(config.codec);
      for (const AudioStream& stream :
           current_negotiation_->offer.audio_streams) {
        if (stream.stream.index == send_index) {
          current_audio_sender_ =
              CreateSender(receiver_ssrc, stream.stream, payload_type);
          senders.audio = current_audio_sender_.get();
          break;
        }
      }
    } else {
      const size_t total_size = current_negotiation_->audio_configs.size() +
                                current_negotiation_->video_configs.size();
      OSP_DCHECK(config_index < total_size);
      const size_t array_index =
          config_index - current_negotiation_->audio_configs.size();

      const VideoCaptureOption& config =
          current_negotiation_->video_configs[array_index];
      const RtpPayloadType payload_type = GetPayloadType(config.codec);
      for (const VideoStream& stream :
           current_negotiation_->offer.video_streams) {
        if (stream.stream.index == send_index) {
          current_video_sender_ =
              CreateSender(receiver_ssrc, stream.stream, payload_type);
          senders.video = current_video_sender_.get();
          break;
        }
      }
    }
  }
  return senders;
}

void SenderSession::SendMessage(Message* message) {
  message->body[kSequenceNumber] = ++current_sequence_number_;

  auto body_or_error = json::Stringify(message->body);
  if (body_or_error.is_value()) {
    OSP_DVLOG << "Sending message: SENDER[" << message->sender_id
              << "], NAMESPACE[" << message->message_namespace << "], BODY:\n"
              << body_or_error.value();
    message_port_->PostMessage(message->sender_id, message->message_namespace,
                               body_or_error.value());
  } else {
    OSP_DLOG_WARN << "Sending message failed with error:\n"
                  << body_or_error.error();
    client_->OnError(this, body_or_error.error());
  }
}

}  // namespace cast
}  // namespace openscreen
