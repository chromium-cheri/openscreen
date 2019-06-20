// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_EVENT_WAITER_H_
#define PLATFORM_API_EVENT_WAITER_H_

#include <vector>

#include "osp_base/error.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

struct UdpSocketReadableEvent {
  UdpSocket* socket;
};

struct UdpSocketWritableEvent {
  UdpSocket* socket;
};

// Note: virtual methods are required to allow for unit testing of all classes
// that depend on this class.
class EventWaiter {
 public:
  // A interface to hide all socket-level manipulation done in this class. This
  // provides a wrapper around Socket operations so that higher-level code may
  // remain platform independent.
  class SocketHandler {
   public:
    // Creates a new instance of SocketHandler.
    static std::unique_ptr<SocketHandler> Create();

    // Virtual destructor needed to support delete call from unique_ptr.
    virtual ~SocketHandler() = default;

    // Watches the selected file handle.
    virtual void Watch(const UdpSocket& socket) = 0;

    // Determines if the given handle has changed.
    virtual bool IsChanged(const UdpSocket& socket) const = 0;

    // Clears any currently set sockets.
    virtual void Clear() = 0;
  };

  static std::unique_ptr<EventWaiter> Create();

  // Virtual destructor needed to support delete call from unique_ptr.
  virtual ~EventWaiter() = default;

  // Waits until the sockets provided to these SocketHandlers determine a read
  // or write has occured, or the provided timeout has passed.
  // NOTE: nullptr is not a valid value for the SocketHandlers objects.
  virtual Error WaitForEvents(Clock::duration timeout,
                              SocketHandler* reads,
                              SocketHandler* writes) = 0;
};

struct EventWaiterPrivate;
using EventWaiterPtr = EventWaiterPrivate*;

// This struct represents a set of events associated with a particular
// EventWaiterPtr and is created by WaitForEvents.  Any combination and number
// of events may be present, depending on how the platform implements event
// waiting and what has occured since the last WaitForEvents call.
struct Events {
  Events();
  ~Events();
  Events(Events&& o);
  Events& operator=(Events&& o);

  std::vector<UdpSocketReadableEvent> udp_readable_events;
  std::vector<UdpSocketWritableEvent> udp_writable_events;
};

// TODO(miu): This should be a std::unique_ptr<> instead of two separate
// methods, so that code structure auto-scopes the lifetime of the instance.
EventWaiterPtr CreateEventWaiter();
void DestroyEventWaiter(EventWaiterPtr waiter);

Error WatchUdpSocketReadable(EventWaiterPtr waiter, UdpSocket* socket);
Error StopWatchingUdpSocketReadable(EventWaiterPtr waiter, UdpSocket* socket);

Error WatchUdpSocketWritable(EventWaiterPtr waiter, UdpSocket* socket);
Error StopWatchingUdpSocketWritable(EventWaiterPtr waiter, UdpSocket* socket);

Error WatchNetworkChange(EventWaiterPtr waiter);
Error StopWatchingNetworkChange(EventWaiterPtr waiter);

// Returns the number of events that were added to |events| if there were any, 0
// if there were no events, and -1 if an error occurred.
ErrorOr<Events> WaitForEvents(EventWaiterPtr waiter);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_EVENT_WAITER_H_
