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

namespace {

void ReplyWithTimeout(
    int sequence_number,
    std::vector<std::pair<int, SenderSessionMessager::ReplyCallback>>*
        replies) {
  auto it = replies->begin();
  for (; it != replies->end(); ++it) {
    if (it->first == sequence_number) {
      it->second(ReceiverMessage{});
    }
  }
  OSP_DCHECK(it != replies->end());
  replies->erase(it);
}

}  // namespace

SessionMessager::SessionMessager(MessagePort* message_port,
                                 std::string source_id,
                                 std::string destination_id,
                                 ErrorCallback cb)
    : message_port_(message_port),
      error_callback_(std::move(cb)),
      destination_id_(destination_id) {
  OSP_DCHECK(message_port_);
  OSP_DCHECK(!destination_id_.empty());
  OSP_DCHECK(!source_id.empty());
  message_port_->SetClient(this, source_id);
}

SessionMessager::~SessionMessager() {
  message_port_->ResetClient();
}

// void SessionMessager::OnMessage(const std::string& source_id,
//                                 const std::string& message_namespace,
//                                 const std::string& message) {
//   ErrorOr<Json::Value> message_json = json::Parse(message);
//   if (message_namespace != kCastWebrtcNamespace) {
//     OSP_DLOG_WARN << "Received message with unknown namespace: "
//                   << message_namespace;
//     return;
//   }

//   if (!message_json) {
//     OSP_DLOG_WARN << "Received an invalid message: " << message;
//     error_callback_(Error::Code::kJsonParseError);
//     return;
//   }
//   OSP_DVLOG << "Received a message: " << message;

//   int sequence_number;
//   if (!json::ParseAndValidateInt(message_json.value()[kSequenceNumber],
//                                  &sequence_number)) {
//     OSP_DLOG_WARN << "Invalid message sequence number";
//     return;
//   }

//   std::string type;
//   if (!json::ParseAndValidateString(message_json.value()[kMessageType],
//                                     &type)) {
//     OSP_DLOG_WARN << "Invalid message key";
//     return;
//   }

//   // Not all messages have results, but if they do the result must be OK.
//   std::string result;
//   if (!json::ParseAndValidateString(message_json.value()[kResult], &result))
//   {
//     result.clear();
//   }

//   // TODO: whatttt
//   // for (const auto& pair : callbacks_) {
//   //   if (pair.first == type) {
//   //     // Currently all body keys are the lowercase version of the message
//   //     type
//   //     // key. This may need to be refactored if this is no longer the
//   case.
//   //     absl::AsciiStrToLower(&type);
//   //     Json::Value body;
//   //     if (result.empty() || result == kResultOk) {
//   //       body = message_json.value()[type];
//   //     } else {
//   //       body = message_json.value()[kErrorMessageBody];
//   //     }
//   //     pair.second(Message{source_id.data(), message_namespace.data(),
//   //                         sequence_number, std::move(body)});
//   //     return;
//   //   }
//   // }
// }

Error SessionMessager::SendMessage(const std::string& namespace_,
                                   const Json::Value& message_root) {
  auto body_or_error = json::Stringify(message_root);
  if (body_or_error.is_error()) {
    return std::move(body_or_error.error());
  }
  OSP_DVLOG << "Sending message: DESTINATION[" << destination_id_
            << "], NAMESPACE[" << namespace_ << "], BODY:\n"
            << body_or_error.value();
  message_port_->PostMessage(destination_id_, namespace_,
                             body_or_error.value());
  return Error::None();
}

SenderSessionMessager::SenderSessionMessager(MessagePort* message_port,
                                             std::string source_id,
                                             std::string destination_id,
                                             ErrorCallback cb,
                                             TaskRunner* task_runner)
    : SessionMessager(message_port,
                      std::move(source_id),
                      std::move(destination_id),
                      std::move(cb)),
      task_runner_(task_runner) {}

Error SenderSessionMessager::SendRequest(SenderMessage message,
                                         ReceiverMessage::Type reply_type,
                                         absl::optional<ReplyCallback> cb) {
  static constexpr std::chrono::milliseconds kReplyTimeout{4000};
  const auto namespace_ = (message.type == SenderMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  ErrorOr<Json::Value> jsonified = message.ToJson();
  if (jsonified.is_error()) {
    return std::move(jsonified.error());
  }
  SessionMessager::SendMessage(namespace_, jsonified.value());

  if (cb) {
    awaiting_replies_.emplace_back(message.sequence_number,
                                   std::move(cb.value()));
    task_runner_->PostTaskWithDelay(
        [self = weak_factory_.GetWeakPtr(), seq_num = message.sequence_number] {
          if (self) {
            ReplyWithTimeout(seq_num, &self->awaiting_replies_);
          }
        },
        kReplyTimeout);
  }

  return Error::None();
}

void SenderSessionMessager::OnMessage(const std::string& source_id,
                                      const std::string& message_namespace,
                                      const std::string& message) {}

void SenderSessionMessager::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messager: " << error;
}

ReceiverSessionMessager::ReceiverSessionMessager(MessagePort* message_port,
                                                 std::string source_id,
                                                 std::string destination_id,
                                                 ErrorCallback cb)
    : SessionMessager(message_port,
                      std::move(source_id),
                      std::move(destination_id),
                      std::move(cb)) {}

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
  const auto namespace_ = (message.type == ReceiverMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  return SessionMessager::SendMessage(namespace_, message.ToJson());
}

void ReceiverSessionMessager::OnMessage(const std::string& source_id,
                                        const std::string& message_namespace,
                                        const std::string& message) {}

void ReceiverSessionMessager::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messager: " << error;
}

}  // namespace cast
}  // namespace openscreen
