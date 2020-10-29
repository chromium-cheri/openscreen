// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/message_dispatcher.h"

// #include "absl/strings/ascii.h"
// #include "cast/common/public/message_port.h"
// #include "cast/streaming/message_fields.h"
// #include "util/json/json_helpers.h"
// #include "util/json/json_serialization.h"
// #include "util/osp_logging.h"

// namespace openscreen {
// namespace cast {

// MessageDispatcher::MessageDispatcher(MessageDispatcher&&) = default;
// MessageDispatcher& MessageDispatcher::operator=(MessageDispatcher&&) =
// default;

// // Holds a request until |timeout| elapses or an acceptable response is
// // received. When timeout, |response_callback| runs with an UNKNOWN type
// // response.
// class MessageDispatcher::RequestHolder {
//  public:
//   RequestHolder() = default;
//   RequestHolder(const RequestHolder&) = default;
//   RequestHolder(RequestHolder&&) = default;
//   RequestHolder& operator=(const RequestHolder&) = default;
//   RequestHolder& operator=(RequestHolder&&) = default;
//   ~RequestHolder() = default;

//   void Start(Clock::duration timeout,
//              int32_t sequence_number,
//              ResponseCallback response_callback) {
//     response_callback_ = std::move(response_callback);
//     sequence_number_ = sequence_number;
//     OSP_DCHECK(!response_callback_.is_null());
//     timer_.Start(
//         FROM_HERE, timeout,
//         base::BindRepeating(&RequestHolder::SendResponse,
//                             base::Unretained(this), ReceiverResponse()));
//   }

//   // Send |response| if the sequence number matches, or if the request times
//   // out, in which case the |response| is UNKNOWN type.
//   void SendResponse(const ReceiverResponse& response) {
//     if (!timer_.IsRunning() || response.sequence_number() ==
//     sequence_number_)
//       std::move(response_callback_).Run(response);
//     // Ignore the response with mismatched sequence number.
//   }

//  private:
//   ResponseCallback response_callback_;
//   base::OneShotTimer timer_;
//   int32_t sequence_number_ = -1;

// };

// MessageDispatcher::MessageDispatcher(
//     MessagePort* message_port,
//     ErrorCallback error_callback)
//     : message_port_(message_port),
//       error_callback_(std::move(error_callback)) {
//   OSP_DCHECK(message_port_);
//   OSP_DCHECK(error_callback_);
// }

// MessageDispatcher::~MessageDispatcher() {
//   // Prevent the re-entrant operations on |callback_map_|.
//   decltype(callback_map_) subscriptions;
//   subscriptions.swap(callback_map_);
//   subscriptions.clear();
// }

// void MessageDispatcher::Send(Message message) {
//   // TODO(crbug.com/1117673): Add MR-internals logging:
//   // VLOG(2) << "Inbound message received: ns=" << message->message_namespace
//   //         << ", data=" << message->json_format_data;

//   if (message.namespace_ != kCastWebrtcNamespace &&
//       message.namespace_ != kCastRemotingNamespace) {
//     OSP_DVLOG << "Ignoring message with unknown namespace = "
//              << message.namespace_;
//     return;  // Ignore message with wrong namespace.
//   }
//   if (message.body.isNull()) {
//     return;  // Ignore null message.
//   }

//   auto response = ReceiverResponse::Parse(message.body);
//   if (!response) {
//     error_callback_("Response parsing error. message=" +
//                         message.body);
//     return;
//   }

// #if OSP_DCHECK_IS_ON()
//   if (response->type() == ResponseType::RPC)
//     OSP_DCHECK_EQ(mojom::kRemotingNamespace, message->message_namespace);
//   else
//     OSP_DCHECK_EQ(mojom::kWebRtcNamespace, message->message_namespace);
// #endif  // OSP_DCHECK_IS_ON()

//   const auto callback_iter = callback_map_.find(response->type());
//   if (callback_iter == callback_map_.end()) {
//     ErrorOr<std::string> body_or_error = json::Stringify(message.body);
//     std::string stringified_body = body_or_error ? body_or_error.value() :
//     "invalid"; error_callback_(
//       Error::Code::kParameterNullPointer, "No callback subscribed. message="
//       + stringified_body);
//     return;
//   }
//   callback_iter->second(*response);
// }

// void MessageDispatcher::Subscribe(ResponseType type,
//                                   ResponseCallback callback) {
//   OSP_DCHECK(type != ResponseType::UNKNOWN);
//   OSP_DCHECK(callback);

//   const auto insert_result =
//       callback_map_.emplace(std::make_pair(type, std::move(callback)));
//   OSP_DCHECK(insert_result.second);
// }

// void MessageDispatcher::Unsubscribe(ResponseType type) {
//   auto iter = callback_map_.find(type);
//   if (iter != callback_map_.end())
//     callback_map_.erase(iter);
// }

// int32_t MessageDispatcher::GetNextSeqNumber() {
//   // Skip 0, which is used by Cast receiver to indicate that the broadcast
//   // status message is not coming from a specific sender (it is an autonomous
//   // status change, not triggered by a command from any sender). Strange
//   usage
//   // of 0 though; could be a null / optional field.
//   return ++last_sequence_number_;
// }

// void MessageDispatcher::SendOutboundMessage(Message message) {
//   auto body_or_error = json::Stringify(message.body);
//   if (body_or_error.is_value()) {
//     OSP_DVLOG << "Sending message: SENDER[" << message.destination_id
//               << "], NAMESPACE[" << message.namespace_ << "], BODY:\n"
//               << body_or_error.value();
//     message_port_->PostMessage(message.destination_id, message.namespace_,
//                                body_or_error.value());
//   } else {
//     OSP_DLOG_WARN << "Sending message failed with error:\n"
//                   << body_or_error.error();

//     error_callback_(body_or_error.error());
//   }
// }

// void MessageDispatcher::RequestReply(Message message,
//                                      ResponseType response_type,
//                                      int32_t sequence_number,
//                                      Clock::duration timeout,
//                                      ResponseCallback callback) {
//   OSP_DCHECK(callback);
//   OSP_DCHECK(timeout.count() > 0);

//   Unsubscribe(response_type);  // Cancel the old request if there is any.
//   RequestHolder* const request_holder = new RequestHolder();
//   // request_holder->Start(
//   //     timeout, sequence_number,
//   //     base::BindOnce(
//   //         [](MessageDispatcher* dispatcher, ResponseType response_type,
//   //            OnceResponseCallback callback, const ReceiverResponse&
//   response) {
//   //           dispatcher->Unsubscribe(response_type);
//   //           std::move(callback).Run(response);
//   //         },
//   //         this, response_type, std::move(callback)));
//   // // |request_holder| keeps alive until the callback is unsubscribed.
//   // Subscribe(response_type, base::BindRepeating(
//   //                              [](RequestHolder* request_holder,
//   //                                 const ReceiverResponse& response) {
//   //                                request_holder->SendResponse(response);
//   //                              },
//   //                              base::Owned(request_holder)));
//   SendOutboundMessage(std::move(message));
// }

// }  // namespace cast
// }  // namespae openscreen
