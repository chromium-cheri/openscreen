// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_receiver.h"

#include "cast/common/mdns/mdns_reader.h"
#include "platform/api/trace_logging.h"

namespace cast {
namespace mdns {

MdnsReceiver::MdnsReceiver(UdpSocket* socket) : socket_(socket) {
  OSP_DCHECK(socket_);
}

MdnsReceiver::~MdnsReceiver() {
  if (state_ == State::kRunning) {
    Stop();
  }
}

void MdnsReceiver::SetQueryDelegate(Delegate* delegate) {
  query_delegate_ = delegate;
}

void MdnsReceiver::SetResponseDelegate(Delegate* delegate) {
  response_delegate_ = delegate;
}

void MdnsReceiver::Start() {
  state_ = State::kRunning;
}

void MdnsReceiver::Stop() {
  state_ = State::kStopped;
}

void MdnsReceiver::OnRead(UdpSocket* socket,
                          openscreen::ErrorOr<UdpPacket> packet_or_error) {
  if (state_ != State::kRunning || packet_or_error.is_error()) {
    return;
  }

  UdpPacket packet = packet_or_error.MoveValue();

  TRACE_SCOPED(TraceCategory::mDNS, "MdnsReceiver::OnRead");
  MdnsReader reader(packet.data(), packet.size());
  MdnsMessage message;
  if (!reader.Read(&message)) {
    return;
  }
  // TODO(yakimakha): We do not know how long it's going to take to run the
  // delegate, this should probably go to TaskRunner

  // Read delegate pointer into a local variable so it's value is not changed
  // between checking for nullptr and calling the callback
  Delegate* delegate = (message.type() == MessageType::Response)
                           ? response_delegate_
                           : query_delegate_;
  if (delegate) {
    delegate->OnMessageReceived(message, packet.source());
  }
}

void MdnsReceiver::OnError(UdpSocket* socket, Error error) {
  // This method should never be called for MdnsReciever.
  OSP_UNIMPLEMENTED();
}

void MdnsReceiver::OnSendError(UdpSocket* socket, Error error) {
  // This method should never be called for MdnsReciever.
  OSP_UNIMPLEMENTED();
}

}  // namespace mdns
}  // namespace cast
