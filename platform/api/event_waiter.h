// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_EVENT_WAITER_H_
#define PLATFORM_API_EVENT_WAITER_H_

#include <vector>

#include "osp_base/error.h"
#include "platform/api/socket.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

struct UdpSocketReadableEvent {
  UdpSocket* socket;
};

struct UdpSocketWritableEvent {
  UdpSocket* socket;
};

// This class functions as a platform-specific mechanism to wake the
// NetworkWaiter out of the Wait(...) loop. The instace used in NetworkWaiter
// is created via the Create() factory method, which can be implemented at
// to return a platform-specific instance.
class WakeUpHandler {
 public:
  virtual ~WakeUpHandler() = default;

  // Returns a new instance of WakeUpHandler. It is the responsibility of the
  // caller to delete this instance.
  static WakeUpHandler* Create();

  // Sets the wakeup handler to wake the NetworkWaiter from a wait loop.
  virtual void Set();

  // Clears the wakeup handler so it will not wake the network waiter.
  virtual void Clear();

 protected:
  WakeUpHandler() = default;
};

// This struct represents a set of events associated with a particular
// EventWaiterPtr and is created by WaitForEvents.  Any combination and number
// of events may be present, depending on how the platform implements event
// waiting and what has occured since the last WaitForEvents call.
// NOTE: Copy constructor is needed for Mocking in UTs.
struct Events {
  Events();
  ~Events();
  Events(const Events& o);
  Events(Events&& o);
  Events& operator=(Events&& o);

  std::vector<UdpSocketReadableEvent> udp_readable_events;
  std::vector<UdpSocketWritableEvent> udp_writable_events;
};

// Note: virtual methods are required to allow for Unit testing of all classes
// that depend on this class.
class EventWaiter {
 public:
  static EventWaiter* Create();
  virtual ~EventWaiter();

  virtual Error WatchUdpSocketReadable(UdpSocket* socket) = 0;
  virtual Error StopWatchingUdpSocketReadable(UdpSocket* socket) = 0;

  virtual Error WatchUdpSocketWritable(UdpSocket* socket) = 0;
  virtual Error StopWatchingUdpSocketWritable(UdpSocket* socket) = 0;

  virtual Error WatchNetworkChange() = 0;
  virtual Error StopWatchingNetworkChange() = 0;

  // Returns the number of events that were added to |events| if there were any,
  // 0 if there were no events, and -1 if an error occurred.
  virtual ErrorOr<Events> WaitForEvents(Clock::duration timeout) = 0;

  // Get the wake up handler associated with this EventWaiter.
  virtual WakeUpHandler* GetWakeUpHandler() = 0;

 protected:
  EventWaiter();
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_EVENT_WAITER_H_
