// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_DISPATCHER_H_
#define CAST_STREAMING_MESSAGE_DISPATCHER_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cast/common/public/message_port.h"
#include "cast/streaming/receiver_response.h"
#include "platform/api/time.h"
#include "util/chrono_helpers.h"

namespace openscreen {
namespace cast {

// Dispatches inbound/outbound messages. The outbound messages are sent out
// through |outbound_channel|, and the inbound messages are handled by this
// class.
class MessageDispatcher final : public MessagePort::Client {
 public:
  struct Message {
    // |destination_id| is always the other side of the message port connection:
    // the source if an incoming message, or the destination if an outgoing
    // message. The sender ID of this side of the message port is passed in
    // through the SessionMessager constructor.
    std::string destination_id = {};

    // The namespace of the message. Currently only kCastWebrtcNamespace
    // is supported--when new namespaces are added this class will have to be
    // updated.
    std::string namespace_ = {};

    // The sequence number of the message. This is important currently for
    // ensuring we reply to the proper request message, such as for OFFER/ANSWER
    // exchanges.
    int sequence_number = 0;

    // The body of the message, as a JSON object.
    Json::Value body;
  };

  using ErrorCallback = std::function<void(Error)>;
  MessageDispatcher(MessagePort* message_port, ErrorCallback error_callback);
  MessageDispatcher(const MessageDispatcher&) = delete;
  MessageDispatcher(MessageDispatcher&&);
  MessageDispatcher& operator=(const MessageDispatcher&) = delete;
  MessageDispatcher& operator=(MessageDispatcher&&);
  ~MessageDispatcher();

  // Registers/Unregisters callback for a certain type of responses.
  using ResponseCallback = std::function<void(ReceiverResponse response)>;
  void Subscribe(ResponseType type, ResponseCallback callback);
  void Unsubscribe(ResponseType type);

  // Sends the given message and subscribes to replies until an acceptable one
  // is received or a timeout elapses. Message of the given response type is
  // delivered to the supplied callback if the sequence number of the response
  // matches |sequence_number|. If the timeout period elapses, the callback will
  // be run once with an unknown type of |response|.
  // Note: Calling RequestReply() before a previous reply was made will cancel
  // the previous request and not run its response callback.
  void RequestReply(Message message,
                    ResponseType response_type,
                    int32_t sequence_number,
                    Clock::duration timeout,
                    ResponseCallback callback);

  // Get the sequence number for the next outbound message. Never returns 0.
  int32_t GetNextSeqNumber();

  // Requests to send outbound |message|.
  void SendOutboundMessage(Message message);

  // MessagePort::Client overrides
  void OnMessage(const std::string& sender_id,
                 const std::string& message_namespace,
                 const std::string& message) override;
  void OnError(Error error) override;

 private:
  class RequestHolder;

  void Send(Message message);

  MessagePort* const message_port_;

  ErrorCallback error_callback_;

  int32_t last_sequence_number_ = 0;

  // Holds callbacks for different types of responses.
  std::unordered_map<ResponseType, ResponseCallback> callback_map_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_DISPATCHER_H_
