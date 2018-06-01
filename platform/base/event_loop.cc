// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/event_loop.h"

#include "platform/api/error.h"
#include "platform/api/logging.h"
#include "platform/api/socket.h"

namespace openscreen {
namespace platform {

ReceivedDataIPv4::ReceivedDataIPv4() = default;
ReceivedDataIPv4::~ReceivedDataIPv4() = default;
ReceivedDataIPv4::ReceivedDataIPv4(ReceivedDataIPv4&& o) = default;
ReceivedDataIPv4& ReceivedDataIPv4::operator=(ReceivedDataIPv4&& o) = default;

ReceivedDataIPv6::ReceivedDataIPv6() = default;
ReceivedDataIPv6::~ReceivedDataIPv6() = default;
ReceivedDataIPv6::ReceivedDataIPv6(ReceivedDataIPv6&& o) = default;
ReceivedDataIPv6& ReceivedDataIPv6::operator=(ReceivedDataIPv6&& o) = default;

ReceivedData::ReceivedData() = default;
ReceivedData::~ReceivedData() = default;
ReceivedData::ReceivedData(ReceivedData&& o) = default;
ReceivedData& ReceivedData::operator=(ReceivedData&& o) = default;

bool ReceiveDataFromIPv4Event(const UdpSocketIPv4ReadableEvent& read_event,
                              ReceivedDataIPv4* data) {
  DCHECK(data);
  uint8_t buf[1024];
  int64_t len = ReceiveUdpIPv4(read_event.socket, buf, sizeof(buf),
                               &data->source, &data->original_destination);
  if (len == -1) {
    LOG_ERROR << "recv() failed: " << GetLastErrorString();
    return false;
  } else if (len == 0) {
    LOG_WARN << "recv() = 0, closed?";
    return false;
  }
  data->bytes = std::vector<uint8_t>(buf, buf + len);
  return true;
}

bool ReceiveDataFromIPv6Event(const UdpSocketIPv6ReadableEvent& read_event,
                              ReceivedDataIPv6* data) {
  return false;
}

ReceivedData OnePlatformLoopIteration(EventWaiterPtr waiter,
                                      Milliseconds timeout) {
  EventMap event_map;
  if (!WaitForEvents({waiter}, timeout, &event_map)) {
    return {};
  }
  auto event_entry = event_map.find(waiter);
  DCHECK(event_entry != end(event_map));
  const auto& events = event_entry->second;
  ReceivedData data;
  for (const auto& read_event : events.udpv4_readable_events) {
    ReceivedDataIPv4 next_data;
    if (ReceiveDataFromIPv4Event(read_event, &next_data)) {
      data.v4_data.emplace_back(std::move(next_data));
    }
  }
  for (const auto& read_event : events.udpv6_readable_events) {
    ReceivedDataIPv6 next_data;
    if (ReceiveDataFromIPv6Event(read_event, &next_data)) {
      data.v6_data.emplace_back(std::move(next_data));
    }
  }
  return data;
}

}  // namespace platform
}  // namespace openscreen
