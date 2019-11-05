// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CONNECTION_NAMESPACE_HANDLER_H_
#define CAST_COMMON_CHANNEL_CONNECTION_NAMESPACE_HANDLER_H_

#include "cast/common/channel/cast_message_handler.h"
#include "util/json/json_reader.h"

namespace cast {
namespace channel {

struct VirtualConnection;
class VirtualConnectionManager;

// Handles CastMessages in the connection namespace by opening and closing
// VirtualConnections on the socket on which the messages were received.
class ConnectionNamespaceHandler final : public CastMessageHandler {
 public:
  class VirtualConnectionPolicy {
   public:
    virtual ~VirtualConnectionPolicy() = default;

    virtual bool IsConnectionAllowed(const VirtualConnection& vconn) = 0;
  };

  ConnectionNamespaceHandler(VirtualConnectionManager* vc_manager,
                             VirtualConnectionPolicy* vc_policy);
  ~ConnectionNamespaceHandler() override;

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 CastMessage&& message) override;

 private:
  void HandleConnect(VirtualConnectionRouter* router,
                     CastSocket* socket,
                     CastMessage&& message,
                     Json::Value&& value);
  void HandleClose(VirtualConnectionRouter* router,
                   CastSocket* socket,
                   CastMessage&& message,
                   Json::Value&& value);

  void SendClose(VirtualConnectionRouter* router, VirtualConnection&& vconn);
  void SendConnectedResponse(VirtualConnectionRouter* router,
                             const VirtualConnection& vconn,
                             int max_protocol_version);

  VirtualConnectionManager* const vc_manager_;
  VirtualConnectionPolicy* const vc_policy_;
  openscreen::JsonReader json_reader_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_CONNECTION_NAMESPACE_HANDLER_H_
