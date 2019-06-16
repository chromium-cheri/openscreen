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

struct EventWaiterPrivate;
using EventWaiterPtr = EventWaiterPrivate*;

struct SocketReadableEvent {
  Socket* socket;
};

struct SocketWritableEvent {
  Socket* socket;
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

  std::vector<SocketReadableEvent> udp_readable_events;
  std::vector<SocketWritableEvent> udp_writable_events;
};

// TODO(miu): This should be a std::unique_ptr<> instead of two separate
// methods, so that code structure auto-scopes the lifetime of the instance.
EventWaiterPtr CreateEventWaiter();
void DestroyEventWaiter(EventWaiterPtr waiter);

Error WatchSocketReadable(EventWaiterPtr waiter, Socket* socket);
Error StopWatchingSocketReadable(EventWaiterPtr waiter, Socket* socket);

Error WatchSocketWritable(EventWaiterPtr waiter, Socket* socket);
Error StopWatchingSocketWritable(EventWaiterPtr waiter, Socket* socket);

Error WatchNetworkChange(EventWaiterPtr waiter);
Error StopWatchingNetworkChange(EventWaiterPtr waiter);

// Returns the number of events that were added to |events| if there were any, 0
// if there were no events, and -1 if an error occurred.
ErrorOr<Events> WaitForEvents(EventWaiterPtr waiter);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_EVENT_WAITER_H_
