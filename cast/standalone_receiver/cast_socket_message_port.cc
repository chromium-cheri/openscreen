// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_socket_message_port.h"

#include <utility>

namespace openscreen {
namespace cast {

CastSocketMessagePort::CastSocketMessagePort(std::unique_ptr<CastSocket> socket)
    : socket_(std::move(socket)) {
  socket->SetClient(this);
}

CastSocketMessagePort::~CastSocketMessagePort() = default;

void CastSocketMessagePort::SetClient(MessagePort::Client* client) {
  client_ = client;
}

void CastSocketMessagePort::OnError(CastSocket* socket, Error error) {
  if (client_) {
    client_->OnError(error);
  }
}

void CastSocketMessagePort::OnMessage(CastSocket* socket,
                                      ::cast::channel::CastMessage message) {
  if (client_) {
    client_->OnMessage(message.source_id(), message.namespace_(),
                       message.payload_utf8());
  }
}

void CastSocketMessagePort::PostMessage(absl::string_view sender_id,
                                        absl::string_view message_namespace,
                                        absl::string_view message) {
  // TODO: add message posting
  // create a CastMessage here.
}

}  // namespace cast
}  // namespace openscreen
