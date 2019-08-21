// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_receiver.h"

#include "cast/common/mdns/mdns_reader.h"
#include "platform/api/trace_logging.h"

namespace cast {
namespace mdns {

MdnsReceiver::MdnsReceiver(UdpSocket* socket, Delegate* delegate)
    : socket_(socket), delegate_(delegate) {
  OSP_DCHECK(socket_);
  OSP_DCHECK(delegate_);
}

MdnsReceiver::~MdnsReceiver() {
  if (state_ == State::kRunning) {
    Stop();
  }
}

Error MdnsReceiver::Start() {
  if (state_ == State::kRunning) {
    return Error::Code::kNone;
  }
  socket_->enable_reading();
  state_ = State::kRunning;
  return Error::Code::kNone;
}

Error MdnsReceiver::Stop() {
  if (state_ == State::kStopped) {
    return Error::Code::kNone;
  }
  socket_->disable_reading();
  state_ = State::kStopped;
  return Error::Code::kNone;
}

void MdnsReceiver::OnRead(UdpSocket* socket,
                          openscreen::ErrorOr<UdpPacket> packet_or_error) {
  if (packet_or_error.is_error()) {
    return;
  }

  UdpPacket packet = packet_or_error.MoveValue();

  TRACE_SCOPED(TraceCategory::mDNS, "MdnsReceiver::OnRead");
  MdnsReader reader(packet.data(), packet.size());
  MdnsMessage message;
  if (!reader.Read(&message)) {
    return;
  }
  if (message.type() == MessageType::Response) {
    delegate_->OnResponseReceived(message, packet.source());
  } else {
    delegate_->OnQueryReceived(message, packet.source());
  }
}

void MdnsReceiver::OnError(UdpSocket* socket, Error error) {
  OSP_UNIMPLEMENTED();
}

void MdnsReceiver::OnSendError(UdpSocket* socket, Error error) {
  OSP_UNIMPLEMENTED();
}

}  // namespace mdns
}  // namespace cast
