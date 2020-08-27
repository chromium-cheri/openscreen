// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_session.h"

#include <chrono>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender.h"
#include "util/json/json_helpers.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

// Using statements for constructor readability.
using ConfiguredSenders = SenderSession::ConfiguredSenders;

namespace {

std::string CodecToString(SenderSession::AudioCodec codec) {
  switch (codec) {
    case SenderSession::AudioCodec::kAac:
      return "aac";
    case SenderSession::AudioCodec::kOpus:
      return "opus";
    default:
      OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
      return {};
  }
}

std::string CodecToString(SenderSession::VideoCodec codec) {
  switch (codec) {
    case SenderSession::VideoCodec::kH264:
      return "h264";
    case SenderSession::VideoCodec::kVp8:
      return "vp8";
    case SenderSession::VideoCodec::kHevc:
      return "hevc";
    case SenderSession::VideoCodec::kVp9:
      return "vp9";
    default:
      OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
      return {};
  }
}

template <typename Stream, typename Codec>
const Stream* SelectStream(const std::vector<Codec>& preferred_codecs,
                           const std::vector<Stream>& offered_streams) {
  for (auto codec : preferred_codecs) {
    const std::string codec_name = CodecToString(codec);
    for (const Stream& offered_stream : offered_streams) {
      if (offered_stream.stream.codec_name == codec_name) {
        OSP_DVLOG << "Selected " << codec_name << " as codec for streaming";
        return &offered_stream;
      }
    }
  }
  return nullptr;
}

// Helper method that creates an invalid Answer response.
Json::Value CreateInvalidAnswerMessage(Error error) {
  Json::Value message_root;
  message_root[kMessageKeyType] = kMessageTypeAnswer;
  message_root[kResult] = kResultError;
  message_root[kErrorMessageBody][kErrorCode] = static_cast<int>(error.code());
  message_root[kErrorMessageBody][kErrorDescription] = error.message();

  return message_root;
}

// Helper method that creates a valid Answer response.
Json::Value CreateAnswerMessage(const Answer& answer) {
  OSP_DCHECK(answer.IsValid());
  Json::Value message_root;
  message_root[kMessageKeyType] = kMessageTypeAnswer;
  message_root[kAnswerMessageBody] = answer.ToJson();
  message_root[kResult] = kResultOk;
  return message_root;
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
  ResetSenders(Client::kEndOfSession);
  message_port_->SetClient(nullptr);
}

// TODO(jophba): refactor when refactoring cast socket message port,
// lots of duplicate code in receiver_session.cc.
void SenderSession::OnMessage(absl::string_view sender_id,
                                absl::string_view message_namespace,
                                absl::string_view message) {
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

  JsonMessage parsed_message{sender_id.data(), message_namespace.data(),
                             sequence_number};
  if (key == kMessageTypeAnswer) {
    parsed_message.body = std::move(message_json.value()[kAnswerMessageBody]);
    if (parsed_message.body.isNull()) {
      client_->OnError(this, Error(Error::Code::kJsonParseError,
                                   "Received answer missing answer body"));
      OSP_DLOG_WARN << "Invalid message offer body";
      return;
    }
    OnAnswer(&parsed_message);
  }
}

void SenderSession::OnError(Error error) {
  OSP_DLOG_WARN << "SenderSession message port error: " << error;
}

void SenderSession::OnAnswer(Json::Value message_body) {
  ErrorOr<Answer> answer = Answer::Parse(std::move(body));
  if (!answer) {
    client_->OnError(this, answer.error());
    OSP_DLOG_WARN << "Could not parse answer" << answer.error();
    return;
  }

  // TODO: check if answer is invalid.
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

  ConfiguredSenders senders =
      SpawnSenders(selected_audio_stream, selected_video_stream);
  // If the answer message is invalid, there is no point in setting up a
  // negotiation because the sender won't be able to connect to it.
  client_->OnNegotiated(this, std::move(senders));
}

std::pair<SessionConfig, std::unique_ptr<Sender>>
SenderSession::ConstructSender(const Stream& stream) {
  SessionConfig config = {stream.ssrc,         stream.ssrc + 1,
                          stream.rtp_timebase, stream.channels,
                          stream.target_delay, stream.aes_key,
                          stream.aes_iv_mask};
  auto sender =
      std::make_unique<Sender>(environment_, &packet_router_, config);

  return std::make_pair(std::move(config), std::move(sender));
}

ConfiguredSenders SenderSession::SpawnSenders(const AudioStream* audio,
                                              const VideoStream* video) {
  OSP_DCHECK(audio || video);
  ResetSenders(Client::kRenegotiated);

  absl::optional<ConfiguredSender<AudioStream>> audio_sender;
  absl::optional<ConfiguredSender<VideoStream>> video_sender;

  if (audio) {
    auto audio_pair = ConstructSender(audio->stream);
    current_audio_sender_ = std::move(audio_pair.second);
    audio_sender.emplace(ConfiguredSender<AudioStream>{
        current_audio_sender_.get(), std::move(audio_pair.first), *audio});
  }

  if (video) {
    auto video_pair = ConstructSender(video->stream);
    current_video_sender_ = std::move(video_pair.second);
    video_sender.emplace(ConfiguredSender<VideoStream>{
        current_video_sender_.get(), std::move(video_pair.first), *video});
  }

  return ConfiguredSenders{std::move(audio_sender),
                             std::move(video_sender)};
}

Offer SenderSession::ConstructOffer(const Negotiation& negotiation)
{}

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
