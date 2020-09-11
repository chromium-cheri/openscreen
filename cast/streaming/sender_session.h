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
#include "cast/streaming/capture_options.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/sender.h"
#include "cast/streaming/sender_packet_router.h"
#include "cast/streaming/session_config.h"
#include "json/value.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace capture_recommendations {
struct Recommendations;
}

class Environment;
class Sender;

// Upon successful negotiation, a set of configured senders is constructed
// for handling audio and video. Note that either sender may be null.
struct ConfiguredSenders {
  // In practice, we may have 0, 1, or 2 senders configured, depending
  // on if the device supports audio and video, and if we were able to
  // successfully negotiate a sender configuration.

  // If the sender is audio- or video-only, either of the senders
  // may be nullptr. However, in the majority of cases they will be populated.
  Sender* audio;
  const AudioCaptureOption* audio_config = nullptr;

  Sender* video;
  const VideoCaptureOption* video_config = nullptr;
};

class SenderSession final : public MessagePort::Client {
 public:
  // The embedder should provide a client for handling the negotiation.
  // When the negotiation is complete, the OnNegotiated callback is called.
  class Client {
   public:
    // Called when a new set of senders has been negotiated. This may be
    // called multiple times during a session, once for every time Negotiate()
    // is called on the SenderSession object. The negotation call also includes
    // capture recommendations that can be used by the sender to provide
    // an optimal video stream for the receiver.
    virtual void OnNegotiated(
        const SenderSession* session,
        ConfiguredSenders senders,
        capture_recommendations::Recommendations capture_recommendations) = 0;

    // Called whenever an error occurs. Ends the ongoing session, and the caller
    // must call Negotiate() again.
    virtual void OnError(const SenderSession* session, Error error) = 0;

   protected:
    virtual ~Client();
  };

  // The SenderSession assumes that the passed in client, environment, and
  // message port persist for at least the lifetime of the SenderSession. If
  // one of these classes needs to be reset, a new SenderSession should be
  // created.
  SenderSession(std::string sender_id,
                IPAddress remote_address,
                Client* const client,
                Environment* environment,
                MessagePort* message_port);
  SenderSession(const SenderSession&) = delete;
  SenderSession(SenderSession&&) = delete;
  SenderSession& operator=(const SenderSession&) = delete;
  SenderSession& operator=(SenderSession&&) = delete;
  ~SenderSession();

  // Starts an OFFER/ANSWER exchange with the already configured receiver
  // over the message port. The caller should assume any configured senders are
  // become invalid when calling this method.
  Error Negotiate(std::vector<AudioCaptureOption> audio_configs,
                  std::vector<VideoCaptureOption> video_configs);

  // MessagePort::Client overrides
  void OnMessage(const std::string& sender_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  struct Message {
    std::string sender_id;
    std::string message_namespace;
    int sequence_number = 0;
    Json::Value body;
  };

  // We store the current negotiation, so that when we get an answer from the
  // receiver we can line up the selected streams with the original
  // configuration.
  struct Negotiation {
    Offer offer;

    std::vector<AudioCaptureOption> audio_configs;
    std::vector<VideoCaptureOption> video_configs;
  };

  // Specific message type handler methods.
  void OnAnswer(const Json::Value& message_body);

  // Used by SpawnSenders to generate a sender for a specific stream.
  std::unique_ptr<Sender> CreateSender(Ssrc receiver_ssrc,
                                       const Stream& stream,
                                       RtpPayloadType type);

  // Spawn a set of configured senders from the currently stored negotiation.
  ConfiguredSenders SpawnSenders(const Answer& answer);

  // Sends a message over the message port.
  void SendMessage(Message* message);

  // The unique sender_id used for all cast messaging.
  const std::string sender_id_;
  std::string receiver_sender_id_;

  // The remote address of the receiver we are communicating with. Used
  // for both TLS and UDP traffic.
  const IPAddress remote_address_;

  // The embedder is expected to provide us a client for notifications about
  // negotiations and errors, a valid cast environment, and a messaging
  // port for communicating to the Receiver over TLS.
  Client* const client_;
  Environment* const environment_;
  MessagePort* const message_port_;

  // The packet router used for messaging across all senders.
  SenderPacketRouter packet_router_;

  // Each negotiation has its own sequence number, and the receiver replies
  // with the same sequence number that we send. Each message to the receiver
  // advances our current sequence number.
  int current_sequence_number_ = 0;

  // The current negotiation. If present, we are expected an ANSWER from
  // the receiver. If not present, any provided ANSWERS are rejected.
  std::unique_ptr<Negotiation> current_negotiation_;

  // If the negotiation has succeeded, we store the current audio and video
  // senders used for this session. Either or both may be nullptr.
  std::unique_ptr<Sender> current_audio_sender_;
  std::unique_ptr<Sender> current_video_sender_;
};  // namespace cast

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SENDER_SESSION_H_
