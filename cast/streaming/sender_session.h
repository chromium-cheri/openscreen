// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SENDER_SESSION_H_
#define CAST_STREAMING_SENDER_SESSION_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cast/streaming/answer_messages.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender_packet_router.h"
#include "cast/streaming/session_base.h"
#include "cast/streaming/session_config.h"
#include "json/value.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

class Environment;
class Sender;

class SenderSession final : public MessagePort::Client {
 public:
  // An audio sender, its session configuration, and its audio configuration.
  struct AudioSender {
    Sender* sender = nullptr;
    SessionConfig session_config;

    // Config originally provided by the embedder in the negotiate call.
    AudioConfig audio_config;
  };

  // A video sender, its session configuration, and its video configuration.
  struct VideoSender {
    Sender* sender = nullptr;
    SessionConfig session_config;

    // Config originally provided by the embedder in the negotiate call.
    VideoConfig video_config;
  };

  // Upon successful negotiation, a set of configured senders is constructed
  // for handling audio and video. Note that either sender may be null.
  struct ConfiguredSenders {
    // In practice, we may have 0, 1, or 2 senders configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a sender configuration.

    // If the sender is audio- or video-only, either of the senders
    // may be nullptr. However, in the majority of cases they will be populated.
    std::unique_ptr<AudioSender> audio;
    std::unique_ptr<VideoSender> video;
  };

  // The embedder should provide a client for handling connections.
  // When a connection is established, the OnNegotiated callback is called.
  class Client {
   public:
    // Called when a new set of senders has been negotiated. This may be
    // called multiple times during a session, once for every time Negotiate()
    // is called on the SenderSession object.
    virtual void OnNegotiated(const SenderSession* session,
                              ConfiguredSenders senders) = 0;

    virtual void OnError(const SenderSession* session, Error error) = 0;

   protected:
    virtual ~Client();
  };

  SenderSession(Client* const client,
                Environment* environment,
                MessagePort* message_port);
  SenderSession(const SenderSession&) = delete;
  SenderSession(SenderSession&&) = delete;
  SenderSession& operator=(const SenderSession&) = delete;
  SenderSession& operator=(SenderSession&&) = delete;
  ~SenderSession();

  // Starts an OFFER/ANSWER exchange with the already configured receiver
  // over the message port. The caller should assume any configured senders are
  // now invalid after calling this method.
  void Negotiate(std::vector<AudioConfig> audio_offers,
                 std::vector<VideoConfig> video_offers);

  // MessagePort::Client overrides
  void OnMessage(const std::string& sender_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  // TODO(jophba): refactor into message channel client?
  struct JsonMessage {
    const std::string sender_id = {};
    const std::string message_namespace = {};
    const int sequence_number = 0;
    Json::Value body;
  };

  // We store the current negotiation, so that when we get an answer from the
  // receiver we can line up the selected streams with the original
  // configuration.
  struct Negotiation {
    Offer offer;

    // Maps of the stream IDs used in the offer message to the embedder-provided
    // audio and video configs.
    std::unordered_map<int, AudioConfig> audio_configs;
    std::unordered_map<int, VideoConfig> video_configs;
  };

  // Specific message type handler methods.
  void OnAnswer(const Json::Value& message_body);

  // Used by SpawnSenders to generate a sender for a specific stream.
  std::pair<SessionConfig, std::unique_ptr<Sender>> ConstructSender(
      const Stream& stream,
      RtpPayloadType type);

  // Spawn a set of configured senders from the currently stored negotiation.
  ConfiguredSenders SpawnSenders(const Answer& answer);

  // Build an offer from the configs provided by the embedder.
  Offer ConstructOffer(const std::vector<AudioConfig>& audio_configs,
                       const std::vector<VideoConfig>& video_configs);

  // Sends a message over the message port.
  void SendMessage(JsonMessage* message);

  Client* const client_;
  Environment* const environment_;
  MessagePort* const message_port_;

  bool supports_wifi_status_reporting_ = false;
  SenderPacketRouter packet_router_;

  std::unique_ptr<Negotiation> current_negotiation_;
  std::unique_ptr<Sender> current_audio_sender_;
  std::unique_ptr<Sender> current_video_sender_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SENDER_SESSION_H_
