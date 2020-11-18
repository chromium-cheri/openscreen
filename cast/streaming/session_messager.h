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

namespace openscreen {
namespace cast {

// A message port interface designed specifically for use by the Receiver
// and Sender session classes.
class SessionMessager : public MessagePort::Client {
 public:
  struct Message {
    // |sender_id| is always the other side of the message port connection:
    // the source if an incoming message, or the destination if an outgoing
    // message. The sender ID of this side of the message port is passed in
    // through the SessionMessager constructor.
    std::string sender_id = {};

    // The namespace of the message. Currently only kCastWebrtcNamespace
    // is supported--when new namespaces are added this class will have to be
    // updated.
    std::string message_namespace = {};

    // The sequence number of the message. This is important currently for
    // ensuring we reply to the proper request message, such as for OFFER/ANSWER
    // exchanges.
    int32_t sequence_number = -1;

    // The body of the message, as a JSON object.
    Json::Value body;
  };

  using MessageCallback = std::function<void(Message)>;
  using ErrorCallback = std::function<void(Error)>;

  using ReplyCallback = std::function<void(ReceiverMessage)>;

  SessionMessager(MessagePort* message_port,
                  std::string source_id,
                  ErrorCallback cb);
  virtual ~SessionMessager();

  // MessagePort::Client overrides
  void OnMessage(const std::string& source_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 protected:
  MessagePort* const message_port_;
  ErrorCallback error_callback_;
};

class SenderSessionMessager final : public SessionMessager {
 public:
  SenderSessionMessager(MessagePort* message_port,
                        std::string source_id,
                        ErrorCallback cb);

  // Send a request (with optional reply callback)
  void SendRequest(SenderMessage message,
                   ReceiverMessage::Type reply_type,
                   absl::optional<ReplyCallback> cb);

  // TODO: private callback list with timeout options?
};

class ReceiverSessionMessager final : public SessionMessager {
 public:
  using RequestCallback = std::function<void(SenderMessage)>;
  ReceiverSessionMessager(MessagePort* message_port,
                          std::string source_id,
                          ErrorCallback cb);

  // Set sender message handler.
  void SetHandler(SenderMessage::Type type, RequestCallback cb);

  // Send a JSON message.
  Error SendMessage(ReceiverMessage message);

 private:
  std::vector<std::pair<SenderMessage::Type, RequestCallback>> callbacks_;
  std::string sender_id_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SESSION_MESSAGER_H_
