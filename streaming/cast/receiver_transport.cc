// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver_transport.h"

#include "platform/api/logging.h"
#include "streaming/cast/packet_util.h"

namespace openscreen {
namespace cast_streaming {

ReceiverTransport::ReceiverTransport(Environment* environment)
    : environment_(environment) {
  OSP_DCHECK(environment_);
}

ReceiverTransport::~ReceiverTransport() {
  OSP_DCHECK(clients_.empty());
}

void ReceiverTransport::RegisterClient(Ssrc ssrc, Client* client) {
  OSP_DCHECK(!FindClient(ssrc));
  clients_.emplace_back(ssrc, client);

  // If there were no registered Clients before, resume receiving packets for
  // dispath. Reset/Clear the remote endpoint, in preparation for later setting
  // it to the source of the first packet received.
  if (clients_.size() == 1) {
    environment_->set_remote_endpoint(IPEndpoint{});
    environment_->ResumeIncomingPackets(this);
  }
}

void ReceiverTransport::DeregisterClient(Ssrc ssrc) {
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->first == ssrc) {
      clients_.erase(it);

      // If there are no longer any Clients, suspend receiving packets.
      if (clients_.empty()) {
        environment_->SuspendIncomingPackets();
      }

      return;
    }
  }
}

void ReceiverTransport::SendRtcpPacket(absl::Span<const uint8_t> packet) {
  OSP_DCHECK(InspectPacketForRouting(packet).first == ApparentPacketType::RTCP);

  // Do not proceed until the remote endpoint is known. See OnReceivedPacket().
  if (environment_->remote_endpoint().port == 0) {
    return;
  }

  environment_->SendPacket(packet);
}

void ReceiverTransport::OnReceivedPacket(
    absl::Span<const uint8_t> packet,
    const IPEndpoint& source,
    platform::Clock::time_point arrival_time) {
  // If the sender endpoint is known, ignore any packet that did not come from
  // that same endpoint.
  if (environment_->remote_endpoint().port != 0) {
    if (source != environment_->remote_endpoint()) {
      return;
    }
  }

  const std::pair<ApparentPacketType, Ssrc> seems_like =
      InspectPacketForRouting(packet);
  if (seems_like.first == ApparentPacketType::UNKNOWN) {
    return;
  }
  if (Client* client = FindClient(seems_like.second)) {
    // At this point, a valid packet has been matched with a client. Lock-in the
    // remote endpoint as the |source| of this |packet| so that only packets
    // from the same source are permitted from here onwards.
    if (environment_->remote_endpoint().port == 0) {
      environment_->set_remote_endpoint(source);
    }

    if (seems_like.first == ApparentPacketType::RTP) {
      client->OnReceivedRtpPacket(packet, arrival_time);
    } else if (seems_like.first == ApparentPacketType::RTCP) {
      client->OnReceivedRtcpPacket(packet, arrival_time);
    }
  }
}

ReceiverTransport::Client* ReceiverTransport::FindClient(Ssrc ssrc) {
  for (const std::pair<Ssrc, Client*>& entry : clients_) {
    if (entry.first == ssrc) {
      return entry.second;
    }
  }
  return nullptr;
}

ReceiverTransport::Client::~Client() = default;

}  // namespace cast_streaming
}  // namespace openscreen
