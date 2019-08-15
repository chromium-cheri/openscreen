// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(Client* client) : client_(client) {
  deletion_callback_ = [](UdpSocket* socket) {};
}

UdpSocket::~UdpSocket() {
  deletion_callback_(this);
}

void UdpSocket::SetDeletionCallback(std::function<void(UdpSocket*)> callback) {
  deletion_callback_ = callback;
}

void Bind() {
  if (!client_in_good_state_) {
    return;
  }
  BindInternal();
}

void SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex) {
  if (!client_in_good_state_) {
    return;
  }
  SetMulticastOutboundInterfaceInternal(ifindex);
}

void JoinMulticastGroup(const IPAddress& address,
                        NetworkInterfaceIndex ifindex) {
  if (!client_in_good_state_) {
    return;
  }
  JoinMulticastGroupInternal(address, ifindex);
}

void SendMessage(const void* data, size_t length, const IPEndpoint& dest) {
  if (!client_in_good_state_) {
    return;
  }
  SendMessageInternal(data, length, dest);
}

void SetDscp(DscpMode state) {
  if (!client_in_good_state_) {
    return;
  }
  SetDscpInternal(state);
}

}  // namespace platform
}  // namespace openscreen
