// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <vector>

#include "cast/streaming/message_port.h"
#include "cast/streaming/offer.h"
#include "cast/streaming/receiver.h"
#include "cast/streaming/receiver_packet_router.h"
#include "cast/streaming/session_config.h"
#include "util/json/json_reader.h"

namespace cast {

namespace channel {
class CastSocket;
class CastMessage;
class VirtualConnectionRouter;
class VirtualConnection;
}  // namespace channel

namespace streaming {

class ReceiverSession : public MessagePort::Client {
 public:
  // Upon successful negotiation, a set of configured receivers is constructed
  // for handling audio and video. Note that either receiver may be null.
  class ConfiguredReceivers {
   public:
    // In practice, we may have 0, 1, or 2 receivers configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a receiver configuration.
    ConfiguredReceivers(
        std::unique_ptr<Receiver> audio_receiver,
        const absl::optional<SessionConfig> audio_receiver_config,
        std::unique_ptr<Receiver> video_receiver,
        const absl::optional<SessionConfig> video_receiver_config);
    ConfiguredReceivers(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers(ConfiguredReceivers&&) noexcept;
    ConfiguredReceivers& operator=(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers& operator=(ConfiguredReceivers&&) noexcept;
    ~ConfiguredReceivers();

    // If the receiver is audio- or video-only, either of the receivers
    // may be nullptr. However, in the majority of cases they will be populated.
    Receiver* audio_receiver() const { return audio_receiver_.get(); }
    const absl::optional<SessionConfig>& audio_session_config() const {
      return audio_receiver_config_;
    }
    Receiver* video_receiver() const { return video_receiver_.get(); }
    const absl::optional<SessionConfig>& video_session_config() const {
      return video_receiver_config_;
    }

   private:
    std::unique_ptr<Receiver> audio_receiver_;
    absl::optional<SessionConfig> audio_receiver_config_;
    std::unique_ptr<Receiver> video_receiver_;
    absl::optional<SessionConfig> video_receiver_config_;
  };

  // The embedder should provide a client for handling connections.
  // When a connection is established, the OnNegotiated callback is called.
  class Client {
   public:
    virtual void OnNegotiated(ReceiverSession* session,
                              ConfiguredReceivers receivers) = 0;
    virtual void OnError(ReceiverSession* session, openscreen::Error error) = 0;
  };

  // The embedder has the option of providing a list of prioritized
  // preferences for selecting from the offer.
  // Note that embedders are required to implement the following
  // codecs to be Cast V2 compliant: H264, VP8, AAC, Opus.
  // TODO(jophba): add additional fields for preferences.
  enum AudioCodec { kAac, kOpus };
  enum VideoCodec { kH264, kVp8, kHevc, kVp9 };

  struct Preferences {
    std::vector<VideoCodec> video_codecs;
    std::vector<AudioCodec> audio_codecs;
  };

  // TODO: own messageport?
  ReceiverSession(Client* client,
                  MessagePort* message_port,
                  Preferences preferences);
  ReceiverSession(Client* client, MessagePort* message_port);
  ReceiverSession(const ReceiverSession&) = delete;
  ReceiverSession(ReceiverSession&&) noexcept;
  ReceiverSession& operator=(const ReceiverSession&) = delete;
  ReceiverSession& operator=(ReceiverSession&&) noexcept;
  ~ReceiverSession();

  // MessagePort::Client overrides
  void OnMessage(absl::string_view message) override;
  void OnError(openscreen::Error error) override;

 private:
  // Message handlers
  void OnOffer(Json::Value root, int sequence_number);

  void SelectStreams(const AudioStream* audio,
                     const VideoStream* video,
                     Offer&& offer);

  Client* client_;
  MessagePort* message_port_;
  Preferences preferences_;

  openscreen::JsonReader json_reader_ = {};
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
