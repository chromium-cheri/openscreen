// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <chrono>  // NOLINT
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer_messages.h"
#include "util/json/json_writer.h"
#include "util/logging.h"

namespace cast {
namespace streaming {

// Using statements for constructor readability.
using Preferences = ReceiverSession::Preferences;
using ConfiguredReceivers = ReceiverSession::ConfiguredReceivers;

namespace {
// List of message namespaces.
const char kMirroringNamespace[] = "urn:x-cast:com.google.cast.webrtc";

// The key for type of message, e.g. OFFER or ANSWER.
const char kMessageKeyType[] = "type";

// List of OFFER message fields.
const char kOfferMessageType[] = "OFFER";
const char kOfferMessageBody[] = "offer";
const char kKeyType[] = "type";
const char kSequenceNumber[] = "seqNum";

// List of ANSWER message fields.
const char kAnswerMessageType[] = "ANSWER";
const char kAnswerMessageBody[] = "answer";
const char kResult[] = "result";
const char kResultOk[] = "ok";
const char kResultError[] = "error";
const char kErrorMessageBody[] = "error";
const char kErrorCode[] = "code";
const char kErrorDescription[] = "description";

std::string GetCodecName(ReceiverSession::AudioCodec codec) {
  switch (codec) {
    case ReceiverSession::AudioCodec::kAac:
      return "aac_ld";
    case ReceiverSession::AudioCodec::kOpus:
      return "opus";
  }

  OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
  return {};
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
  return {};
}

template <typename Stream, typename Codec>
const Stream* SelectStream(const std::vector<Codec>& preferred_codecs,
                           const std::vector<Stream>& offered_streams) {
  for (Codec codec : preferred_codecs) {
    const std::string codec_name = GetCodecName(codec);
    for (const Stream& offered_stream : offered_streams) {
      if (offered_stream.stream.codec_name == codec_name) {
        OSP_VLOG << "Selected " << codec_name << " as codec for streaming.";
        return &offered_stream;
      }
    }
  }
  return nullptr;
}

// Currently, the SessionConfig is very similar between Audio and
// Video streams, even though the streams themselves expose many different
// fields
openscreen::ErrorOr<SessionConfig> ParseSessionConfig(const Stream& stream,
                                                      int channels) {
  return SessionConfig{stream.ssrc, stream.ssrc + 1, stream.rtp_timebase,
                       channels,    stream.aes_key,  stream.aes_iv_mask};
}

// The answer object's Json parsing is focused on the valid use case. The
// spec also outlines an error condition, which is captured here.
Json::Value ToJsonMessageBody(const Answer& answer) {
  Json::Value message_root;
  message_root[kMessageKeyType] = kAnswerMessageType;
  openscreen::ErrorOr<Json::Value> answer_body = answer.ToJson();
  if (answer_body.is_value()) {
    message_root[kAnswerMessageBody] = answer_body.value();
    message_root[kResult] = kResultOk;
  } else {
    message_root[kResult] = kResultError;
    message_root[kErrorMessageBody][kErrorCode] =
        static_cast<int>(answer_body.error().code());
    message_root[kErrorMessageBody][kErrorDescription] =
        answer_body.error().message();
  }

  return message_root;
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

ConfiguredReceivers::ConfiguredReceivers(ConfiguredReceivers&&) noexcept =
    default;
ConfiguredReceivers& ConfiguredReceivers::operator=(
    ConfiguredReceivers&&) noexcept = default;
ConfiguredReceivers::~ConfiguredReceivers() = default;

Preferences::Preferences() = default;
Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs)
    : Preferences(video_codecs, audio_codecs, nullptr, nullptr) {}

Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs,
                         std::unique_ptr<Constraints> constraints,
                         std::unique_ptr<DisplayDescription> description)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)),
      constraints(std::move(constraints)),
      display_description(std::move(description)) {}

Preferences::Preferences(Preferences&&) noexcept = default;
Preferences& Preferences::operator=(Preferences&&) noexcept = default;

ReceiverSession::ReceiverSession(Client* const client,
                                 std::unique_ptr<Environment> environment,
                                 std::unique_ptr<MessagePort> message_port,
                                 Preferences preferences)
    : client_(client),
      environment_(std::move(environment)),
      message_port_(std::move(message_port)),
      preferences_(std::move(preferences)),
      packet_router_(environment_.get()) {
  OSP_DCHECK(client_);
  OSP_DCHECK(message_port_);
  OSP_DCHECK(environment_);

  message_port_->SetClient(this);
}

ReceiverSession::~ReceiverSession() {
  message_port_->SetClient(nullptr);
}

void ReceiverSession::OnMessage(absl::string_view sender_id,
                                absl::string_view namespace_,
                                absl::string_view message) {
  openscreen::ErrorOr<Json::Value> message_json = json_reader_.Read(message);

  if (!message_json) {
    client_->OnError(this, openscreen::Error::Code::kJsonParseError);
    OSP_LOG_WARN << "Received an invalid message: " << message;
    return;
  }

  // TODO(jophba): add sender connected/disconnected messaging.
  if (namespace_ != kMirroringNamespace) {
    return;
  }

  const int sequence_number = message_json.value()[kSequenceNumber].asInt();
  const MessageMetadata metadata{sender_id.data(), namespace_.data(),
                                 sequence_number};

  const std::string key_type = message_json.value()[kKeyType].asString();
  if (key_type == kOfferMessageType) {
    OnOffer(std::move(message_json.value()[kOfferMessageBody]), metadata);
  } else {
    OSP_LOG_WARN << "Received message of invalid type: " << key_type;
  }
}

void ReceiverSession::OnError(openscreen::Error error) {
  OSP_LOG_WARN << "ReceiverSession's MessagePump encountered an error:"
               << error;
}

void ReceiverSession::OnOffer(Json::Value root,
                              const MessageMetadata& metadata) {
  openscreen::ErrorOr<Offer> offer = Offer::Parse(std::move(root));
  if (!offer) {
    client_->OnError(this, offer.error());
    OSP_LOG_WARN << "Could not parse offer" << offer.error();
    return;
  }

  const AudioStream* selected_audio_stream = nullptr;
  if (!offer.value().audio_streams.empty() &&
      !preferences_.audio_codecs.empty()) {
    selected_audio_stream =
        SelectStream(preferences_.audio_codecs, offer.value().audio_streams);
  }

  const VideoStream* selected_video_stream = nullptr;
  if (!offer.value().video_streams.empty() &&
      !preferences_.video_codecs.empty()) {
    selected_video_stream =
        SelectStream(preferences_.video_codecs, offer.value().video_streams);
  }

  cast_mode_ = offer.value().cast_mode;
  supports_wifi_status_reporting_ =
      offer.value().supports_wifi_status_reporting;

  SendAnswer(metadata, selected_audio_stream, selected_video_stream);

  NegotiateReceivers(selected_audio_stream, selected_video_stream,
                     std::move(offer.value()));
}

void ReceiverSession::NegotiateReceivers(const AudioStream* audio,
                                         const VideoStream* video,
                                         Offer&& offer) {
  std::unique_ptr<Receiver> audio_receiver;
  absl::optional<SessionConfig> audio_config;
  if (audio) {
    auto error_or_config = ParseSessionConfig(audio->stream, audio->channels);
    if (!error_or_config) {
      client_->OnError(this, error_or_config.error());
      OSP_LOG_WARN << "Could not parse audio configuration from stream: "
                   << error_or_config.error();
      return;
    }
    audio_config = std::move(error_or_config.value());
    audio_receiver = std::make_unique<Receiver>(
        environment_.get(), &packet_router_, audio_config.value(),
        audio->stream.target_delay);
  }

  std::unique_ptr<Receiver> video_receiver;
  absl::optional<SessionConfig> video_config;
  if (video) {
    auto error_or_config = ParseSessionConfig(video->stream, 1 /* channels */);
    if (!error_or_config) {
      client_->OnError(this, error_or_config.error());
      OSP_LOG_WARN << "Could not parse video configuration from stream: "
                   << error_or_config.error();
      return;
    }
    video_config = std::move(error_or_config.value());
    video_receiver = std::make_unique<Receiver>(
        environment_.get(), &packet_router_, video_config.value(),
        audio->stream.target_delay);
  }

  ConfiguredReceivers receivers(
      std::move(audio_receiver), std::move(audio_config),
      std::move(video_receiver), std::move(video_config));
  client_->OnNegotiated(this, std::move(receivers));
}

// TODO(jophba): implement Answer messaging.
void ReceiverSession::SendAnswer(const MessageMetadata& metadata,
                                 const AudioStream* selected_audio_stream,
                                 const VideoStream* selected_video_stream) {
  std::vector<int> stream_indexes(2);
  std::vector<Ssrc> stream_ssrcs(2);
  if (selected_audio_stream) {
    stream_indexes.push_back(selected_audio_stream->stream.index);
    stream_ssrcs.push_back(selected_audio_stream->stream.ssrc + 1);
  }

  if (selected_video_stream) {
    stream_indexes.push_back(selected_video_stream->stream.index);
    stream_ssrcs.push_back(selected_video_stream->stream.ssrc + 1);
  }

  const Answer answer{
      cast_mode_, udp_port_, std::move(stream_indexes), std::move(stream_ssrcs),
      preferences_.constraints ? *preferences_.constraints : Constraints{},
      preferences_.display_description ? *preferences_.display_description
                                       : DisplayDescription{},
      std::vector<int>{},  // receiver_rtcp_event_log
      std::vector<int>{},  // receiver_rtcp_dscp
      supports_wifi_status_reporting_,
      // RTP extensions should be empty, but not null.
      std::vector<std::string>{}  // rtp_extensions
  };

  Json::Value message_body = ToJsonMessageBody(answer);
  SendMessage(metadata, std::move(message_body));
}

void ReceiverSession::SendMessage(const MessageMetadata& metadata,
                                  Json::Value body) {
  // All messages have the sequence number embedded.
  body[kSequenceNumber] = metadata.sequence_number;
  auto body_or_error = json_writer_.Write(body);
  if (body_or_error.is_value()) {
    message_port_->PostMessage(metadata.sender_id, metadata.namespace_,
                               body_or_error.value());
  } else {
    client_->OnError(this, body_or_error.error());
  }
}
}  // namespace streaming
}  // namespace cast
