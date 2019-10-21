// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_H_
#define CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_H_

#include <array>
#include <cstdint>
#include <string>

#include "cast/common/channel/proto/cast_channel.pb.h"

namespace cast {
namespace channel {

// Transport system on top of CastSocket that allows routing messages over a
// single socket to different device endpoints (e.g. system messages vs.
// messages for a particular app).
struct VirtualConnection {
  enum class Type {
    // Normal user connections.
    kStrong,

    // Same as strong except if the connected endpoint is an app, it may stop if
    // its only remaining open connections are all weak.
    kWeak,

    // Apps do not receive connected/disconnected notifications about these
    // connections.  The following additional conditions apply:
    //  - Receiver app can still receive "urn:x-cast:com.google.cast.media"
    //    messages over invisible connections.
    //  - Receiver app can only send broadcast messages over an invisible
    //    connection.
    kInvisible,
  };

  enum CloseReason {
    // Underlying socket has been closed by peer. This happens when cast sender
    // closed transport connection normally without graceful virtual connection
    // close. Though it is not an error, graceful virtual connection in advance
    // is better.
    kTransportClosed,

    // Underlying socket has been aborted by peer. Peer is no longer reachable
    // because of app crash or network error.
    kTransportAborted,

    // Messages sent from peer are in wrong format or too long.
    kTransportInvalidMessage,

    // Underlying socket has been idle for a long period. This only happens when
    // heartbeat is enabled and there is a network error.
    kTransportTooLongInactive,

    // The virtual connection has been closed by this endpoint.
    kClosedBySelf,

    // The virtual connection has been closed by the peer gracefully.
    kClosedByPeer,

    kUnknown,
  };

  struct AssociatedData {
    Type type;
    std::string user_agent;
    std::array<uint8_t, 2> ip_fragment;
    CastMessage::ProtocolVersion max_protocol_version;
  };

  std::string local_id;
  std::string peer_id;
  uint32_t socket_id;
};

bool operator==(const VirtualConnection& a, const VirtualConnection& b);
bool operator!=(const VirtualConnection& a, const VirtualConnection& b);

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_H_
