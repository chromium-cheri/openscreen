// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/internal_services.h"

#include <algorithm>

#include "api/impl/mdns_responder_service.h"
#include "base/make_unique.h"
#include "discovery/mdns/mdns_responder_adapter_impl.h"
#include "platform/api/error.h"

namespace openscreen {
namespace {

#if 1
constexpr char kServiceType[] = "_openscreen._udp";
#else
constexpr char kServiceType[] = "_googlecast._tcp";
#endif

class MdnsResponderAdapterImplFactory final
    : public MdnsResponderAdapterFactory {
 public:
  MdnsResponderAdapterImplFactory() = default;
  ~MdnsResponderAdapterImplFactory() override = default;

  std::unique_ptr<mdns::MdnsResponderAdapter> Create() override {
    return MakeUnique<mdns::MdnsResponderAdapterImpl>();
  }
};

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

// static
void InternalServices::RunEventLoopOnce() {
  auto* services = GetInstance();
  services->mdns_service_->HandleNewEvents(
      platform::OnePlatformLoopIteration(services->internal_service_waiter_));
}

// static
std::unique_ptr<ScreenListener> InternalServices::CreateListener(
    const MdnsScreenListenerConfig& config,
    ScreenListenerObserver* observer) {
  auto* services = GetInstance();
  services->EnsureMdnsServiceCreated();
  auto listener =
      MakeUnique<ScreenListenerImpl>(observer, services->mdns_service_.get());
  return listener;
}

// static
std::unique_ptr<ScreenPublisher> InternalServices::CreatePublisher(
    const ScreenPublisher::Config& config,
    ScreenPublisher::Observer* observer) {
  auto* services = GetInstance();
  services->EnsureMdnsServiceCreated();
  // TODO(btolsch): Hostname and instance should either come from config or
  // platform+generated.
  services->mdns_service_->SetServiceConfig(
      "turtle-deadbeef", "deadbeef", config.connection_server_port,
      config.network_interface_indices, {"fn=" + config.friendly_name});
  auto publisher =
      MakeUnique<ScreenPublisherImpl>(observer, services->mdns_service_.get());
  return publisher;
}

// static
InternalServices* InternalServices::GetInstance() {
  static InternalServices services;
  return &services;
}

InternalServices::InternalPlatformLinkage::InternalPlatformLinkage(
    InternalServices* parent)
    : parent_(parent) {}
InternalServices::InternalPlatformLinkage::~InternalPlatformLinkage() = default;

MdnsPlatformService::BoundInterfaces
InternalServices::InternalPlatformLinkage::RegisterInterfaces(
    const std::vector<int32_t>& interface_index_whitelist) {
  auto addrinfo = platform::GetInterfaceAddresses();
  std::vector<int> v4_index_list;
  std::vector<int> v6_index_list;
  for (const auto& interface : addrinfo) {
    if (!interface_index_whitelist.empty() &&
        std::find(interface_index_whitelist.begin(),
                  interface_index_whitelist.end(),
                  interface.info.index) == interface_index_whitelist.end()) {
      continue;
    }
    if (!interface.ipv4_addresses.empty()) {
      v4_index_list.push_back(interface.info.index);
    } else if (!interface.ipv6_addresses.empty()) {
      v6_index_list.push_back(interface.info.index);
    }
  }

  auto v4_sockets = SetupMulticastSocketsV4(v4_index_list);
  auto v6_sockets = SetupMulticastSocketsV6(v6_index_list);

  // Listen on all interfaces
  // TODO: v6
  BoundInterfaces result;
  auto fd_it = v4_sockets.begin();
  for (int index : v4_index_list) {
    const auto& addr = *std::find_if(
        addrinfo.begin(), addrinfo.end(),
        [index](const openscreen::platform::InterfaceAddresses& addr) {
          return addr.info.index == index;
        });
    // Pick any address for the given interface.
    result.v4_interfaces.emplace_back(addr.info, addr.ipv4_addresses.front(),
                                      *fd_it++);
  }

  for (auto* socket : v4_sockets) {
    parent_->RegisterMdnsSocket(socket);
  }
  for (auto* socket : v6_sockets) {
    parent_->RegisterMdnsSocket(socket);
  }
  return result;
}

void InternalServices::InternalPlatformLinkage::DeregisterInterfaces(
    const BoundInterfaces& registered_interfaces) {
  for (const auto& interface : registered_interfaces.v4_interfaces) {
    parent_->DeregisterMdnsSocket(interface.socket);
    platform::DestroyUdpSocket(interface.socket);
  }
  for (const auto& interface : registered_interfaces.v6_interfaces) {
    parent_->DeregisterMdnsSocket(interface.socket);
    platform::DestroyUdpSocket(interface.socket);
  }
}

InternalServices::InternalServices() = default;
InternalServices::~InternalServices() = default;

void InternalServices::EnsureInternalServiceEventWaiterCreated() {
  if (internal_service_waiter_) {
    return;
  }
  internal_service_waiter_ = platform::CreateEventWaiter();
}

void InternalServices::EnsureMdnsServiceCreated() {
  EnsureInternalServiceEventWaiterCreated();
  if (mdns_service_) {
    return;
  }
  auto mdns_responder_factory = MakeUnique<MdnsResponderAdapterImplFactory>();
  auto platform = MakeUnique<InternalPlatformLinkage>(this);
  mdns_service_ = MakeUnique<MdnsResponderService>(
      kServiceType, std::move(mdns_responder_factory), std::move(platform));
}

void InternalServices::RegisterMdnsSocket(platform::UdpSocketIPv4Ptr socket) {
  platform::WatchUdpSocketIPv4Readable(internal_service_waiter_, socket);
}

void InternalServices::RegisterMdnsSocket(platform::UdpSocketIPv6Ptr socket) {
  platform::WatchUdpSocketIPv6Readable(internal_service_waiter_, socket);
}

void InternalServices::DeregisterMdnsSocket(platform::UdpSocketIPv4Ptr socket) {
  platform::StopWatchingUdpSocketIPv4Readable(internal_service_waiter_, socket);
}

void InternalServices::DeregisterMdnsSocket(platform::UdpSocketIPv6Ptr socket) {
  platform::StopWatchingUdpSocketIPv6Readable(internal_service_waiter_, socket);
}

}  // namespace openscreen
