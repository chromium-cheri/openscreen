// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tcp_socket_posix.h"

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
constexpr int kDefaultMaxBacklogSize = 64;
constexpr int kInvalidFileDescriptor = -1;
}  // namespace

TcpSocketPosix::TcpSocketPosix(const IPEndpoint& local_endpoint)
    : address_(local_endpoint),
      file_descriptor_(kInvalidFileDescriptor),
      last_error_code_(Error::Code::kNone) {}

TcpSocketPosix::TcpSocketPosix(SocketAddressPosix address, int file_descriptor)
    : address_(address),
      file_descriptor_(file_descriptor),
      last_error_code_(Error::Code::kNone) {}

TcpSocketPosix::~TcpSocketPosix() {
  if (IsOpen()) {
    Close();
  }
}

std::unique_ptr<TcpSocket> TcpSocketPosix::Accept() {
  if (!EnsureInitialized()) {
    ReportSocketClosedError();
    return nullptr;
  }

  // We copy our address to new_peer_address since it should be in the same
  // family. The accept call will overwrite it.
  SocketAddressPosix new_peer_address = address_;
  socklen_t peer_address_size = new_peer_address.size();
  const int new_file_descriptor = accept(
      file_descriptor_.load(), new_peer_address.address(), &peer_address_size);
  if (new_file_descriptor == kInvalidFileDescriptor) {
    CloseOnError(Error::Code::kSocketAcceptFailure);
    return nullptr;
  }

  return std::make_unique<TcpSocketPosix>(new_peer_address,
                                          new_file_descriptor);
}

Error TcpSocketPosix::Bind() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (bind(file_descriptor_.load(), address_.address(), address_.size()) != 0) {
    return CloseOnError(Error::Code::kSocketBindFailure);
  }

  return Error::None();
}

Error TcpSocketPosix::Close() {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  const int file_descriptor_to_close =
      file_descriptor_.exchange(kInvalidFileDescriptor);
  if (close(file_descriptor_to_close) != 0) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  return Error::None();
}

Error TcpSocketPosix::Connect(const IPEndpoint& peer_endpoint) {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  SocketAddressPosix address(peer_endpoint);
  if (connect(file_descriptor_.load(), address.address(), address.size()) !=
      0) {
    return CloseOnError(Error::Code::kSocketConnectFailure);
  }

  peer_address_ = peer_endpoint;
  state_ = TcpSocket::State::kConnected;
  return Error::None();
}

int64_t TcpSocketPosix::GetFileDescriptor() const {
  return file_descriptor_.load();
}

ErrorOr<IPEndpoint> TcpSocketPosix::GetPeerAddress() const {
  if (state_ != TcpSocket::State::kConnected) {
    return Error::None();
  }

  if (!peer_address_) {
    return Error::Code::kItemNotFound;
  }

  return peer_address_.value();
}

TcpSocket::State TcpSocketPosix::GetState() const {
  return state_;
}

Error TcpSocketPosix::Listen() {
  return Listen(kDefaultMaxBacklogSize);
}

Error TcpSocketPosix::Listen(int max_backlog_size) {
  if (!EnsureInitialized()) {
    return ReportSocketClosedError();
  }

  if (listen(file_descriptor_.load(), max_backlog_size) != 0) {
    return CloseOnError(Error::Code::kSocketListenFailure);
  }

  return Error::None();
}

const SocketAddressPosix& TcpSocketPosix::GetLocalAddress() const {
  return address_;
}

bool TcpSocketPosix::EnsureInitialized() {
  if ((state_ == TcpSocket::State::kUninitialized) &&
      (last_error_code_.load() == Error::Code::kNone)) {
    return Initialize() == Error::None();
  }

  return false;
}

Error TcpSocketPosix::Initialize() {
  if (IsOpen()) {
    return Error::Code::kItemAlreadyExists;
  }

  int domain;
  switch (address_.version()) {
    case IPAddress::Version::kV4:
      domain = AF_INET;
      break;
    case IPAddress::Version::kV6:
      domain = AF_INET6;
      break;
  }

  file_descriptor_ = socket(domain, SOCK_STREAM, 0);
  if (file_descriptor_ == kInvalidFileDescriptor) {
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  const int current_flags = fcntl(file_descriptor_, F_GETFL, 0);
  if (fcntl(file_descriptor_, F_SETFL, current_flags | O_NONBLOCK) == -1) {
    close(file_descriptor_);
    last_error_code_ = Error::Code::kSocketInvalidState;
    return Error::Code::kSocketInvalidState;
  }

  state_ = TcpSocket::State::kOpen;
  // last_error_code_ should still be Error::None().
  return Error::None();
}

Error TcpSocketPosix::CloseOnError(Error::Code error_code) {
  last_error_code_ = error_code;
  Close();
  state_ = TcpSocket::State::kClosed;
  return error_code;
}

// If is_open is false, the socket has either not been initialized
// or has been closed, either on purpose or due to error.
Error TcpSocketPosix::ReportSocketClosedError() {
  last_error_code_ = Error::Code::kSocketClosedFailure;
  return Error::Code::kSocketClosedFailure;
}

// The file_descriptor_ field is initialized to an invalid value, and resets
// to that value when closed. We can assume that if the file_descriptor_ is
// set, we are open.
bool TcpSocketPosix::IsOpen() {
  return (state_ != TcpSocket::State::kUninitialized) &&
         (state_ != TcpSocket::State::kClosed);
}
}  // namespace platform
}  // namespace openscreen
