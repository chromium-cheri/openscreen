// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CAST_SOCKET_H_
#define CAST_COMMON_CHANNEL_CAST_SOCKET_H_

#include "platform/api/logging.h"
#include "platform/base/error.h"

namespace cast {
namespace channel {

using openscreen::Error;

class CastMessage;

// Represents a simple message-oriented socket for communicating with the Cast
// V2 protocol.
class CastSocket {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnError(CastSocket* socket, const Error& error) = 0;
    virtual void OnMessage(CastSocket* socket, CastMessage* message) = 0;
  };

  CastSocket(Delegate* delegate, uint32_t socket_id)
      : delegate_(delegate), socket_id_(socket_id) {}
  virtual ~CastSocket() = default;

  virtual Error SendMessage(const CastMessage& message) = 0;

  void set_delegate(Delegate* delegate) {
    OSP_DCHECK(delegate);
    delegate_ = delegate;
  }
  uint32_t socket_id() const { return socket_id_; }

 protected:
  Delegate* delegate_;
  const uint32_t socket_id_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_CAST_SOCKET_H_
