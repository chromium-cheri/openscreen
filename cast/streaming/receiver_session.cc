// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <utility>

#include "platform/api/logging.h"

namespace cast {
namespace streaming {

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    std::unique_ptr<Receiver> audio_receiver,
    std::unique_ptr<ReceiverConfig> audio_receiver_config,
    std::unique_ptr<Receiver> video_receiver,
    std::unique_ptr<ReceiverConfig> video_receiver_config)
    : audio_receiver_(std::move(audio_receiver)),
      audio_receiver_config_(std::move(audio_receiver_config)),
      video_receiver_(std::move(video_receiver)),
      video_receiver_config_(std::move(video_receiver_config)) {}

ReceiverSession::ConfiguredReceivers::~ConfiguredReceivers() = default;

// static
std::unique_ptr<ReceiverSession> ReceiverSession::Create(
    Client* client,
    ReceiverPacketRouter* packet_router) {
  return std::unique_ptr<ReceiverSession>(
      new ReceiverSession(client, packet_router));
}

void ReceiverSession::SelectOffer(const ReceiverConfig& selected_offer) {
  // TODO(jophba): implement receiver session methods.
  OSP_UNIMPLEMENTED();
}

ReceiverSession::ReceiverSession(Client* client, ReceiverPacketRouter* router)
    : client_(client), router_(router) {
  OSP_DCHECK(client_);
  OSP_DCHECK(router_);
}

}  // namespace streaming
}  // namespace cast
