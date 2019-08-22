// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RECEIVER_TRANSPORT_H_
#define STREAMING_CAST_RECEIVER_TRANSPORT_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "streaming/cast/environment.h"
#include "streaming/cast/ssrc.h"

namespace openscreen {
namespace cast_streaming {

// Handles all network I/O among multiple Receivers meant for synchronized
// play-out (e.g., one Receiver for audio, one Receiver for video). Incoming
// traffic is dispatched to the appropriate Receiver, based on its SSRC. Also,
// all traffic not coming from the same source is filtered-out.
class ReceiverTransport : public Environment::PacketConsumer {
 public:
  class Client {
   public:
    virtual ~Client() = 0;

    // Called to provide the Client with what looks like a RTP packet meant for
    // it specifically (among other Clients) to process.
    virtual void OnReceivedRtpPacket(
        absl::Span<const uint8_t> packet,
        platform::Clock::time_point arrival_time) = 0;

    // Called to provide the Client with what looks like a RTCP packet meant for
    // it specifically (among other Clients) to process.
    virtual void OnReceivedRtcpPacket(
        absl::Span<const uint8_t> packet,
        platform::Clock::time_point arrival_time) = 0;
  };

  explicit ReceiverTransport(Environment* environment);
  ~ReceiverTransport();

  // Register/Deregister a Client instance expecting RTP/RTCP packets destined
  // for the given |ssrc|.
  void RegisterClient(Ssrc ssrc, Client* client);
  void DeregisterClient(Ssrc ssrc);

  // Send a RTCP packet back to the source from which earlier packets were
  // received, or does nothing if OnReceivedPacket() has not been called yet.
  void SendRtcpPacket(absl::Span<const uint8_t> packet);

 private:
  // Environment::PacketConsumer implementation.
  void OnReceivedPacket(absl::Span<const uint8_t> packet,
                        const IPEndpoint& source,
                        platform::Clock::time_point arrival_time) final;

  // Helper to return the registered Client having the given |ssrc|, or nullptr
  // if not found.
  Client* FindClient(Ssrc ssrc);

  Environment* const environment_;

  std::vector<std::pair<Ssrc, Client*>> clients_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RECEIVER_TRANSPORT_H_
