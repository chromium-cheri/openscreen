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
    : address_(local_endpoint), last_error_(Error::None()) {}

StreamSocketPosix::StreamSocketPosix(SocketAddressPosix address,
                                     int file_descriptor)
    : address_(address),
      file_descriptor_(file_descriptor),
      last_error_(Error::None()) {}

StreamSocketPosix::~StreamSocketPosix() {
  if (is_open_) {
    Close();
  }
}

Error StreamSocketPosix::Initialize() {
  if (last_error_ != Error::None()) {
    return last_error_;
  }

  if (is_open_ == true) {
    return Error::Code::kItemAlreadyExists;
  }

  int domain;
  switch (address_.value().version()) {
    case IPAddress::Version::kV4:
      domain = AF_INET;
      break;
    case IPAddress::Version::kV6:
      domain = AF_INET6;
      break;
  }

  file_descriptor_ = socket(domain, SOCK_STREAM, 0);
  if (file_descriptor_ == -1) {
    CloseOnError(Error::Code::kSocketInvalidState);
    return last_error_;
  }

  const int current_flags = fcntl(file_descriptor_, F_GETFL, 0);
  if (fcntl(file_descriptor_, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    close(file_descriptor_);
    CloseOnError(Error::Code::kSocketInvalidState);
    return last_error_;
  }

  is_open_ = true;
  return last_error_;
}

ErrorOr<StreamSocketPosix> StreamSocketPosix::Accept() {
  if (last_error_ != Error::None()) {
    return Error::Code::kSocketInvalidState;
  }

  // We copy our address to new_peer_address since it should be in the same
  // family. The accept call will overwrite it.
  SocketAddressPosix new_peer_address = address_.value();
  socklen_t peer_address_size = new_peer_address.Size();
  const int new_file_descriptor =
      accept(file_descriptor_, new_peer_address.address(), &peer_address_size);
  if (new_file_descriptor == -1) {
    CloseOnError(Error::Code::kSocketAcceptFailure);
    return last_error_;
  }

  return ErrorOr<StreamSocketPosix>(
      StreamSocketPosix(std::move(new_peer_address), new_file_descriptor));
}

Error StreamSocketPosix::Bind() {
  if (last_error_ != Error::None()) {
    return Error::Code::kSocketInvalidState;
  }

  if (!address_) {
    return Error::Code::kParameterInvalid;
  }

  SocketAddressPosix address(address_.value());
  if (bind(file_descriptor_, address.address(), address.Size()) != 0) {
    CloseOnError(Error::Code::kSocketBindFailure);
  }

  return last_error_;
}

Error StreamSocketPosix::Close() {
  if (last_error_ != Error::None()) {
    return Error::Code::kSocketInvalidState;
  }

  if (!is_open_) {
    return Error::Code::kAlreadyClosed;
  }

  if (close(file_descriptor_) != 0) {
    CloseOnError(Error::Code::kSocketClosedFailure);
  }

  return last_error_;
}

Error StreamSocketPosix::Connect(const IPEndpoint& peer_endpoint) {
  if (last_error_ != Error::None()) {
    return Error::Code::kSocketInvalidState;
  }

  SocketAddressPosix address(peer_endpoint);
  if (connect(file_descriptor_, address.address(), address.Size()) != 0) {
    CloseOnError(Error::Code::kSocketConnectFailure);
  }

  return last_error_;
}

Error StreamSocketPosix::Listen() {
  return Listen(kMaxBacklogSize);
}

Error StreamSocketPosix::Listen(int max_backlog_size) {
  if (last_error_ != Error::None()) {
    return Error::Code::kSocketInvalidState;
  }

  if (listen(file_descriptor_, max_backlog_size) != 0) {
    CloseOnError(Error::Code::kSocketListenFailure);
  }

  return last_error_;
}

void StreamSocketPosix::CloseOnError(Error error) {
  last_error_ = error;
  Close();
}
}  // namespace platform
}  // namespace openscreen
