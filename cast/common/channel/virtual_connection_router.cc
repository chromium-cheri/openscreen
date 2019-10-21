// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include "cast/common/channel/cast_message_handler.h"
#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "platform/api/logging.h"

namespace cast {
namespace channel {

VirtualConnectionRouter::VirtualConnectionRouter(
    VirtualConnectionManager* vc_manager)
    : vc_manager_(vc_manager) {
  OSP_DCHECK(vc_manager);
}

VirtualConnectionRouter::~VirtualConnectionRouter() = default;

void VirtualConnectionRouter::AddHandlerForLocalId(
    const std::string& local_id,
    CastMessageHandler* endpoint) {
  endpoints_.emplace(local_id, endpoint);
}

void VirtualConnectionRouter::RemoveHandlerForLocalId(
    const std::string& local_id) {
  endpoints_.erase(local_id);
}

void VirtualConnectionRouter::TakeSocket(SocketErrorHandler* error_handler,
                                         std::unique_ptr<CastSocket> socket) {
  uint32_t id = socket->socket_id();
  socket->set_client(this);
  sockets_.emplace(id, SocketWithHandler{std::move(socket), error_handler});
}

void VirtualConnectionRouter::CloseSocket(uint32_t id) {
  auto it = sockets_.find(id);
  if (it != sockets_.end()) {
    std::unique_ptr<CastSocket> socket = std::move(it->second.socket);
    SocketErrorHandler* error_handler = it->second.error_handler;
    sockets_.erase(it);
    error_handler->OnClose(socket.get());
  }
}

Error VirtualConnectionRouter::SendMessage(const VirtualConnection& vconn,
                                           CastMessage&& message) {
  if (!vc_manager_->HasConnection(vconn)) {
    return Error::Code::kUnknownError;
  }
  auto it = sockets_.find(vconn.socket_id);
  if (it == sockets_.end()) {
    return Error::Code::kUnknownError;
  }
  message.set_source_id(vconn.local_id);
  message.set_destination_id(vconn.peer_id);
  return it->second.socket->SendMessage(message);
}

void VirtualConnectionRouter::OnError(CastSocket* socket, Error error) {
  uint32_t id = socket->socket_id();
  auto it = sockets_.find(id);
  if (it != sockets_.end()) {
    std::unique_ptr<CastSocket> socket_owned = std::move(it->second.socket);
    SocketErrorHandler* error_handler = it->second.error_handler;
    sockets_.erase(it);
    error_handler->OnError(socket, error);
  }
}

void VirtualConnectionRouter::OnMessage(CastSocket* socket,
                                        CastMessage message) {
  const std::string& local_id = message.destination_id();
  auto it = endpoints_.find(local_id);
  if (it != endpoints_.end()) {
    it->second->OnMessage(socket, std::move(message));
  }
}

}  // namespace channel
}  // namespace cast
