// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_EVENT_WAITER_H_
#define PLATFORM_API_EVENT_WAITER_H_

#include <vector>

#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

struct EventWaiterPrivate;
using EventWaiterPtr = EventWaiterPrivate*;

struct NetworkChangeEvent {
  std::vector<InterfaceAddresses> current_addresses;
};

struct UdpSocketIPv4ReadableEvent {
  UdpSocketIPv4Ptr socket;
};

struct UdpSocketIPv6ReadableEvent {
  UdpSocketIPv6Ptr socket;
};

struct UdpSocketIPv4WritableEvent {
  UdpSocketIPv4Ptr socket;
};

struct UdpSocketIPv6WritableEvent {
  UdpSocketIPv6Ptr socket;
};

// This struct represents a set of events associated with a particular
// EventWaiterPtr and is created by WaitForEvents.  Any combination and number
// of events may be present, depending on how the platform implements event
// waiting and what has occured since the last WaitForEvents call.
struct Events {
  Events();
  ~Events();
  Events(Events&& o);
  Events& operator=(Events&& o);

  std::vector<NetworkChangeEvent> network_change_events;
  std::vector<UdpSocketIPv4ReadableEvent> udpv4_readable_events;
  std::vector<UdpSocketIPv6ReadableEvent> udpv6_readable_events;
  std::vector<UdpSocketIPv4WritableEvent> udpv4_writable_events;
  std::vector<UdpSocketIPv6WritableEvent> udpv6_writable_events;
};

EventWaiterPtr CreateEventWaiter();
void DestroyEventWaiter(EventWaiterPtr waiter);

enum class WatchResult {
  kError = -1,
  kSetConflict = 0,  // Already watching for Watch*, not watching for Stop*.
  kSuccess = 1,
};

// Returns true if |socket| was successfully added to the set of watched
// sockets, false otherwise.  It will also return false if |socket| is already
// being watched.
WatchResult WatchUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket);
WatchResult WatchUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket);

// Returns true if |socket| was successfully removed from the set of watched
// sockets.
WatchResult StopWatchingUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                              UdpSocketIPv4Ptr socket);
WatchResult StopWatchingUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                              UdpSocketIPv6Ptr socket);

// Returns true if |socket| was successfully added to the set of watched
// sockets, false otherwise.  It will also return false if |socket| is already
// being watched.
WatchResult WatchUdpSocketIPv4Writable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket);
WatchResult WatchUdpSocketIPv6Writable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket);

// Returns true if |socket| was successfully removed from the set of watched
// sockets.
WatchResult StopWatchingUdpSocketIPv4Writable(EventWaiterPtr waiter,
                                              UdpSocketIPv4Ptr socket);
WatchResult StopWatchingUdpSocketIPv6Writable(EventWaiterPtr waiter,
                                              UdpSocketIPv6Ptr socket);

// Returns true if |waiter| successfully started monitoring network change
// events, false otherwise.  It will also return false if |waiter| is already
// monitoring network change events.
WatchResult WatchNetworkChange(EventWaiterPtr waiter);

// Returns true if |waiter| successfully stopped monitoring network change
// events, false otherwise.  It will also return false if |waiter| wasn't
// monitoring network change events already.
WatchResult StopWatchingNetworkChange(EventWaiterPtr waiter);

// Returns 1 if events were added to |events|, 0 if the timeout was reached or
// there were no events on which to wait, and -1 if an error occurred.
int WaitForEvents(EventWaiterPtr waiter, TimeDelta timeout, Events* events);

}  // namespace platform
}  // namespace openscreen

#endif
