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

void ReplyIfTimedOut(
    int sequence_number,
    ReceiverMessage::Type reply_type,
    std::vector<std::pair<int, SenderSessionMessager::ReplyCallback>>*
        replies) {
  auto it = replies->begin();
  for (; it != replies->end(); ++it) {
    if (it->first == sequence_number) {
      OSP_DVLOG
          << "Replying with empty message due to timeout for sequence number: "
          << sequence_number;
      it->second(ReceiverMessage{reply_type, sequence_number});
      replies->erase(it);
      break;
    }
  }
}

}  // namespace

SessionMessager::SessionMessager(MessagePort* message_port,
                                 std::string source_id,
                                 ErrorCallback cb)
    : message_port_(message_port), error_callback_(std::move(cb)) {
  OSP_DCHECK(message_port_);
  OSP_DCHECK(!source_id.empty());
  message_port_->SetClient(this, source_id);
}

SessionMessager::~SessionMessager() {
  message_port_->ResetClient();
}

Error SessionMessager::SendMessage(const std::string& destination_id,
                                   const std::string& namespace_,
                                   const Json::Value& message_root) {
  auto body_or_error = json::Stringify(message_root);
  if (body_or_error.is_error()) {
    return std::move(body_or_error.error());
  }
  OSP_DVLOG << "Sending message: DESTINATION[" << destination_id
            << "], NAMESPACE[" << namespace_ << "], BODY:\n"
            << body_or_error.value();
  message_port_->PostMessage(destination_id, namespace_, body_or_error.value());
  return Error::None();
}

SenderSessionMessager::SenderSessionMessager(MessagePort* message_port,
                                             std::string source_id,
                                             std::string receiver_id,
                                             ErrorCallback cb,
                                             TaskRunner* task_runner)
    : SessionMessager(message_port, std::move(source_id), std::move(cb)),
      task_runner_(task_runner),
      receiver_id_(receiver_id) {}

void SenderSessionMessager::SetHandler(ReceiverMessage::Type type,
                                       ReplyCallback cb) {
  // Currently the only handler allowed is for RPC messages.
  OSP_DCHECK(type == ReceiverMessage::Type::kRpc);
  rpc_callback_ = std::move(cb);
}

Error SenderSessionMessager::SendOutboundMessage(SenderMessage message) {
  const auto namespace_ = (message.type == SenderMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  ErrorOr<Json::Value> jsonified = message.ToJson();
  if (jsonified.is_error()) {
    return std::move(jsonified.error());
  }
  return SessionMessager::SendMessage(receiver_id_, namespace_,
                                      jsonified.value());
}

Error SenderSessionMessager::SendRequest(SenderMessage message,
                                         ReceiverMessage::Type reply_type,
                                         ReplyCallback cb) {
  static constexpr std::chrono::milliseconds kReplyTimeout{4000};
  // RPC messages are not meant to be request/reply.
  OSP_DCHECK(reply_type != ReceiverMessage::Type::kRpc);

  SendOutboundMessage(message);
  awaiting_replies_.emplace_back(message.sequence_number, std::move(cb));
  task_runner_->PostTaskWithDelay(
      [self = weak_factory_.GetWeakPtr(), reply_type,
       seq_num = message.sequence_number] {
        if (self) {
          ReplyIfTimedOut(seq_num, reply_type, &self->awaiting_replies_);
        }
      },
      kReplyTimeout);

  return Error::None();
}

void SenderSessionMessager::OnMessage(const std::string& source_id,
                                      const std::string& message_namespace,
                                      const std::string& message) {
  if (source_id != receiver_id_) {
    OSP_DLOG_WARN << "Received message from unknown/incorrect sender, expected "
                     "id \""
                  << receiver_id_ << "\", got \"" << source_id << "\"";
    return;
  }

  if (message_namespace != kCastWebrtcNamespace &&
      message_namespace != kCastRemotingNamespace) {
    OSP_DLOG_WARN << "Received message from unknown namespace: "
                  << message_namespace;
    return;
  }

  ErrorOr<Json::Value> message_body = json::Parse(message);
  if (!message_body) {
    error_callback_(Error(Error::Code::kJsonParseError,
                          "Received an invalid message: " + message));
    return;
  }

  // We don't know if the sequence number is required yet, it may be
  // an RPC message.
  int sequence_number;
  if (!json::ParseAndValidateInt(message_body.value()[kSequenceNumber],
                                 &sequence_number)) {
    sequence_number = -1;
  }

  ErrorOr<ReceiverMessage> receiver_message =
      ReceiverMessage::Parse(message_body.value());
  if (receiver_message.is_error()) {
    OSP_DLOG_WARN << "Message was an invalid receiver message: "
                  << receiver_message.error();
  }

  if (sequence_number == -1) {
    OSP_DLOG_WARN << "Received message without sequence number";
    return;
  }

  if (receiver_message.value().type == ReceiverMessage::Type::kRpc) {
    if (rpc_callback_) {
      rpc_callback_(receiver_message.is_value() ? receiver_message.value()
                                                : ReceiverMessage{});
    }
  } else {
    auto it = awaiting_replies_.begin();
    for (; it != awaiting_replies_.end(); ++it) {
      if (it->first == sequence_number) {
        break;
      }
    }
    if (it == awaiting_replies_.end()) {
      OSP_DLOG_WARN << "Received a reply I wasn't waiting for: "
                    << sequence_number;
      return;
    }

    it->second(receiver_message.is_value() ? receiver_message.value()
                                           : ReceiverMessage{});
    awaiting_replies_.erase(it);
  }
}

void SenderSessionMessager::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messager: " << error;
}

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
  if (sender_session_id_.empty()) {
    return Error(Error::Code::kInitializationFailure,
                 "Tried to send a message without receving one first");
  }

  const auto namespace_ = (message.type == ReceiverMessage::Type::kRpc)
                              ? kCastRemotingNamespace
                              : kCastWebrtcNamespace;

  ErrorOr<Json::Value> message_json = message.ToJson();
  if (message_json.is_error()) {
    return std::move(message_json.error());
  }
  return SessionMessager::SendMessage(sender_session_id_, namespace_,
                                      message_json.value());
}

void ReceiverSessionMessager::OnMessage(const std::string& source_id,
                                        const std::string& message_namespace,
                                        const std::string& message) {
  // We assume we are connected to the first sender_id we receive.
  if (sender_session_id_.empty()) {
    sender_session_id_ = source_id;
  } else if (source_id != sender_session_id_) {
    OSP_DLOG_WARN << "Received message from unknown/incorrect sender, expected "
                     "id \""
                  << sender_session_id_ << "\", got \"" << source_id << "\"";
    return;
  }

  if (message_namespace != kCastWebrtcNamespace &&
      message_namespace != kCastRemotingNamespace) {
    OSP_DLOG_WARN << "Received message from unknown namespace: "
                  << message_namespace;
    return;
  }

  ErrorOr<Json::Value> message_body = json::Parse(message);
  if (message_body.is_error()) {
    error_callback_(
        Error(Error::Code::kParameterInvalid, "Received invalid message"));
    return;
  }

  ErrorOr<SenderMessage> sender_message =
      SenderMessage::Parse(message_body.value());
  if (sender_message.is_error()) {
    OSP_DLOG_WARN << "Received an invalid sender message: "
                  << sender_message.error();
    return;
  }

  for (auto& callback : callbacks_) {
    if (callback.first == sender_message.value().type) {
      callback.second(sender_message.value());
    }
  }
}

void ReceiverSessionMessager::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messager: " << error;
}

}  // namespace cast
}  // namespace openscreen
