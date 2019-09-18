// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_SENDER_TRANSPORT_H_
#define STREAMING_CAST_SENDER_TRANSPORT_H_

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "platform/api/time.h"
#include "streaming/cast/bandwidth_estimator.h"
#include "streaming/cast/environment.h"
#include "streaming/cast/ssrc.h"
#include "util/alarm.h"

namespace openscreen {
namespace cast_streaming {

// Manages packet I/O for one or more Senders, pacing the sending of packets
// over the network, and employing network bandwidth/availabitily monitoring and
// congestion control.
class SenderTransport : public Environment::PacketConsumer {
 public:
  class Client {
   public:
    virtual ~Client();

    // Called to provide the Client with what looks like a RTCP packet meant for
    // it specifically (among other Clients) to process.
    virtual void OnReceivedRtcpPacket(absl::Span<const uint8_t> packet) = 0;

    // Returns the current round trip time, or duration::zero() if not known.
    virtual platform::Clock::duration GetCurrentRoundTripTime() = 0;

    // Populates the given |buffer| with a RTCP/RTP packet that will be sent
    // immediately. Returns the portion of |buffer| contaning the packet, or an
    // empty Span if nothing is ready to send.
    virtual absl::Span<uint8_t> GetRtcpPacketForImmediateSend(
        absl::Span<uint8_t> buffer) = 0;
    virtual absl::Span<uint8_t> GetRtpPacketForImmediateSend(
        absl::Span<uint8_t> buffer) = 0;

    // Returns the point-in-time at which RTP sending should resume, or kNever
    // if it should be suspended until an explicit call to RequestRtpSend().
    virtual platform::Clock::time_point GetRtpResumeTime() = 0;
  };

  SenderTransport(Environment* environment, int max_bitrate);
  ~SenderTransport();

  int max_packet_size() const { return packet_buffer_size_; }

  ///////BUG: Should be receiver_ssrc, but also need to replace other ssrcs in
  /// this class with Client* since those reference the client.
  void RegisterClient(Ssrc ssrc, Client* client);
  void DeregisterClient(Ssrc ssrc);

  // Requests an immediate send of a RTCP packet, and then RTCP sending will
  // repeat at regular intervals (see kRtcpSendInterval) until the Client is
  // de-registered.
  void RequestRtcpSend(Ssrc ssrc);

  // Requests an immediate send of a RTP packet. RTP sending will continue until
  // the Client stops providing packet data.
  //
  // See also: Client::GetRtpResumeTime().
  void RequestRtpSend(Ssrc ssrc);

  // Publicly-exposed API of the BandwidthEstimator owned by this transport.
  void RecordFlightPlan(int payload_bytes,
                        platform::Clock::time_point begin,
                        platform::Clock::time_point end) {
    estimator_.RecordFlightPlan(payload_bytes, begin, end);
  }
  void RecordFeedback(int payload_bytes_acknowledged,
                      platform::Clock::time_point feedback_time_minus_rtt) {
    estimator_.RecordFeedback(payload_bytes_acknowledged,
                              feedback_time_minus_rtt);
  }
  absl::optional<int> ComputeEffectiveBitrate() const {
    return estimator_.ComputeEffectiveBitrate();
  }

  // Returns the BandwidthEstimator's prediction, but capped to the maximum
  // bitrate (because SenderTransport won't attempt to send at a faster rate).
  absl::optional<int> PredictAvailableBitrate(
      platform::Clock::time_point begin,
      platform::Clock::time_point end) const;

  // A reasonable default maximum bitrate.
  static constexpr int kDefaultMaxBitrate = 24 << 20;  // 24 megabits/sec

  // A special time_point value representing "never."
  static constexpr platform::Clock::time_point kNever{
      platform::Clock::time_point::max()};

 private:
  struct ClientEntry {
    Ssrc ssrc;
    Client* client;
    platform::Clock::time_point next_rtcp_send_time;
    platform::Clock::time_point next_rtp_send_time;

    // Entries are ordered by the priority implied by their SSRC. See ssrc.h for
    // details.
    bool operator<(const ClientEntry& other) const {
      return ComparePriority(ssrc, other.ssrc) < 0;
    }
  };

  platform::Clock::time_point now() const {
    return environment_->now_function()();
  }

  // Environment::PacketConsumer implementation.
  void OnReceivedPacket(const IPEndpoint& source,
                        platform::Clock::time_point arrival_time,
                        std::vector<uint8_t> packet) final;

  // Retrieve the ClientEntry record by |ssrc|.
  ClientEntry* FindClientEntry(Ssrc ssrc);

  // Examine the next send time for all Clients, and decide whether to schedule
  // a burst-send.
  void ScheduleNextBurst();

  // Performs a burst-send of packets. This is called whevener the Alarm fires.
  void SendBurstOfPackets();

  // Send an RTCP packet from each Client that has one ready, and return the
  // number of packets sent.
  int SendJustTheRtcpPackets(platform::Clock::time_point send_time);

  // Send zero or more RTP packets from each Client, up to a maximum of
  // |num_packets_to_send|, and return the number of packets sent.
  int SendJustTheRtpPackets(platform::Clock::time_point send_time,
                            int num_packets_to_send);

  // Returns the maximum number of packets to send in one burst, based on the
  // |packet_size| in bytes and the |max_bitrate| in bits-per-second.
  static int ComputeMaxPacketsPerBurst(int packet_size, int max_bitrate);

  Environment* const environment_;
  const int packet_buffer_size_;
  const std::unique_ptr<uint8_t[]> packet_buffer_;
  const int max_bitrate_;
  const int max_packets_per_burst_;

  // Schedules tasks that call back into this SenderTransport at a later time.
  Alarm alarm_;

  // The current list of Clients and their timing information. This is
  // maintained in order of the priority implied by the Client SSRC's.
  std::vector<ClientEntry> clients_;

  // The last time a burst of packets was sent. This is used to determine the
  // next burst time.
  platform::Clock::time_point last_burst_time_{
      platform::Clock::time_point::min()};

  // Tracks recent activity, and estimates currently-effective and available
  // bitrates.
  BandwidthEstimator estimator_;

  // The minimum amount of time between burst-sends. The methodology by which
  // this value was determined is lost knowledge, but is likely the result of
  // experimentation with various network and operating system configurations.
  // This value came from the original Chromium Cast Streaming implementation.
  static constexpr std::chrono::milliseconds kPacingInterval{10};
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_SENDER_TRANSPORT_H_
