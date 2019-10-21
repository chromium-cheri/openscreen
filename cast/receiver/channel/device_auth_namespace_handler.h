// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_CHANNEL_DEVICE_AUTH_NAMESPACE_HANDLER_H_
#define CAST_RECEIVER_CHANNEL_DEVICE_AUTH_NAMESPACE_HANDLER_H_

#include "cast/common/channel/cast_message_handler.h"

namespace cast {
namespace channel {

class DeviceAuthNamespaceHandler final : public CastMessageHandler {
 public:
  class CredentialsProvider {
   public:
    virtual int GetCurrentTransportCredentials() = 0;
  };

  explicit DeviceAuthNamespaceHandler(CredentialsProvider* creds_provider);
  ~DeviceAuthNamespaceHandler();

  // CastMessageHandler overrides.
  void OnMessage(CastSocket* socket, CastMessage&& message) override;

 private:
  CredentialsProvider* const creds_provider_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_RECEIVER_CHANNEL_DEVICE_AUTH_NAMESPACE_HANDLER_H_
