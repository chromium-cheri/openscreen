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
#include "util/crypto/aes_key.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

namespace {

// Using statements for constructor readability.
using ConfiguredSenders = SenderSession::ConfiguredSenders;

// Namespace for OFFER/ANSWER messages.
constexpr char kCastWebrtcNamespace[] = "urn:x-cast:com.google.cast.webrtc";

// Default target delay for both audio and video.
constexpr auto kTargetDelay = std::chrono::milliseconds(400);

// Timebase to be used for all RTP payload types.
constexpr int kRtpTimebase = 96000;

// NOTE: currently we match the legacy Chrome sender's behavior of always
// sending the audio and video hacks for AndroidTV, however we should migrate
// to using proper rtp payload types. New payload types for new codecs, such
// as VP9, should also be defined.
RtpPayloadType GetPayloadType(AudioCodec codec) {
  return RtpPayloadType::kAudioHackForAndroidTV;
}
RtpPayloadType GetPayloadType(VideoCodec codec) {
  return RtpPayloadType::kVideoHackForAndroidTV;
}

ErrorOr<AudioStream> CreateStream(int index, const AudioConfig& config) {
  ErrorOr<AESKey> aes_key = AESKey::Create();
  if (aes_key.is_error()) {
    return std::move(aes_key.error());
  }

  return AudioStream{Stream{index,
                            Stream::Type::kAudioSource,
                            config.channels,
                            CodecToString(config.codec),
                            GetPayloadType(config.codec),
                            GenerateSsrc(true /*high_priority*/),
                            kTargetDelay,
                            aes_key.value().key(),
                            aes_key.value().iv(),
                            false /* receiver_rtcp_event_log */,
                            {} /* receiver_rtcp_dscp */,
                            kRtpTimebase},
                     config.bit_rate};
}

Resolution ToResolution(const DisplayResolution& display_resolution) {
  return Resolution{display_resolution.width, display_resolution.height};
}

ErrorOr<VideoStream> CreateStream(int index, const VideoConfig& config) {
  ErrorOr<AESKey> aes_key = AESKey::Create();
  if (aes_key.is_error()) {
    return std::move(aes_key.error());
  }

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
             kTargetDelay,
             aes_key.value().key(),
             aes_key.value().iv(),
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
std::vector<S> CreateStreamList(int offset_index,
                                const std::vector<C>& configs) {
  std::vector<S> streams;
  streams.reserve(configs.size());

  for (size_t i = 0; i < configs.size(); ++i) {
    auto eov = CreateStream(i + offset_index, configs[i]);
    OSP_DCHECK(eov.is_value());
    if (eov.is_value()) {
      streams.push_back(std::move(eov.value()));
    }
  }
  return streams;
}

Offer CreateOffer(const std::vector<AudioConfig>& audio_configs,
                  const std::vector<VideoConfig>& video_configs) {
  return Offer{
      {CastMode::Type::kMirroring},
      false /* supports_wifi_status_reporting */,
      CreateStreamList<AudioStream>(0, audio_configs),
      CreateStreamList<VideoStream>(audio_configs.size(), video_configs)};
}

std::string GenerateSenderId() {
  int16_t id = 0;
  OSP_DCHECK(RAND_pseudo_bytes(reinterpret_cast<uint8_t*>(&id), 2) != -1);

  return "sender-" + std::to_string(id);
}

bool IsValidAudioConfig(const AudioConfig& config) {
  return config.channels >= 1 && config.bit_rate > 0;
}

bool IsValidResolution(const DisplayResolution& resolution) {
  static constexpr int kMinDimension = 240;
  return resolution.width > kMinDimension && resolution.height > kMinDimension;
}

bool IsValidVideoConfig(const VideoConfig& config) {
  return config.max_frame_rate.numerator > 0 &&
         config.max_frame_rate.denominator > 0 && config.max_bit_rate > 0 &&
         !config.resolutions.empty() &&
         std::all_of(config.resolutions.begin(), config.resolutions.end(),
                     IsValidResolution);
}

bool IsValid(const std::vector<AudioConfig>& audio_configs,
             const std::vector<VideoConfig>& video_configs) {
  return std::all_of(audio_configs.begin(), audio_configs.end(),
                     IsValidAudioConfig) &&
         std::all_of(video_configs.begin(), video_configs.end(),
                     IsValidVideoConfig);
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

void SenderSession::Negotiate(std::vector<AudioConfig> audio_configs,
                              std::vector<VideoConfig> video_configs) {
  // Each negotiation has a different sequence number.
  current_sequence_number_++;
  current_negotiation_.reset();

  // Negotiating with no streams doesn't make any sense.
  if (audio_configs.empty() && video_configs.empty()) {
    client_->OnError(
        this, Error(Error::Code::kParameterInvalid,
                    "Need at least one audio or video config to negotiate."));
    return;
  }
  if (!IsValid(audio_configs, video_configs)) {
    client_->OnError(this, Error(Error::Code::kParameterInvalid,
                                 "Invalid configs provided."));
    return;
  }

  Offer offer = CreateOffer(audio_configs, video_configs);
  ErrorOr<Json::Value> jsonified = offer.ToJson();
  if (jsonified.is_error()) {
    client_->OnError(this, std::move(jsonified.error()));
    return;
  }

  current_negotiation_ = std::make_unique<Negotiation>();
  current_negotiation_->offer = std::move(offer);
  for (size_t i = 0; i < audio_configs.size(); ++i) {
    current_negotiation_->audio_configs[i] = std::move(audio_configs[i]);
  }
  for (size_t i = 0; i < video_configs.size(); ++i) {
    current_negotiation_->video_configs[i] = std::move(video_configs[i]);
  }

  Json::Value message_body;
  message_body[kMessageKeyType] = kMessageTypeOffer;
  message_body[kOfferMessageBody] = std::move(jsonified.value());

  JsonMessage message;
  message.sender_id = sender_id_;
  message.message_namespace = kCastWebrtcNamespace;
  message.body = std::move(message_body);
  SendMessage(&message);
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
  OSP_DVLOG << "Received a message: " << message;

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

  std::string key;
  if (!json::ParseAndValidateString(message_json.value()[kKeyType], &key)) {
    OSP_DLOG_WARN << "Received message with invalid message key, dropping.";
    return;
  }

  if (key == kMessageTypeAnswer) {
    const Json::Value body =
        std::move(message_json.value()[kAnswerMessageBody]);
    if (body.isNull()) {
      client_->OnError(
          this, Error(Error::Code::kJsonParseError, "Failed to parse answer"));
      current_negotiation_.reset();
      OSP_DLOG_WARN
          << "Received message with invalid answer message body, dropping.";
      return;
    }
    OnAnswer(body);
  }
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
    current_negotiation_.reset();
    return;
  }

  ConfiguredSenders senders = SpawnSenders(answer);
  // If the answer message is invalid, there is no point in setting up a
  // negotiation because the sender won't be able to connect to it.
  client_->OnNegotiated(this, std::move(senders),
                        capture_recommendations::GetRecommendations(answer));
}

std::pair<SessionConfig, std::unique_ptr<Sender>> SenderSession::CreateSender(
    Ssrc receiver_ssrc,
    const Stream& stream,
    RtpPayloadType type) {
  SessionConfig config = {
      stream.ssrc,         receiver_ssrc,  kRtpTimebase,      stream.channels,
      stream.target_delay, stream.aes_key, stream.aes_iv_mask};

  auto sender =
      std::make_unique<Sender>(environment_, &packet_router_, config, type);

  return std::make_pair(std::move(config), std::move(sender));
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

    if (current_negotiation_->audio_configs.find(send_index) !=
        current_negotiation_->audio_configs.end()) {
      const AudioConfig& config =
          current_negotiation_->audio_configs[send_index];
      const RtpPayloadType payload_type = GetPayloadType(config.codec);
      for (const AudioStream& stream :
           current_negotiation_->offer.audio_streams) {
        if (stream.stream.index == send_index) {
          auto config_and_sender =
              CreateSender(receiver_ssrc, stream.stream, payload_type);
          current_audio_sender_ = std::move(config_and_sender.second);
          senders.audio = std::unique_ptr<ConfiguredSender<AudioConfig>>(
              new ConfiguredSender<AudioConfig>{
                  current_audio_sender_.get(),
                  std::move(config_and_sender.first), config});
        }
      }
    } else if (current_negotiation_->video_configs.find(send_index) !=
               current_negotiation_->video_configs.end()) {
      const VideoConfig& config =
          current_negotiation_->video_configs[send_index];
      const RtpPayloadType payload_type = GetPayloadType(config.codec);
      for (const VideoStream& stream :
           current_negotiation_->offer.video_streams) {
        if (stream.stream.index == send_index) {
          auto config_and_sender =
              CreateSender(receiver_ssrc, stream.stream, payload_type);
          current_video_sender_ = std::move(config_and_sender.second);
          senders.video = std::unique_ptr<ConfiguredSender<VideoConfig>>(
              new ConfiguredSender<VideoConfig>{
                  current_video_sender_.get(),
                  std::move(config_and_sender.first), config});
        }
      }
    }
  }
  return senders;
}

void SenderSession::SendMessage(JsonMessage* message) {
  // All messages have the sequence number embedded.
  // I probably have to keep the sequence number here?
  message->body[kSequenceNumber] = current_sequence_number_;

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
