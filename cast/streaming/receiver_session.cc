// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

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

const char kOfferMessageType[] = "OFFER";
const char kKeyType[] = "type";
const char kSequenceNumber[] = "seqNum";

std::string GetCodecName(ReceiverSession::AudioCodec codec) {
  switch (codec) {
    case ReceiverSession::AudioCodec::kAac:
      return "aac_ld";
    case ReceiverSession::AudioCodec::kOpus:
      return "opus";
  }
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
}

template <typename Stream, typename Codec>
const Stream* SelectStream(const std::vector<Codec>& preferred_codecs,
                           const std::vector<Stream>& offered_streams) {
  for (Codec codec : preferred_codecs) {
    const std::string codec_name = GetCodecName(codec);
    for (const Stream& offered_stream : offered_streams) {
      if (offered_stream.codec_name() == codec_name) {
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
    return -1;
  }

  return rtp_timebase;
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
                                 MessagePort* message_port,
                                 Preferences preferences)
    : client_(client), message_port_(message_port), preferences_(preferences) {
  OSP_DCHECK(client_);
  OSP_DCHECK(message_port_);
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
  // TODO: lower amount of duplicate code lines.
  std::unique_ptr<Receiver> audio_receiver;
  absl::optional<SessionConfig> audio_config;
  const int kAesKeySize = 16;
  if (audio) {
    std::array<uint8_t, kAesKeySize> aes_key{};
    if (!HexToBytes(audio->aes_key(), aes_key.data(), aes_key.size()).ok()) {
      return;
    }
    std::array<uint8_t, kAesKeySize> aes_iv_mask{};
    if (!HexToBytes(audio->aes_iv_mask(), aes_iv_mask.data(),
                    aes_iv_mask.size())
             .ok()) {
      return;
    }
    audio_config =
        SessionConfig{audio->ssrc(),
                      audio->ssrc(),  // TODO: what is the receiver SSRC?
                      ParseRtpTimebase(audio->time_base()),
                      audio->channels(),
                      std::move(aes_key),
                      std::move(aes_iv_mask)};
  }

  std::unique_ptr<Receiver> video_receiver;
  absl::optional<SessionConfig> video_config;
  if (video) {
    std::array<uint8_t, kAesKeySize> aes_key{};
    if (!HexToBytes(video->aes_key(), aes_key.data(), aes_key.size()).ok()) {
      return;
    }
    std::array<uint8_t, kAesKeySize> aes_iv_mask{};
    if (!HexToBytes(video->aes_iv_mask(), aes_iv_mask.data(),
                    aes_iv_mask.size())
             .ok()) {
      return;
    }
    video_config =
        SessionConfig{video->ssrc(),
                      video->ssrc(),  // TODO: what is the receiver SSRC?
                      ParseRtpTimebase(video->time_base()),
                      1,
                      std::move(aes_key),
                      std::move(aes_iv_mask)};
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
    return;
  }

  // TODO: need sender id?
  const int sequence_number = message_json.value()[kSequenceNumber].asInt();
  const std::string key_type = message_json.value()[kKeyType].asString();
  // Mirroring offer created by mirroring service:
  // void Session::CreateAndSendOffer()

  // Consumed by: CastChannelReceiver
  // https://cs.corp.google.com/eureka_internal/chromium/src/chromecast/internal/mirroring/cast_channel_receiver.cc?type=cs&g=0
  // CastChannelReceiver::OnMessage
  if (key_type == kOfferMessageType) {
    OnOffer(std::move(message_json.value()), sequence_number);
  }
}

void ReceiverSession::OnError(openscreen::Error error) {
  client_->OnError(this, error);
}

// Old Way:
// OnOffer -> ParseOffer -> ParseStream -> ParseVideoStream/ParseAudioStream
// in Parse*Stream, check CastCodecUtil::isCodecSupported
void ReceiverSession::OnOffer(Json::Value root, int sequence_number) {
  openscreen::ErrorOr<Offer> offer = Offer::Parse(std::move(root));
  if (!offer) {
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
