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
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  const int current_flags = fcntl(file_descriptor_, F_GETFL, 0);
  if (fcntl(file_descriptor_, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    return CloseOnError(Error::Code::kSocketInvalidState);
  }

  is_open_ = true;

  // last_error_ should still be Error::None().
  return Error::None();
}

ErrorOr<StreamSocketPosix> StreamSocketPosix::Accept() {
  if (!is_open_) {
    return ReportSocketClosedError();
  }

  // We copy our address to new_peer_address since it should be in the same
  // family. The accept call will overwrite it.
  SocketAddressPosix new_peer_address = address_.value();
  socklen_t peer_address_size = new_peer_address.Size();
  const int new_file_descriptor =
      accept(file_descriptor_, new_peer_address.address(), &peer_address_size);
  if (new_file_descriptor == -1) {
    return CloseOnError(Error::Code::kSocketAcceptFailure);
  }

  return ErrorOr<StreamSocketPosix>(
      StreamSocketPosix(std::move(new_peer_address), new_file_descriptor));
}

Error StreamSocketPosix::Bind() {
  if (!is_open_) {
    return ReportSocketClosedError();
  }

  if (!address_) {
    return CloseOnError(Error::Code::kParameterInvalid);
  }

  SocketAddressPosix address(address_.value());
  if (bind(file_descriptor_, address.address(), address.Size()) != 0) {
    return CloseOnError(Error::Code::kSocketBindFailure);
  }

  return Error::None();
}

Error StreamSocketPosix::Close() {
  if (!is_open_) {
    return ReportSocketClosedError();
  }

  if (close(file_descriptor_) != 0) {
    last_error_ = Error::Code::kSocketClosedFailure;
    return last_error_;
  }

  return Error::None();
}

Error StreamSocketPosix::Connect(const IPEndpoint& peer_endpoint) {
  if (!is_open_) {
    return ReportSocketClosedError();
  }

  SocketAddressPosix address(peer_endpoint);
  if (connect(file_descriptor_, address.address(), address.Size()) != 0) {
    return CloseOnError(Error::Code::kSocketConnectFailure);
  }

  return Error::None();
}

Error StreamSocketPosix::Listen() {
  return Listen(kMaxBacklogSize);
}

Error StreamSocketPosix::Listen(int max_backlog_size) {
  if (!is_open_) {
    return ReportSocketClosedError();
  }

  if (listen(file_descriptor_, max_backlog_size) != 0) {
    return CloseOnError(Error::Code::kSocketListenFailure);
  }

  return Error::None();
}

Error StreamSocketPosix::CloseOnError(Error error) {
  last_error_ = error;
  Close();

  return last_error_;
}

// If is_open is false, the socket has either not been initialized
// or has been closed, either on purpose or due to error.
Error StreamSocketPosix::ReportSocketClosedError() {
  last_error_ = Error::Code::kSocketClosedFailure;

  return last_error_;
}
}  // namespace platform
}  // namespace openscreen
