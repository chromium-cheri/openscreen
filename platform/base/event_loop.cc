// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/event_loop.h"

#include <utility>

#include "platform/api/logging.h"
#include "platform/api/socket.h"

namespace openscreen {
namespace platform {

Error ReceiveDataFromEvent(const SocketReadableEvent& read_event,
                           Socket::Message* data) {
  OSP_DCHECK(data);
  ErrorOr<Socket::Message> msg = read_event.socket->ReceiveMessage();

  if (!msg) {
    OSP_LOG_ERROR << "ReceiveMessage() on socket failed: "
                  << msg.error().message();
    return msg.error();
  }
  OSP_DCHECK_LE(msg.value().length, static_cast<size_t>(kMaxSocketPacketSize));

  *data = msg.MoveValue();
  return Error::None();
}

std::vector<Socket::Message> HandleSocketReadEvents(const Events& events) {
  std::vector<Socket::Message> data;
  for (const auto& read_event : events.udp_readable_events) {
    Socket::Message next_data;
    if (ReceiveDataFromEvent(read_event, &next_data).ok())
      data.emplace_back(std::move(next_data));
  }
  return data;
}

std::vector<Socket::Message> OnePlatformLoopIteration(EventWaiterPtr waiter) {
  ErrorOr<Events> events = WaitForEvents(waiter);
  if (!events)
    return {};

  return HandleSocketReadEvents(events.value());
}

}  // namespace platform
}  // namespace openscreen
