// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SESSION_MESSAGER_H_
#define CAST_STREAMING_SESSION_MESSAGER_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "cast/common/public/message_port.h"
#include "cast/streaming/answer_messages.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver_message.h"
#include "cast/streaming/sender_message.h"
#include "json/value.h"
#include "platform/api/task_runner.h"
#include "util/weak_ptr.h"

namespace openscreen {
namespace cast {

// A message port interface designed specifically for use by the Receiver
// and Sender session classes.
class SessionMessager : public MessagePort::Client {
 public:
  using ErrorCallback = std::function<void(Error)>;
  using ReplyCallback = std::function<void(ReceiverMessage)>;

  SessionMessager(MessagePort* message_port,
                  std::string source_id,
                  std::string destination_id,
                  ErrorCallback cb);
  virtual ~SessionMessager();

 protected:
  // Barebones message sending method shared by both children.
  Error SendMessage(const std::string& namespace_,
                    const Json::Value& message_root);

  MessagePort* const message_port_;
  ErrorCallback error_callback_;
  std::string destination_id_;
};

class SenderSessionMessager final : public SessionMessager {
 public:
  SenderSessionMessager(MessagePort* message_port,
                        std::string source_id,
                        std::string destination_id,
                        ErrorCallback cb,
                        TaskRunner* task_runner);

  // Send a request (with optional reply callback)
  Error SendRequest(SenderMessage message,
                    ReceiverMessage::Type reply_type,
                    absl::optional<ReplyCallback> cb);

  // MessagePort::Client overrides
  void OnMessage(const std::string& source_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  TaskRunner* const task_runner_;

  // We keep a list here of replies we are expecting--if the reply is
  // received for this sequence number, we call its respective callback,
  // otherwise it is called after an internally specified timeout.
  std::vector<std::pair<int, ReplyCallback>> awaiting_replies_;

  WeakPtrFactory<SenderSessionMessager> weak_factory_{this};
};

class ReceiverSessionMessager final : public SessionMessager {
 public:
  using RequestCallback = std::function<void(SenderMessage)>;
  ReceiverSessionMessager(MessagePort* message_port,
                          std::string source_id,
                          std::string destination_id,
                          ErrorCallback cb);

  // Set sender message handler.
  void SetHandler(SenderMessage::Type type, RequestCallback cb);

  // Send a JSON message.
  Error SendMessage(ReceiverMessage message);

  // MessagePort::Client overrides
  void OnMessage(const std::string& source_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  std::vector<std::pair<SenderMessage::Type, RequestCallback>> callbacks_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SESSION_MESSAGER_H_
