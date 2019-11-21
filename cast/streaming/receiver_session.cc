// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <utility>

#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/virtual_connection.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/streaming/offer.h"
#include "util/logging.h"

namespace cast {
namespace streaming {

namespace {

const char kOfferMessageType[] = "OFFER";
// TODO(btolsch): determine what streaming local id should be.
const char kLocalId[] = "";
const char kKeyType[] = "type";

}  // namespace

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    std::unique_ptr<Receiver> audio_receiver,
    absl::optional<SessionConfig> audio_receiver_config,
    std::unique_ptr<Receiver> video_receiver,
    absl::optional<SessionConfig> video_receiver_config)
    : audio_receiver_(std::move(audio_receiver)),
      audio_receiver_config_(std::move(audio_receiver_config)),
      video_receiver_(std::move(video_receiver)),
      video_receiver_config_(std::move(video_receiver_config)) {}

ReceiverSession::ConfiguredReceivers::ConfiguredReceivers(
    ConfiguredReceivers&&) noexcept = default;
ReceiverSession::ConfiguredReceivers& ReceiverSession::ConfiguredReceivers::
operator=(ConfiguredReceivers&&) noexcept = default;
ReceiverSession::ConfiguredReceivers::~ConfiguredReceivers() = default;

ReceiverSession::ReceiverSession(
    Client* client,
    ReceiverPacketRouter* packet_router,
    channel::VirtualConnectionRouter* connection_router)
    : client_(client),
      packet_router_(packet_router),
      connection_router_(connection_router) {
  OSP_DCHECK(client_);
  OSP_DCHECK(packet_router_);
  OSP_DCHECK(connection_router_);
  connection_router_->AddHandlerForLocalId(kLocalId, this);
}

ReceiverSession::ReceiverSession(ReceiverSession&&) noexcept = default;
ReceiverSession& ReceiverSession::operator=(ReceiverSession&&) = default;
ReceiverSession::~ReceiverSession()
    [connection_router_->RemoveHandlerForLocalId(kLocalId);]

    void ReceiverSession::SelectOffer(const SessionConfig& selected_offer) {
  // TODO(jophba): implement receiver session methods.
  OSP_UNIMPLEMENTED();
}

// 1. Yes.
// 2. I'll get back to you on what local_id should be in this case because I'm
// not familiar with whether mirroring piggy-backs on the platform id or
// generates its own.
// ...
// 5. Yes.  You would directly construct a VirtualConnection from
// {message.destination_id(), message.source_id(), socket->id()}, but this might
// warrant a helper function in the future as well.

// Here's a high-level flow of how virtual connections work:
// 1. A VirtualConnection needs to be "opened/announced" before ever sending a
// message "on" it.  AFAIK this is always the sender's responsibility since the
// receiver typically just responds to queries or long-standing connections.
// 2. VirtualConnectionManager can be used on both sides to track which
// VirtualConnections are currently open.
// 3. The sender should therefore check its VirtualConnectionManager before
// sending on a particular VirtualConnection (GetConnectionData doubles as
// "HasConnection") (unless it knows apriori that it must exist, for example).
// This is the step that might get a helper function like
// "EnsureVirtualConnection" in the future, because if it's not open it just
// needs to send a "connect" message on the platform channel.  That might sound
// complicated but it's mostly just a bunch of constants and there's no ack.
// 4. As long as a VirtualConnection is open, anyone can use
// VirtualConnectionRouter::SendMessage to populate the appropriate proto fields
// + send it over the socket.
// 5. Either side can tell the other to "close" a VirtualConnection with a
// reason (i.e. normal operation or some error).

void ReceiverSession::OnMessage(channel::VirtualConnectionRouter* router,
                                channel::CastSocket* socket,
                                channel::CastMessage&& message) {
  // TODO(jophba): do I need to add to a manager?
  channel::VirtualConnection connection{message.destination_id(),
                                        message.source_id(), socket->id()};

  OSP_DCHECK(message.payload_type() == channel::PayloadType::String);
  const openscreen::ErrorOr<Json::Value> message_payload =
      json_reader_.Read(message.payload_utf8());

  // TODO(jophba): Add an error handling implementation.
  if (!message_payload) {
    return;
  }

  Json::Value& key_type = message_payload.value()[kKeyType];
  // Mirroring offer created by mirroring service:
  // void Session::CreateAndSendOffer()

  // Consumed by: CastChannelReceiver
  // https://cs.corp.google.com/eureka_internal/chromium/src/chromecast/internal/mirroring/cast_channel_receiver.cc?type=cs&g=0

  // CastChannelReceiver::OnMessage
  if (key_type.asString().compare(kOfferMessageType)) {
    OnOffer(std::move(connection), std::move(message_payload.value()));
  }
}

void ReceiverSession::OnOffer(channel::VirtualConnection connection,
                              Json::Value root) {
  // TODO: wat do connection
  openscreen::ErrorOr<Offer> offer = Offer::Parse(std::move(root));
  if (!offer) {
    return;
  }

  client_->OnOffer(std::move(offer.value()));
}

}  // namespace streaming
}  // namespace cast
