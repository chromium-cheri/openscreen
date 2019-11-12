// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <vector>

#include "cast/streaming/receiver_config.h"
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
    ConfiguredReceivers(std::unique_ptr<Receiver> audio_receiver,
                        std::unique_ptr<ReceiverConfig> audio_receiver_config,
                        std::unique_ptr<Receiver> video_receiver,
                        std::unique_ptr<ReceiverConfig> video_receiver_config);
    ~ConfiguredReceivers();

    Receiver* GetAudioReceiver() { return audio_receiver_.get(); }
    ReceiverConfig* GetAudioReceiverConfig() {
      return audio_receiver_config_.get();
    }
    Receiver* GetVideoReceiver() { return video_receiver_.get(); }
    ReceiverConfig* GetVideoReceiverConfig() {
      return video_receiver_config_.get();
    }

   private:
    std::unique_ptr<Receiver> audio_receiver_;
    std::unique_ptr<ReceiverConfig> audio_receiver_config_;
    std::unique_ptr<Receiver> video_receiver_;
    std::unique_ptr<ReceiverConfig> video_receiver_config_;
  };

  class Client {
   public:
    virtual void OnOffer(std::vector<const ReceiverConfig> offers) = 0;
    virtual void OnNegotiated(ConfiguredReceivers receivers) = 0;
  };

  static std::unique_ptr<ReceiverSession> Create(
      Client* client,
      ReceiverPacketRouter* packet_router);

  void SelectOffer(const ReceiverConfig& selected_offer);

 private:
  ReceiverSession(Client* client, ReceiverPacketRouter* router);

  Client* client_;
  ReceiverPacketRouter* router_;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
