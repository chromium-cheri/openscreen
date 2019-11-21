// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_PORT_H_
#define CAST_STREAMING_MESSAGE_PORT_H_

#include "absl/strings/string_view.h"

namespace openscreen {
class Error;
}

namespace cast {
namespace streaming {

class MessagePort {
 public:
  class Client {
   public:
    virtual void OnMessage(absl::string_view message) = 0;
    virtual void OnError(openscreen::Error error) = 0;
  };

  virtual void SetClient(Client* client) = 0;
  virtual void PostMessage(absl::string_view message) = 0;
  virtual void Close() = 0;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_MESSAGE_PORT_H_
