// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/sender_transport.h"

#include <algorithm>

#include "platform/api/logging.h"
#include "streaming/cast/constants.h"
#include "streaming/cast/packet_util.h"

namespace openscreen {
namespace cast_streaming {

decltype(SenderTransport::kPacingInterval) constexpr SenderTransport::
    kPacingInterval;

SenderTransport::SenderTransport(Environment* environment, int max_bitrate)
    : environment_(environment),
      packet_buffer_size_(environment->GetMaxPacketSize()),
      packet_buffer_(new uint8_t[packet_buffer_size_]),
      max_bitrate_(max_bitrate),
      max_packets_per_burst_(
          ComputeMaxPacketsPerBurst(packet_buffer_size_, max_bitrate_)),
      alarm_(environment_->now_function(), environment_->task_runner()) {
  OSP_DCHECK(environment_);
  OSP_DCHECK_GT(packet_buffer_size_, kRequiredNetworkPacketSize);

  environment_->ConsumeIncomingPackets(this);
}

SenderTransport::~SenderTransport() {
  environment_->DropIncomingPackets();
  OSP_DCHECK(clients_.empty());
}

void SenderTransport::RegisterClient(Ssrc ssrc, Client* client) {
  OSP_DCHECK(!FindClientEntry(ssrc));
  clients_.push_back(ClientEntry{ssrc, client, kNever, kNever});
  // Sort the list of clients so that they are iterated in priority order.
  std::sort(clients_.begin(), clients_.end());
}

void SenderTransport::DeregisterClient(Ssrc ssrc) {
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->ssrc == ssrc) {
      clients_.erase(it);
      return;
    }
  }
}

void SenderTransport::RequestRtcpSend(Ssrc ssrc) {
  ClientEntry* const entry = FindClientEntry(ssrc);
  OSP_DCHECK(entry);
  entry->next_rtcp_send_time = now();
  ScheduleNextBurst();
}

void SenderTransport::RequestRtpSend(Ssrc ssrc) {
  ClientEntry* const entry = FindClientEntry(ssrc);
  OSP_DCHECK(entry);
  entry->next_rtp_send_time = now();
  ScheduleNextBurst();
}

absl::optional<int> SenderTransport::PredictAvailableBitrate(
    platform::Clock::time_point begin,
    platform::Clock::time_point end) const {
  absl::optional<int> result = estimator_.PredictAvailableBitrate(begin, end);
  if (result && *result > max_bitrate_) {
    result = max_bitrate_;
  }
  return result;
}

void SenderTransport::OnReceivedPacket(const IPEndpoint& source,
                                       platform::Clock::time_point arrival_time,
                                       std::vector<uint8_t> packet) {
  const std::pair<ApparentPacketType, Ssrc> seems_like =
      InspectPacketForRouting(packet);
  if (seems_like.first != ApparentPacketType::RTCP) {
    return;  // Senders only receive RTCP packets.
  }
  for (ClientEntry& entry : clients_) {
    if (entry.ssrc == seems_like.second) {
      entry.client->OnReceivedRtcpPacket(packet);
      break;
    }
  }
}

SenderTransport::ClientEntry* SenderTransport::FindClientEntry(Ssrc ssrc) {
  for (ClientEntry& entry : clients_) {
    if (entry.ssrc == ssrc) {
      return &entry;
    }
  }
  return nullptr;
}

void SenderTransport::ScheduleNextBurst() {
  // Determine the next burst time by scanning for the earliest of the
  // next-scheduled send times for each client. The loop exits early if any send
  // time is before the earliest allowed burst time, at least one pacing
  // interval since the last burst.
  const platform::Clock::time_point earliest_allowed_burst_time =
      last_burst_time_ + kPacingInterval;
  platform::Clock::time_point next_burst_time = kNever;
  for (const ClientEntry& entry : clients_) {
    const auto next_send_time =
        std::min(entry.next_rtcp_send_time, entry.next_rtp_send_time);
    if (next_send_time >= next_burst_time) {
      continue;
    }
    if (next_send_time <= earliest_allowed_burst_time) {
      next_burst_time = earliest_allowed_burst_time;
      break;
    }
    next_burst_time = next_send_time;
  }

  // Schedule the alarm for the next burst time. If no client has anything to
  // send, then cancel the possibly-already-armed alarm.
  if (next_burst_time == kNever) {
    alarm_.Cancel();
    return;
  }
  alarm_.Schedule(std::bind(&SenderTransport::SendBurstOfPackets, this),
                  next_burst_time);
}

void SenderTransport::SendBurstOfPackets() {
  const platform::Clock::time_point burst_time = now();
  const int num_rtcp_packets_sent = SendJustTheRtcpPackets(burst_time);
  const int num_rtp_packets_sent = SendJustTheRtpPackets(
      burst_time, max_packets_per_burst_ - num_rtcp_packets_sent);
  last_burst_time_ = burst_time;

  if (num_rtp_packets_sent > 0) {
    estimator_.RecordActiveBurstTime(burst_time);
  } else {
    estimator_.RecordInactiveBurstTime(burst_time);
  }

  ScheduleNextBurst();
}

int SenderTransport::SendJustTheRtcpPackets(
    platform::Clock::time_point send_time) {
  int num_sent = 0;
  for (ClientEntry& entry : clients_) {
    if (entry.next_rtcp_send_time > send_time) {
      continue;
    }
    const absl::Span<uint8_t> packet =
        entry.client->GetRtcpPacketForImmediateSend(
            absl::Span<uint8_t>(packet_buffer_.get(), packet_buffer_size_));
    if (!packet.empty()) {
      environment_->SendPacket(packet);
      entry.next_rtcp_send_time = send_time + kRtcpReportInterval;
      ++num_sent;
    }
  }

  return num_sent;
}

int SenderTransport::SendJustTheRtpPackets(
    platform::Clock::time_point send_time,
    int num_packets_to_send) {
  if (num_packets_to_send <= 0) {
    return 0;
  }

  int num_sent = 0;
  for (ClientEntry& entry : clients_) {
    if (entry.next_rtp_send_time > send_time) {
      continue;
    }

    for (;;) {
      const absl::Span<uint8_t> packet =
          entry.client->GetRtpPacketForImmediateSend(
              absl::Span<uint8_t>(packet_buffer_.get(), packet_buffer_size_));
      if (packet.empty()) {
        entry.next_rtp_send_time = entry.client->GetRtpResumeTime();
        break;
      }

      environment_->SendPacket(packet);
      ++num_sent;
      if (num_sent >= num_packets_to_send) {
        return num_sent;
      }
    }
  }
  return num_sent;
}

// static
int SenderTransport::ComputeMaxPacketsPerBurst(int packet_size,
                                               int max_bitrate) {
  constexpr int kBitsPerByte = 8;
  const int packets_per_second = max_bitrate / (packet_size * kBitsPerByte);
  const auto packets_per_pacing_interval =
      (packets_per_second * kPacingInterval) / std::chrono::seconds(1);
  return std::max<int>(1, packets_per_pacing_interval);
}

SenderTransport::Client::~Client() = default;

// static
constexpr platform::Clock::time_point SenderTransport::kNever;

}  // namespace cast_streaming
}  // namespace openscreen
