// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_session.h"

#include <chrono>
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
#include "util/json/json_helpers.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

namespace {

// Using statements for constructor readability.
using ConfiguredSenders = SenderSession::ConfiguredSenders;

// TODO: put in android TV hacks
RtpPayloadType GetPayloadType(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAac:
      return RtpPayloadType::kAudioAac;
    case AudioCodec::kOpus:
      return RtpPayloadType::kAudioOpus;
  }
  OSP_NOTREACHED();
  return RtpPayloadType::kNull;
}

// TODO: different payload type for VP9? I don't see it anywhere.
RtpPayloadType GetPayloadType(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kHevc:
    case VideoCodec::kH264:
      return RtpPayloadType::kVideoH264;
    case VideoCodec::kVp9:
    case VideoCodec::kVp8:
      return RtpPayloadType::kVideoVp8;
  }
  OSP_NOTREACHED();
  return RtpPayloadType::kNull;
}

}  // namespace

SenderSession::Client::~Client() = default;

SenderSession::SenderSession(Client* const client,
                             Environment* environment,
                             MessagePort* message_port)
    : client_(client),
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

void SenderSession::Negotiate(std::vector<AudioConfig> audio_offers,
                              std::vector<VideoConfig> video_offers) {}

void SenderSession::OnMessage(const std::string& sender_id,
                              const std::string& message_namespace,
                              const std::string& message) {
  ErrorOr<Json::Value> message_json = json::Parse(message);

  if (!message_json) {
    client_->OnError(this, Error::Code::kJsonParseError);
    OSP_DLOG_WARN << "Received an invalid message: " << message;
    return;
  }
  OSP_DVLOG << "Received a message: " << message;

  int sequence_number;
  if (!json::ParseAndValidateInt(message_json.value()[kSequenceNumber],
                                 &sequence_number)) {
    OSP_DLOG_WARN << "Invalid message sequence number";
    return;
  }

  std::string key;
  if (!json::ParseAndValidateString(message_json.value()[kKeyType], &key)) {
    OSP_DLOG_WARN << "Invalid message key";
    return;
  }

  if (key == kMessageTypeAnswer) {
    const Json::Value body =
        std::move(message_json.value()[kAnswerMessageBody]);
    if (body.isNull()) {
      client_->OnError(this, Error(Error::Code::kJsonParseError,
                                   "Received answer missing answer body"));
      OSP_DLOG_WARN << "Invalid message offer body";
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
    OSP_DLOG_WARN << "Could not parse answer message";
    return;
  }

  ConfiguredSenders senders = SpawnSenders(answer);
  // If the answer message is invalid, there is no point in setting up a
  // negotiation because the sender won't be able to connect to it.
  client_->OnNegotiated(this, std::move(senders),
                        capture_recommendations::GetRecommendations(answer));
}

std::pair<SessionConfig, std::unique_ptr<Sender>>
SenderSession::ConstructSender(const Stream& stream, RtpPayloadType type) {
  SessionConfig config = {stream.ssrc,         stream.ssrc + 1,
                          stream.rtp_timebase, stream.channels,
                          stream.target_delay, stream.aes_key,
                          stream.aes_iv_mask};

  auto sender =
      std::make_unique<Sender>(environment_, &packet_router_, config, type);

  return std::make_pair(std::move(config), std::move(sender));
}

ConfiguredSenders SenderSession::SpawnSenders(const Answer& answer) {
  OSP_DCHECK(current_negotiation_);

  ConfiguredSenders senders;
  // TODO: what do I do with the UDP port? Looks like we don't
  // have UDP ports set up anywhere??????????????????
  for (size_t i = 0; i < answer.send_indexes.size(); ++i) {
    // TODO: SSRC belongs to the packet router I guess???
    // const Ssrc receiver_ssrc = answer.ssrcs[i];
    const int send_index = answer.send_indexes[i];

    // TODO: lower audio + video duplication?
    if (current_negotiation_->audio_configs.find(send_index) !=
        current_negotiation_->audio_configs.end()) {
      const AudioConfig& config =
          current_negotiation_->audio_configs[send_index];
      const RtpPayloadType payload_type = GetPayloadType(config.codec);
      for (const AudioStream& stream :
           current_negotiation_->offer.audio_streams) {
        if (stream.stream.index == send_index) {
          auto config_and_sender = ConstructSender(stream.stream, payload_type);
          current_audio_sender_ = std::move(config_and_sender.second);
          senders.audio = std::unique_ptr<AudioSender>(
              new AudioSender{current_audio_sender_.get(),
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
          auto config_and_sender = ConstructSender(stream.stream, payload_type);
          current_video_sender_ = std::move(config_and_sender.second);
          senders.video = std::unique_ptr<VideoSender>(
              new VideoSender{current_video_sender_.get(),
                              std::move(config_and_sender.first), config});
        }
      }
    }
  }
  return senders;
}

// TODO: build an offer
// TODO: copy from unit testing? AES key generation, e.g.?
Offer SenderSession::ConstructOffer(
    const std::vector<AudioConfig>& audio_configs,
    const std::vector<VideoConfig>& video_configs) {
  return Offer{};
}

void SenderSession::SendMessage(JsonMessage* message) {
  // All messages have the sequence number embedded.
  message->body[kSequenceNumber] = message->sequence_number;

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
