// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <vector>

#include "cast/streaming/session_config.h"
#include "streaming/cast/receiver.h"
#include "streaming/cast/receiver_packet_router.h"

namespace cast {
namespace streaming {

// TODO(jophba): remove once these namespaces are merged.
using openscreen::cast_streaming::Receiver;
using openscreen::cast_streaming::ReceiverPacketRouter;

class ReceiverSession {
 public:
  class ConfiguredReceivers {
   public:
    ConfiguredReceivers(
        std::unique_ptr<Receiver> audio_receiver,
        const absl::optional<SessionConfig> audio_receiver_config,
        std::unique_ptr<Receiver> video_receiver,
        const absl::optional<SessionConfig> video_receiver_config);
    ConfiguredReceivers(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers(ConfiguredReceivers&&) = default;
    ConfiguredReceivers& operator=(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers& operator=(ConfiguredReceivers&&) = default;
    ~ConfiguredReceivers() = default;

    // If the receiver is audio- or video-only, either of the receivers
    // may be nullptr. However, in the majority of cases they will be populated.
    inline Receiver* audio_receiver() { return audio_receiver_.get(); }
    inline const absl::optional<SessionConfig>& audio_session_config() const {
      return audio_receiver_config_;
    }
    inline Receiver* video_receiver() { return video_receiver_.get(); }
    inline const absl::optional<SessionConfig>& video_session_config() const {
      return video_receiver_config_;
    }

   private:
    std::unique_ptr<Receiver> audio_receiver_;
    absl::optional<SessionConfig> audio_receiver_config_;
    std::unique_ptr<Receiver> video_receiver_;
    absl::optional<SessionConfig> video_receiver_config_;
  };

  class Client {
   public:
    virtual ~Client() = 0;
    virtual void OnOffer(std::vector<const SessionConfig> offers) = 0;
    virtual void OnNegotiated(ConfiguredReceivers receivers) = 0;
  };

  ReceiverSession(Client* client, ReceiverPacketRouter* router);
  ReceiverSession(const ReceiverSession&) = delete;
  ReceiverSession(ReceiverSession&&) = default;
  ReceiverSession& operator=(const ReceiverSession&) = delete;
  ReceiverSession& operator=(ReceiverSession) = delete;
  ~ReceiverSession() = default;

  void SelectOffer(const SessionConfig& selected_offer);

 private:
  Client* const client_;
  ReceiverPacketRouter* const router_;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
