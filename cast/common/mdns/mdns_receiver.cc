// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_receiver.h"

#include "cast/common/mdns/mdns_reader.h"

namespace cast {
namespace mdns {

MdnsReceiver::MdnsReceiver(UdpSocket* socket,
                           NetworkRunner* network_runner,
                           Delegate* delegate)
    : socket_(socket), network_runner_(network_runner), delegate_(delegate) {
  OSP_DCHECK(socket_ != nullptr);
  OSP_DCHECK(network_runner_ != nullptr);
  OSP_DCHECK(delegate_ != nullptr);
}

MdnsReceiver::~MdnsReceiver() {
  if (state_ == State::Running) {
    StopReceiving();
  }
}

Error MdnsReceiver::StartReceiving() {
  if (state_ == State::Running) {
    return Error::Code::kNone;
  }
  Error result = network_runner_->ReadRepeatedly(socket_, this);
  if (result.ok()) {
    state_ = State::Running;
  }
  return result;
}

Error MdnsReceiver::StopReceiving() {
  if (state_ == State::NotRunning) {
    return Error::Code::kNone;
  }
  bool result = network_runner_->CancelRead(socket_);
  if (result) {
    state_ = State::NotRunning;
    return Error::Code::kNone;
  }
  return Error::Code::kNotRunning;
}

void MdnsReceiver::OnRead(UdpPacket packet, NetworkRunner* network_runner) {
  MdnsReader reader(packet.data(), packet.size());
  MdnsMessage message;
  if (reader.Read(&message)) {
    // TODO(yakimakha): make flags a proper type and hide bit manipulation
    if ((message.flags() & kFlagResponse) != 0) {
      delegate_->OnResponseReceived(message, packet.source());
    } else {
      delegate_->OnQueryReceived(message, packet.source());
    }
  }
}

}  // namespace mdns
}  // namespace cast
