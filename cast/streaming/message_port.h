// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_PORT_H_
#define CAST_STREAMING_MESSAGE_PORT_H_

namespace openscreen {
class Error;
}

namespace cast {
namespace streaming {

// This interface is intended to provide an abstraction for communicating
// cast messages across a pipe with guaranteed delivery. This is used to
// decouple the cast receiver session (and potentially other classes) from any
// network implementation.
class MessagePort {
 public:
  class Client {
   public:
    virtual void OnMessage(const std::string& sender_id,
                           const std::string& namespace_,
                           const std::string& message) = 0;
    virtual void OnError(openscreen::Error error) = 0;
  };

  virtual ~MessagePort() = default;
  virtual void SetClient(Client* client) = 0;
  virtual void PostMessage(std::string message) = 0;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_MESSAGE_PORT_H_
