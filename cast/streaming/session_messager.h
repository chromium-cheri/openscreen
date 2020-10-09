// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SESSION_MESSAGER_H_
#define CAST_STREAMING_SESSION_MESSAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "cast/common/public/message_port.h"
#include "json/value.h"

namespace openscreen {
namespace cast {

// A message port interface designed specifically for use by the Receiver
// and Sender session classes.
class SessionMessager : public MessagePort::Client {
 public:
  struct Message {
    std::string sender_id = {};
    std::string message_namespace = {};
    int sequence_number = 0;
    Json::Value body;
  };

  using MessageCallback = std::function<void(Message)>;
  using ErrorCallback = std::function<void(Error)>;

  SessionMessager(MessagePort* message_port,
                  std::string sender_id,
                  ErrorCallback cb);
  ~SessionMessager();

  // Set a message callback, such as OnOffer or OnAnswer.
  void SetHandler(std::string message_type, MessageCallback cb);

  // Send a JSON message.
  void SendMessage(Message message);

  // MessagePort::Client overrides
  void OnMessage(const std::string& sender_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  // Since the number of message callbacks is expected to be low,
  // we use a vector of key, value pairs instead of a map.
  std::vector<std::pair<std::string, MessageCallback>> callbacks_;

  MessagePort* const message_port_;
  ErrorCallback error_callback_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SESSION_MESSAGER_H_
