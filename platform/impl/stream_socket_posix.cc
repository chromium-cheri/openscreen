// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/stream_socket_posix.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

namespace {
constexpr int kMaxBacklogSize = 64;
}

StreamSocketPosix::StreamSocketPosix(const IPEndpoint& local_endpoint)
    : address_(SocketAddressPosix(local_endpoint)) {
  int domain;
  if (local_endpoint.address.IsV4()) {
    domain = AF_INET;
  } else {
    domain = AF_INET6;
  }

  file_descriptor_ = socket(domain, SOCK_STREAM, 0);
  if (file_descriptor_ == -1) {
    in_valid_state_ = false;
    return;
  }

  const int current_flags = fcntl(file_descriptor_, F_GETFL, 0);
  if (fcntl(file_descriptor_, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    close(file_descriptor_);
    in_valid_state_ = false;
    return;
  }
}

ErrorOr<StreamSocketPosix> StreamSocketPosix::Accept(
    const IPEndpoint& peer_endpoint) {
  if (!in_valid_state_) {
    return Error::Code::kSocketInvalidState;
  }

  SocketAddressPosix peer_address(peer_endpoint);
  socklen_t peer_address_size = peer_address.size();
  const int file_descriptor =
      accept(file_descriptor_, peer_address.address(), &peer_address_size);
  if (file_descriptor == -1) {
    in_valid_state_ = false;
    return Error::Code::kSocketAcceptFailure;
  }

  peer_address.set_size(peer_address_size);
  return ErrorOr<StreamSocketPosix>(
      StreamSocketPosix(file_descriptor_, *address_));
}

Error StreamSocketPosix::Bind() {
  if (!in_valid_state_) {
    return Error::Code::kSocketInvalidState;
  }

  if (bind(file_descriptor_, address_->address(), address_->size()) != 0) {
    in_valid_state_ = false;
    return Error::Code::kSocketBindFailure;
  }

  return Error::None();
}

Error StreamSocketPosix::Close() {
  if (!in_valid_state_) {
    return Error::Code::kSocketInvalidState;
  }

  if (close(file_descriptor_) != 0) {
    in_valid_state_ = false;
    return Error::Code::kSocketClosedFailure;
  }

  return Error::None();
}

Error StreamSocketPosix::Connect(const IPEndpoint& peer_endpoint) {
  if (!in_valid_state_) {
    return Error::Code::kSocketInvalidState;
  }

  SocketAddressPosix address(peer_endpoint);
  if (connect(file_descriptor_, address.address(), address.size()) != 0) {
    in_valid_state_ = false;
    return Error::Code::kSocketConnectFailure;
  }

  return Error::None();
}

Error StreamSocketPosix::Listen() {
  if (!in_valid_state_) {
    return Error::Code::kSocketInvalidState;
  }

  if (listen(file_descriptor_, kMaxBacklogSize) != 0) {
    in_valid_state_ = false;
    return Error::Code::kSocketListenFailure;
  }

  return Error::None();
}

StreamSocketPosix::StreamSocketPosix(int file_descriptor,
                                     absl::optional<SocketAddressPosix> address)
    : address_(address), file_descriptor_(file_descriptor) {}

}  // namespace platform
}  // namespace openscreen
