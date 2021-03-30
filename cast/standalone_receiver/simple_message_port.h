// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_SIMPLE_MESSAGE_PORT_H_
#define CAST_STANDALONE_RECEIVER_SIMPLE_MESSAGE_PORT_H_

#include <string>
#include <vector>

#include "cast/streaming/receiver_session.h"

namespace openscreen {
namespace cast {

class SimpleMessagePort : public MessagePort {
 public:
  SimpleMessagePort();
  ~SimpleMessagePort() override;

  void SetClient(MessagePort::Client* client, std::string client_sender_id) override;
  void ResetClient() override;
  void ReceiveMessage(const std::string&  message);
  void ReceiveError(Error error);
  void PostMessage(const std::string&  sender_id,
                   const std::string&  message_namespace,
                   const std::string&  message) override;

  MessagePort::Client* client() const { return client_; }
  const std::vector<std::string>& posted_messages() const {
    return posted_messages_;
  }

 private:
  MessagePort::Client* client_ = nullptr;
  std::vector<std::string> posted_messages_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_SIMPLE_MESSAGE_PORT_H_
