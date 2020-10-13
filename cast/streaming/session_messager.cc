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
                                 std::string sender_id,
                                 ErrorCallback cb)
    : message_port_(message_port), error_callback_(std::move(cb)) {
  OSP_DCHECK(message_port_);
  message_port_->SetClient(this, sender_id);
}

SessionMessager::~SessionMessager() {
  message_port_->ResetClient();
}

void SessionMessager::SetHandler(std::string message_type,
                                 SessionMessager::MessageCallback cb) {
  for (auto& pair : callbacks_) {
    if (pair.first == message_type) {
      pair.second = std::move(cb);
      return;
    }
  }

  callbacks_.emplace_back(message_type, std::move(cb));
}

void SessionMessager::SendMessage(Message message) {
  if (message.message_namespace != kCastWebrtcNamespace) {
    OSP_DLOG_WARN << "Not sending message with invalid namespace: "
                  << message.message_namespace;
    return;
  }
  if (message.body.isNull()) {
    OSP_DLOG_WARN << "Not sending message with no body.";
  }
  message.body[kSequenceNumber] = message.sequence_number;

  auto body_or_error = json::Stringify(message.body);
  if (body_or_error.is_value()) {
    OSP_DVLOG << "Sending message: SENDER[" << message.sender_id
              << "], NAMESPACE[" << message.message_namespace << "], BODY:\n"
              << body_or_error.value();
    message_port_->PostMessage(message.sender_id, message.message_namespace,
                               body_or_error.value());
  } else {
    OSP_DLOG_WARN << "Sending message failed with error:\n"
                  << body_or_error.error();

    error_callback_(body_or_error.error());
  }
}

void SessionMessager::OnMessage(const std::string& sender_id,
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

  std::string key;
  if (!json::ParseAndValidateString(message_json.value()[kKeyType], &key)) {
    OSP_DLOG_WARN << "Invalid message key";
    return;
  }

  // Not all messages have results, but if they do the result must be OK.
  std::string result;
  if (!json::ParseAndValidateString(message_json.value()[kResult], &result)) {
    result.clear();
  }

  for (auto& pair : callbacks_) {
    if (pair.first == key) {
      // Currently all body keys are the lowercase version of the message type
      // key. This may need to be refactored if this is no longer the case.
      absl::AsciiStrToLower(&key);
      Json::Value body;
      if (result.empty() || result == kResultOk) {
        body = message_json.value()[key];
      } else {
        body = message_json.value()[kErrorMessageBody];
      }
      pair.second(Message{sender_id.data(), message_namespace.data(),
                          sequence_number, std::move(body)});
      return;
    }
  }
}

void SessionMessager::OnError(Error error) {
  OSP_DLOG_WARN << "Received an error in the session messager: " << error;
}

}  // namespace cast
}  // namespace openscreen
