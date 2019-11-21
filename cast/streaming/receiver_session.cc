// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <chrono>  // NOLINT
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer.h"
#include "util/logging.h"
#include "util/string.h"

using openscreen::HexToBytes;

namespace cast {
namespace streaming {

namespace {

const char kKeyNamespace[] = "namespace";
const char kKeySenderId[] = "senderId";
const char kKeyData[] = "data";

const char kOfferMessageType[] = "OFFER";
const char kOfferMessageBody[] = "offer";
const char kKeyType[] = "type";
const char kSequenceNumber[] = "seqNum";

std::string GetCodecName(ReceiverSession::AudioCodec codec) {
  switch (codec) {
    case ReceiverSession::AudioCodec::kAac:
      return "aac_ld";
    case ReceiverSession::AudioCodec::kOpus:
      return "opus";
  }
  OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
}

std::string GetCodecName(ReceiverSession::VideoCodec codec) {
  switch (codec) {
    case ReceiverSession::VideoCodec::kH264:
      return "h264";
    case ReceiverSession::VideoCodec::kVp8:
      return "vp8";
    case ReceiverSession::VideoCodec::kHevc:
      return "hevc";
    case ReceiverSession::VideoCodec::kVp9:
      return "vp9";
  }

  OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
}

template <typename Stream, typename Codec>
const Stream* SelectStream(const std::vector<Codec>& preferred_codecs,
                           const std::vector<Stream>& offered_streams) {
  for (Codec codec : preferred_codecs) {
    const std::string codec_name = GetCodecName(codec);
    for (const Stream& offered_stream : offered_streams) {
      if (offered_stream.codec_name() == codec_name) {
        OSP_VLOG << "Selected " << codec_name << " as codec for streaming.";
        return &offered_stream;
      }
    }
  }
  return nullptr;
}

int ParseRtpTimebase(absl::string_view time_base) {
  int rtp_timebase = 0;
  const char kTimeBasePrefix[] = "1/";
  if (!absl::StartsWith(time_base, kTimeBasePrefix) ||
      !absl::SimpleAtoi(time_base.substr(strlen(kTimeBasePrefix)),
                        &rtp_timebase) ||
      (rtp_timebase <= 0)) {
    OSP_LOG_WARN << "Could not parse RTP time value: '" << time_base << "'.";
    return -1;
  }

  return rtp_timebase;
}

// Currently, the SessionConfig is very similar between Audio and
// Video streams, even though the streams themselves expose many different
// fields
openscreen::ErrorOr<SessionConfig> ParseSessionConfig(const Stream& stream) {
  constexpr int kAesKeySize = 16;
  std::array<uint8_t, kAesKeySize> aes_key{};
  if (!HexToBytes(stream.aes_key(), aes_key.data(), aes_key.size()).ok()) {
    return openscreen::Error::Code::kParseError;
  }
  std::array<uint8_t, kAesKeySize> aes_iv_mask{};
  if (!HexToBytes(stream.aes_iv_mask(), aes_iv_mask.data(), aes_iv_mask.size())
           .ok()) {
    return openscreen::Error::Code::kParseError;
  }

  return SessionConfig{stream.ssrc(),
                       stream.ssrc() + 1,
                       ParseRtpTimebase(stream.time_base()),
                       1,
                       std::move(aes_key),
                       std::move(aes_iv_mask)};
}

}  // namespace

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    std::unique_ptr<Receiver> audio_receiver,
    absl::optional<SessionConfig> audio_receiver_config,
    std::unique_ptr<Receiver> video_receiver,
    absl::optional<SessionConfig> video_receiver_config)
    : audio_receiver_(std::move(audio_receiver)),
      audio_receiver_config_(std::move(audio_receiver_config)),
      video_receiver_(std::move(video_receiver)),
      video_receiver_config_(std::move(video_receiver_config)) {}

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    ConfiguredReceivers&&) noexcept = default;
ReceiverSession::ConfiguredReceivers& ReceiverSession::ConfiguredReceivers::
operator=(ConfiguredReceivers&&) noexcept = default;
ReceiverSession::ConfiguredReceivers::~ConfiguredReceivers() = default;

ReceiverSession::ReceiverSession(Client* client,
                                 std::unique_ptr<MessagePort> message_port,
                                 std::unique_ptr<Environment> environment,
                                 Preferences preferences)
    : client_(client),
      message_port_(std::move(message_port)),
      environment_(std::move(environment)),
      preferences_(preferences),
      packet_router_(
          std::make_unique<ReceiverPacketRouter>(environment_.get())) {
  OSP_DCHECK(client_);
  OSP_DCHECK(message_port_);
  OSP_DCHECK(environment_);

  message_port_->SetClient(this);
}

ReceiverSession::ReceiverSession(ReceiverSession&&) noexcept = default;
ReceiverSession& ReceiverSession::operator=(ReceiverSession&&) = default;
ReceiverSession::~ReceiverSession() {
  message_port_->SetClient(nullptr);
}

void ReceiverSession::SelectStreams(const AudioStream* audio,
                                    const VideoStream* video,
                                    Offer&& offer) {
  std::unique_ptr<Receiver> audio_receiver;
  absl::optional<SessionConfig> audio_config;
  if (audio) {
    auto error_or_config = ParseSessionConfig(*audio);
    if (!error_or_config) {
      client_->OnError(this, error_or_config.error());
      OSP_LOG_WARN << "Could not parse audio configuration from stream: "
                   << error_or_config.error();
      return;
    }
    audio_config = error_or_config.value();

    // Audio streams have an extra field.
    audio_config.value().channels = audio->channels();
    std::chrono::milliseconds initial_delay(audio->target_delay());
    audio_receiver =
        std::make_unique<Receiver>(environment_.get(), packet_router_.get(),
                                   audio_config.value(), initial_delay);
  }

  std::unique_ptr<Receiver> video_receiver;
  absl::optional<SessionConfig> video_config;
  if (video) {
    auto error_or_config = ParseSessionConfig(*video);
    if (!error_or_config) {
      client_->OnError(this, error_or_config.error());
      OSP_LOG_WARN << "Could not parse video configuration from stream: "
                   << error_or_config.error();
      return;
    }

    video_config = error_or_config.value();
    std::chrono::milliseconds initial_delay(audio->target_delay());
    video_receiver =
        std::make_unique<Receiver>(environment_.get(), packet_router_.get(),
                                   video_config.value(), initial_delay);
  }

  ConfiguredReceivers receivers(
      std::move(audio_receiver), std::move(audio_config),
      std::move(video_receiver), std::move(video_config));

  // TODO(jophba): implement Answer messaging.
  client_->OnNegotiated(this, std::move(receivers));
}

void ReceiverSession::OnMessage(absl::string_view message) {
  openscreen::ErrorOr<Json::Value> message_json = json_reader_.Read(message);

  if (!message_json) {
    client_->OnError(this, openscreen::Error::Code::kJsonParseError);
    OSP_LOG_WARN << "Received an invalid message: " << message;
    return;
  }

  // TODO(jophba): add sender connected/disconnected messaging.
  const std::string sender_id = message_json.value()[kKeySenderId].asString();
  const std::string ns = message_json.value()[kKeyNamespace].asString();
  const Json::Value payload = message_json.value()[kKeyData];

  const int sequence_number = payload[kSequenceNumber].asInt();
  const std::string key_type = payload[kKeyType].asString();
  if (key_type == kOfferMessageType) {
    OnOffer(std::move(payload[kOfferMessageBody]), sequence_number);
  } else {
    OSP_LOG_WARN << "Received message of invalid type: " << key_type;
  }
}

void ReceiverSession::OnError(openscreen::Error error) {
  OSP_LOG_WARN << "ReceiverSession's MessagePump encountered an error:"
               << error;
}

// Old Way:
// OnOffer -> ParseOffer -> ParseStream -> ParseVideoStream/ParseAudioStream
// in Parse*Stream, check CastCodecUtil::isCodecSupported
void ReceiverSession::OnOffer(Json::Value root, int sequence_number) {
  openscreen::ErrorOr<Offer> offer = Offer::Parse(std::move(root));
  if (!offer) {
    client_->OnError(this, offer.error());
    OSP_LOG_WARN << "Could not parse offer" << offer.error();
    return;
  }

  const AudioStream* selected_audio_stream = nullptr;
  if (!offer.value().audio_streams().empty() &&
      !preferences_.audio_codecs.empty()) {
    selected_audio_stream =
        SelectStream(preferences_.audio_codecs, offer.value().audio_streams());
  }

  const VideoStream* selected_video_stream = nullptr;
  if (!offer.value().video_streams().empty() &&
      !preferences_.video_codecs.empty()) {
    selected_video_stream =
        SelectStream(preferences_.video_codecs, offer.value().video_streams());
  }

  SelectStreams(selected_audio_stream, selected_video_stream,
                std::move(offer.value()));
}

}  // namespace streaming
}  // namespace cast
