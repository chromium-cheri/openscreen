// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <string>

#include "discovery/mdns/mdns_responder_adapter.h"
#include "discovery/mdns/mdns_responder_platform.h"
#include "platform/api/event_waiter.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "platform/base/event_loop.h"

namespace openscreen {
namespace {

constexpr platform::Milliseconds kLoopDuration(3000);

void BrowseDemo(const std::string& service_type) {
  mdns::MdnsResponderAdapter mdns_adapter;
  platform::EventWaiterPtr waiter;
  (&mdns_adapter)->Init();

  const auto& v4_sockets = (&mdns_adapter)->GetIPv4SocketsToWatch();
  const auto& v6_sockets = (&mdns_adapter)->GetIPv6SocketsToWatch();
  waiter = platform::CreateEventWaiter();
  for (auto* socket : v4_sockets) {
    platform::WatchUdpSocketIPv4Readable(waiter, socket);
  }
  for (auto* socket : v6_sockets) {
    platform::WatchUdpSocketIPv6Readable(waiter, socket);
  }

  const auto start_time = platform::GetMonotonicTimeNow();
  auto now = platform::GetMonotonicTimeNow();
  (&mdns_adapter)->StartBrowse(service_type);
  while (platform::ToMilliseconds(now - start_time) < kLoopDuration) {
    (&mdns_adapter)->Execute();
    auto data =
        platform::OnePlatformLoopIteration(waiter, platform::Milliseconds(500));
    for (auto& packet : data.v4_data) {
      (&mdns_adapter)
          ->OnDataReceived(packet.source, packet.original_destination,
                           packet.bytes.data(), packet.bytes.size(),
                           packet.socket);
    }
    for (auto& packet : data.v6_data) {
      (&mdns_adapter)
          ->OnDataReceived(packet.source, packet.original_destination,
                           packet.bytes.data(), packet.bytes.size(),
                           packet.socket);
    }
    now = platform::GetMonotonicTimeNow();
  }
  (&mdns_adapter)->StopBrowse(service_type);
  platform::StopWatchingNetworkChange(waiter);
  for (auto* socket : v4_sockets) {
    platform::StopWatchingUdpSocketIPv4Readable(waiter, socket);
  }
  for (auto* socket : v6_sockets) {
    platform::StopWatchingUdpSocketIPv6Readable(waiter, socket);
  }
  platform::DestroyEventWaiter(waiter);
  (&mdns_adapter)->Close();
}

}  // namespace
}  // namespace openscreen

int main(int argc, char** argv) {
  std::string service_type("_openscreen._udp");
  if (argc >= 2) {
    service_type = argv[1];
  }

  openscreen::BrowseDemo(service_type);

  return 0;
}
