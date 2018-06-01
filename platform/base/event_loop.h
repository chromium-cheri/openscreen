// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_EVENT_LOOP_H_
#define PLATFORM_BASE_EVENT_LOOP_H_

#include <cstdint>
#include <vector>

#include "base/ip_address.h"
#include "platform/api/event_waiter.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

struct ReceivedDataIPv4 {
  ReceivedDataIPv4();
  ~ReceivedDataIPv4();
  ReceivedDataIPv4(ReceivedDataIPv4&& o);
  ReceivedDataIPv4& operator=(ReceivedDataIPv4&& o);

  IPv4Endpoint source;
  IPv4Endpoint original_destination;
  std::vector<uint8_t> bytes;
  UdpSocketIPv4Ptr socket;
};

struct ReceivedDataIPv6 {
  ReceivedDataIPv6();
  ~ReceivedDataIPv6();
  ReceivedDataIPv6(ReceivedDataIPv6&& o);
  ReceivedDataIPv6& operator=(ReceivedDataIPv6&& o);

  IPv6Endpoint source;
  IPv6Endpoint original_destination;
  std::vector<uint8_t> bytes;
  UdpSocketIPv6Ptr socket;
};

struct ReceivedData {
  ReceivedData();
  ~ReceivedData();
  ReceivedData(ReceivedData&& o);
  ReceivedData& operator=(ReceivedData&& o);

  std::vector<ReceivedDataIPv4> v4_data;
  std::vector<ReceivedDataIPv6> v6_data;
};

bool ReceiveDataFromIPv4Event(const UdpSocketIPv4ReadableEvent& read_event,
                              ReceivedDataIPv4* data);
bool ReceiveDataFromIPv6Event(const UdpSocketIPv6ReadableEvent& read_event,
                              ReceivedDataIPv6* data);
ReceivedData OnePlatformLoopIteration(EventWaiterPtr waiter,
                                      Milliseconds timeout);

}  // namespace platform
}  // namespace openscreen

#endif
