// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RECEIVER_H_
#define CAST_COMMON_MDNS_MDNS_RECEIVER_H_

#include "cast/common/mdns/mdns_records.h"
#include "platform/api/network_runner.h"
#include "platform/api/udp_packet.h"
#include "platform/api/udp_read_callback.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace cast {
namespace mdns {

using Error = openscreen::Error;
using NetworkRunner = openscreen::platform::NetworkRunner;
using UdpSocket = openscreen::platform::UdpSocket;
using UdpReadCallback = openscreen::platform::UdpReadCallback;
using UdpPacket = openscreen::platform::UdpPacket;
using IPEndpoint = openscreen::IPEndpoint;

class MdnsReceiver : UdpReadCallback {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnQueryReceived(const MdnsMessage& message,
                                 const IPEndpoint& sender) = 0;
    virtual void OnResponseReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) = 0;
  };

  MdnsReceiver(UdpSocket* socket,
               NetworkRunner* network_runner,
               Delegate* delegate);
  MdnsReceiver(const MdnsReceiver& other) = delete;
  MdnsReceiver(MdnsReceiver&& other) noexcept = default;
  ~MdnsReceiver() override;

  MdnsReceiver& operator=(const MdnsReceiver& other) = delete;
  MdnsReceiver& operator=(MdnsReceiver&& other) noexcept = default;

  Error StartReceiving();
  Error StopReceiving();

  void OnRead(UdpPacket packet, NetworkRunner* network_runner) override;

 private:
  enum class State {
    NotRunning,
    Running,
  };

  UdpSocket* socket_;
  NetworkRunner* network_runner_;
  Delegate* delegate_;
  State state_ = State::NotRunning;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RECEIVER_H_
