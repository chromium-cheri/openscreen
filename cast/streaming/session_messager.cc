// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_messager.h"

#include "absl/strings/ascii.h"
#include "cast/common/public/message_port.h"
#include "cast/streaming/message_fields.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

SessionMessager::SessionMessager(MessagePort* message_port,
                                 std::string source_id,
                                 ErrorCallback cb)
    : message_port_(message_port), error_callback_(std::move(cb)) {
  OSP_DCHECK(message_port_);
  message_port_->SetClient(this, source_id);
}

SessionMessager::~SessionMessager() {
  message_port_->ResetClient();
}

void SessionMessager::OnMessage(const std::string& source_id,
                                const std::string& message_namespace,
                                const std::string& message) {
  ErrorOr<Json::Value> message_json = json::Parse(message);
  if (message_namespace != kCastWebrtcNamespace) {
    OSP_DLOG_WARN << "Received message with unknown namespace: "
                  << message_namespace;
    return;
  }

  if (!message_json) {
    OSP_DLOG_WARN << "Received an invalid message: " << message;
    error_callback_(Error::Code::kJsonParseError);
    return;
  }
  OSP_DVLOG << "Received a message: " << message;

  int sequence_number;
  if (!json::ParseAndValidateInt(message_json.value()[kSequenceNumber],
                                 &sequence_number)) {
    OSP_DLOG_WARN << "Invalid message sequence number";
    return;
  }

  std::string type;
  if (!json::ParseAndValidateString(message_json.value()[kMessageType],
                                    &type)) {
    OSP_DLOG_WARN << "Invalid message key";
    return;
  }

  // Not all messages have results, but if they do the result must be OK.
  std::string result;
  if (!json::ParseAndValidateString(message_json.value()[kResult], &result)) {
    result.clear();
  }

  // TODO: whatttt
  // for (const auto& pair : callbacks_) {
  //   if (pair.first == type) {
  //     // Currently all body keys are the lowercase version of the message
  //     type
  //     // key. This may need to be refactored if this is no longer the case.
  //     absl::AsciiStrToLower(&type);
  //     Json::Value body;
  //     if (result.empty() || result == kResultOk) {
  //       body = message_json.value()[type];
  //     } else {
  //       body = message_json.value()[kErrorMessageBody];
  //     }
  //     pair.second(Message{source_id.data(), message_namespace.data(),
  //                         sequence_number, std::move(body)});
  //     return;
  //   }
  // }
}

void SessionMessager::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messager: " << error;
}

SenderSessionMessager::SenderSessionMessager(MessagePort* message_port,
                                             std::string source_id,
                                             ErrorCallback cb)
    : SessionMessager(message_port, std::move(source_id), std::move(cb)) {}

void SenderSessionMessager::SendRequest(SenderMessage message,
                                        ReceiverMessage::Type reply_type,
                                        absl::optional<ReplyCallback> cb) {}

ReceiverSessionMessager::ReceiverSessionMessager(MessagePort* message_port,
                                                 std::string source_id,
                                                 ErrorCallback cb)
    : SessionMessager(message_port, std::move(source_id), std::move(cb)) {}

void ReceiverSessionMessager::SetHandler(SenderMessage::Type type,
                                         RequestCallback cb) {
  OSP_DCHECK(std::none_of(
      callbacks_.begin(), callbacks_.end(),
      [type](std::pair<SenderMessage::Type, RequestCallback> pair) {
        return pair.first == type;
      }));

  callbacks_.emplace_back(type, std::move(cb));
}

Error ReceiverSessionMessager::SendMessage(ReceiverMessage message) {
  if (sender_id_.empty()) {
    return Error(Error::Code::kInvalidConnectionState,
                 "Cannot send a message without first receiving one");
  }
  const auto namespace_ = (message.type == ReceiverMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  auto body_or_error = json::Stringify(message.ToJson());
  if (body_or_error.is_error()) {
    return std::move(body_or_error.error());
  }
  OSP_DVLOG << "Sending message: DESTINATION[" << sender_id_ << "], NAMESPACE["
            << namespace_ << "], BODY:\n"
            << body_or_error.value();
  message_port_->PostMessage(sender_id_, namespace_, body_or_error.value());
  return Error::None();
}

}  // namespace cast
}  // namespace openscreen
