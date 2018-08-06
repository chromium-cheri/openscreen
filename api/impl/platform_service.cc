// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/platform_service.h"

#include <algorithm>

#include "platform/api/error.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"

namespace openscreen {
namespace {

std::vector<platform::UdpSocketIPv4Ptr> SetupMulticastSocketsV4(
    const std::vector<int>& index_list) {
  std::vector<platform::UdpSocketIPv4Ptr> fds;
  for (const auto ifindex : index_list) {
    auto* socket = platform::CreateUdpSocketIPv4();
    if (!JoinUdpMulticastGroupIPv4(socket, IPv4Address{{224, 0, 0, 251}},
                                   ifindex)) {
      LOG_ERROR << "join multicast group failed: "
                << platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }
    if (!BindUdpSocketIPv4(
            socket, IPv4Endpoint{IPv4Address{{0, 0, 0, 0}}, 5353}, ifindex)) {
      LOG_ERROR << "bind failed: " << platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }

    LOG_INFO << "listening on interface " << ifindex;
    fds.push_back(socket);
  }
  return fds;
}

// TODO
std::vector<platform::UdpSocketIPv6Ptr> SetupMulticastSocketsV6(
    const std::vector<int>& index_list) {
  return {};
}

}  // namespace

PlatformService::PlatformService() {
  waiter_ = platform::CreateEventWaiter();
}

PlatformService::~PlatformService() = default;

// TODO: fix registration insanity
void PlatformService::RegisterInterfaces(
    mdns::MdnsResponderAdapter* mdns_responder) {
  auto addrinfo = platform::GetInterfaceAddresses();
  std::vector<int> v4_index_list;
  std::vector<int> v6_index_list;
  for (const auto& interface : addrinfo) {
    if (!interface.ipv4_addresses.empty()) {
      v4_index_list.push_back(interface.info.index);
    }
    if (!interface.ipv6_addresses.empty()) {
      v6_index_list.push_back(interface.info.index);
    }
  }

  std::sort(v4_index_list.begin(), v4_index_list.end());
  std::sort(v6_index_list.begin(), v6_index_list.end());
  v4_index_list.erase(std::unique(v4_index_list.begin(), v4_index_list.end()),
                      v4_index_list.end());
  v6_index_list.erase(std::remove_if(v6_index_list.begin(), v6_index_list.end(),
                                     [&v4_index_list](int index) {
                                       return std::find(v4_index_list.begin(),
                                                        v4_index_list.end(),
                                                        index) !=
                                              v4_index_list.end();
                                     }),
                      v6_index_list.end());
  v4_sockets_ = SetupMulticastSocketsV4(v4_index_list);
  v6_sockets_ = SetupMulticastSocketsV6(v6_index_list);

  // Listen on all interfaces
  // TODO: v6
  auto fd_it = v4_sockets_.begin();
  for (int index : v4_index_list) {
    const auto& addr = *std::find_if(
        addrinfo.begin(), addrinfo.end(),
        [index](const openscreen::platform::InterfaceAddresses& addr) {
          return addr.info.index == index;
        });
    // Pick any address for the given interface.
    mdns_responder->RegisterInterface(addr.info, addr.ipv4_addresses.front(),
                                      *fd_it++, false);
  }

  for (auto* socket : v4_sockets_) {
    platform::WatchUdpSocketIPv4Readable(waiter_, socket);
  }
  for (auto* socket : v6_sockets_) {
    platform::WatchUdpSocketIPv6Readable(waiter_, socket);
  }
}

void PlatformService::DeregisterInterfaces(
    mdns::MdnsResponderAdapter* mdns_responder) {
  for (auto* socket : v4_sockets_) {
    mdns_responder->DeregisterInterface(socket);
  }
  for (auto* socket : v6_sockets_) {
    mdns_responder->DeregisterInterface(socket);
  }

  for (auto* socket : v4_sockets_) {
    platform::StopWatchingUdpSocketIPv4Readable(waiter_, socket);
  }
  for (auto* socket : v6_sockets_) {
    platform::StopWatchingUdpSocketIPv6Readable(waiter_, socket);
  }
}

void PlatformService::RunEventLoopOnce() {
  auto data = platform::OnePlatformLoopIteration(waiter_);
  callback_->callback(std::move(data));
}

}  // namespace openscreen
