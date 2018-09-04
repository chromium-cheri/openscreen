// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/fake_mdns_platform_service.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

TEST(FakeMdnsPlatformServiceTest, SimpleRegistration) {
  FakeMdnsPlatformService platform_service;
  auto socket = reinterpret_cast<platform::UdpSocketIPv4Ptr>(16);
  constexpr uint8_t mac[6] = {11, 22, 33, 44, 55, 66};
  MdnsPlatformService::BoundInterfaces bound_interfaces{
      {MdnsPlatformService::BoundInterfaceIPv4{
          platform::InterfaceInfo{1, mac, "eth0",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPv4Subnet{IPv4Address{192, 168, 3, 2}, 24}, socket}},
      {}};

  platform_service.set_interfaces(bound_interfaces);

  auto registered_interfaces = platform_service.RegisterInterfaces({});
  EXPECT_EQ(bound_interfaces, registered_interfaces);
  platform_service.DeregisterInterfaces(registered_interfaces);

  registered_interfaces = platform_service.RegisterInterfaces({});
  EXPECT_EQ(bound_interfaces, registered_interfaces);
  platform_service.DeregisterInterfaces(registered_interfaces);
  platform_service.set_interfaces(MdnsPlatformService::BoundInterfaces{});

  registered_interfaces = platform_service.RegisterInterfaces({});
  EXPECT_EQ((MdnsPlatformService::BoundInterfaces{}), registered_interfaces);
  platform_service.DeregisterInterfaces(registered_interfaces);

  auto new_socket = reinterpret_cast<platform::UdpSocketIPv6Ptr>(24);
  constexpr uint8_t new_mac[6] = {12, 23, 34, 45, 56, 67};
  MdnsPlatformService::BoundInterfaces new_interfaces{
      {},
      {MdnsPlatformService::BoundInterfaceIPv6{
          platform::InterfaceInfo{2, new_mac, "eth1",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPv6Subnet{
              IPv6Address{1, 2, 3, 4, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 8}, 24},
          new_socket}}};

  platform_service.set_interfaces(new_interfaces);

  registered_interfaces = platform_service.RegisterInterfaces({});
  EXPECT_EQ(new_interfaces, registered_interfaces);
  platform_service.DeregisterInterfaces(registered_interfaces);
}

TEST(FakeMdnsPlatformServiceTest, ObeyIndexWhitelist) {
  FakeMdnsPlatformService platform_service;
  auto socket1 = reinterpret_cast<platform::UdpSocketIPv4Ptr>(16);
  auto socket2 = reinterpret_cast<platform::UdpSocketIPv6Ptr>(24);
  constexpr uint8_t mac1[6] = {11, 22, 33, 44, 55, 66};
  constexpr uint8_t mac2[6] = {12, 23, 34, 45, 56, 67};
  MdnsPlatformService::BoundInterfaces bound_interfaces{
      {MdnsPlatformService::BoundInterfaceIPv4{
          platform::InterfaceInfo{1, mac1, "eth0",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPv4Subnet{IPv4Address{192, 168, 3, 2}, 24}, socket1}},
      {MdnsPlatformService::BoundInterfaceIPv6{
          platform::InterfaceInfo{2, mac2, "eth1",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPv6Subnet{
              IPv6Address{1, 2, 3, 4, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 8}, 24},
          socket2}}};
  platform_service.set_interfaces(bound_interfaces);

  auto eth0_only = platform_service.RegisterInterfaces({1});
  EXPECT_EQ((MdnsPlatformService::BoundInterfaces{
                bound_interfaces.v4_interfaces, {}}),
            eth0_only);
  platform_service.DeregisterInterfaces(eth0_only);

  auto eth1_only = platform_service.RegisterInterfaces({2});
  EXPECT_EQ((MdnsPlatformService::BoundInterfaces{
                {}, bound_interfaces.v6_interfaces}),
            eth1_only);
  platform_service.DeregisterInterfaces(eth1_only);

  auto both = platform_service.RegisterInterfaces({1, 2});
  EXPECT_EQ(bound_interfaces, both);
  platform_service.DeregisterInterfaces(both);
}

TEST(FakeMdnsPlatformServiceTest, PartialDeregister) {
  FakeMdnsPlatformService platform_service;
  auto socket1 = reinterpret_cast<platform::UdpSocketIPv4Ptr>(16);
  auto socket2 = reinterpret_cast<platform::UdpSocketIPv6Ptr>(24);
  constexpr uint8_t mac1[6] = {11, 22, 33, 44, 55, 66};
  constexpr uint8_t mac2[6] = {12, 23, 34, 45, 56, 67};
  MdnsPlatformService::BoundInterfaces bound_interfaces{
      {MdnsPlatformService::BoundInterfaceIPv4{
          platform::InterfaceInfo{1, mac1, "eth0",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPv4Subnet{IPv4Address{192, 168, 3, 2}, 24}, socket1}},
      {MdnsPlatformService::BoundInterfaceIPv6{
          platform::InterfaceInfo{2, mac2, "eth1",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPv6Subnet{
              IPv6Address{1, 2, 3, 4, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 8}, 24},
          socket2}}};
  platform_service.set_interfaces(bound_interfaces);

  auto both = platform_service.RegisterInterfaces({});
  MdnsPlatformService::BoundInterfaces eth0_only{
      {bound_interfaces.v4_interfaces[0]}, {}};
  MdnsPlatformService::BoundInterfaces eth1_only{
      {}, {bound_interfaces.v6_interfaces[0]}};
  platform_service.DeregisterInterfaces(eth0_only);
  platform_service.DeregisterInterfaces(eth1_only);
}

}  // namespace openscreen
