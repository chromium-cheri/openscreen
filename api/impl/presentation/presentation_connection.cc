// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_connection.h"

#include <algorithm>
#include <memory>

#include "api/impl/presentation/presentation_common.h"
#include "api/public/network_service_manager.h"
#include "api/public/presentation/presentation_controller.h"
#include "api/public/presentation/presentation_receiver.h"
#include "api/public/protocol_connection.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "third_party/abseil/src/absl/strings/string_view.h"

// NOTE: TODOs in this file fall under Issue 27.

namespace openscreen {
namespace presentation {

namespace {

msgs::PresentationConnectionCloseEvent_reason GetEventCloseReason(
    presentation::Connection::CloseReason reason) {
  switch (reason) {
    case presentation::Connection::CloseReason::kDiscarded:
      return msgs::kConnectionDestruction;

    case presentation::Connection::CloseReason::kError:
      return msgs::kUnrecoverableError;

    case presentation::Connection::CloseReason::kClosed:  // fallthrough
    default:
      return msgs::kCloseMethod;
  }
}

Error WriteConnectionMessage(const msgs::PresentationConnectionMessage& message,
                             ProtocolConnection* connection) {
  return WriteMessage(message, msgs::EncodePresentationConnectionMessage,
                      connection);
}

Error WriteCloseMessage(const msgs::PresentationConnectionCloseEvent& message,
                        ProtocolConnection* connection) {
  return WriteMessage(message, msgs::EncodePresentationConnectionCloseEvent,
                      connection);
}

Error SendCloseResponse(
    const msgs::PresentationConnectionCloseResponse& message,
    ProtocolConnection* connection) {
  return WriteMessage(message, msgs::EncodePresentationConnectionCloseResponse,
                      connection);
}

template <typename Key, typename Value>
void RemoveValueFromMap(std::map<Key, Value*>* map, Value* value) {
  for (auto it = map->begin(); it != map->end();) {
    if (it->second == value) {
      it = map->erase(it);
    } else {
      ++it;
    }
  }
}
}  // namespace

Connection::Connection(const PresentationInfo& info,
                       Role role,
                       Delegate* delegate)
    : presentation_(info),
      state_(State::kConnecting),
      delegate_(delegate),
      role_(role),
      connection_id_(0),
      protocol_connection_(nullptr) {}

Connection::~Connection() {
  if (state_ == State::kConnected) {
    Close(CloseReason::kDiscarded);
    delegate_->OnDiscarded();
  }
  if (role_ == Role::kController)
    ;  // Controller::Get()->OnConnectionDestroyed(this);
  else
    // TODO(jophba): Let the controller and receiver add themselves as
    // delegates that get informed about connection destruction, instead
    // of having the connection know about them.
    Receiver::Get()->OnConnectionDestroyed(this);
}

void Connection::OnConnected(
    uint64_t connection_id,
    uint64_t endpoint_id,
    std::unique_ptr<ProtocolConnection> protocol_connection) {
  if (state_ != State::kConnecting) {
    return;
  }
  connection_id_ = connection_id;
  endpoint_id_ = endpoint_id;
  protocol_connection_ = std::move(protocol_connection);
  state_ = State::kConnected;
  delegate_->OnConnected();
}

void Connection::OnClosedByRemote() {
  if (state_ != State::kConnecting && state_ != State::kConnected)
    return;
  protocol_connection_.reset();
  state_ = State::kClosed;
  delegate_->OnClosedByRemote();
}

void Connection::OnTerminatedByRemote() {
  if (state_ == State::kTerminated)
    return;
  protocol_connection_.reset();
  state_ = State::kTerminated;
  delegate_->OnTerminatedByRemote();
}

Error Connection::SendString(absl::string_view message) {
  if (state_ != State::kConnected)
    return Error::Code::kNoActiveConnection;

  msgs::PresentationConnectionMessage cbor_message;
  OSP_LOG_INFO << "sending '" << message << "' to (" << presentation_.id << ", "
               << connection_id_.value() << ")";
  cbor_message.presentation_id = presentation_.id;
  cbor_message.connection_id = connection_id_.value();
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kString;

  new (&cbor_message.message.str) std::string(message);

  return WriteConnectionMessage(cbor_message, protocol_connection_.get());
}

Error Connection::SendBinary(std::vector<uint8_t>& data) {
  if (state_ != State::kConnected)
    return Error::Code::kNoActiveConnection;

  msgs::PresentationConnectionMessage cbor_message;
  OSP_LOG_INFO << "sending " << data.size() << " bytes to (" << presentation_.id
               << ", " << connection_id_.value() << ")";
  cbor_message.presentation_id = presentation_.id;
  cbor_message.connection_id = connection_id_.value();
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kBytes;

  new (&cbor_message.message.bytes) std::vector<uint8_t>(data);

  return WriteConnectionMessage(cbor_message, protocol_connection_.get());
  return Error::None();
}

Error Connection::Close(CloseReason reason) {
  if (state_ == State::kClosed || state_ == State::kTerminated)
    return Error::Code::kAlreadyClosed;

  state_ = State::kClosed;
  protocol_connection_.reset();

  // TODO(jophba): switch to a strategy pattern.
  switch (role_) {
    case Role::kController:
      OSP_DCHECK(false) << "Controller implementation hasn't landed";
      return Error::Code::kNotImplemented;

    case Role::kReceiver: {
      // Need to open a stream pointed the other way from ours, otherwise
      // the receiver has a stream but the controller does not.
      std::unique_ptr<ProtocolConnection> stream =
          GetProtocolConnection(endpoint_id_.value());

      if (!stream)
        return Error::Code::kNoActiveConnection;

      msgs::PresentationConnectionCloseEvent event;
      event.presentation_id = presentation_.id;
      event.connection_id = connection_id_.value();
      // TODO(btolsch): More event/request asymmetry...
      event.reason = GetEventCloseReason(reason);
      event.has_error_message = false;
      msgs::CborEncodeBuffer buffer;
      return WriteCloseMessage(event, stream.get());
    } break;
  }

  return Error::None();
}

void Connection::Terminate(TerminationReason reason) {
  if (state_ == State::kTerminated)
    return;
  state_ = State::kTerminated;
  protocol_connection_.reset();
  switch (role_) {
    case Role::kController:
      // Controller::Get()->OnPresentationTerminated(presentation_.id, reason);
      break;
    case Role::kReceiver:
      Receiver::Get()->OnPresentationTerminated(presentation_.id, reason);
      break;
  }
}
ConnectionManager::ConnectionManager(MessageDemuxer* demuxer) {
  message_watch_ = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionMessage, this);

  close_request_watch_ = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseRequest, this);

  close_response_watch_ = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseResponse, this);

  close_event_watch_ = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseEvent, this);
}

void ConnectionManager::AddConnection(Connection* connection) {
#if OSP_DCHECK_IS_ON()
  auto emplace_result =
#endif
      connections_.emplace(
          std::make_pair(connection->get_presentation_info().id,
                         connection->connection_id()),
          connection);

  OSP_DCHECK(emplace_result.second);
}

void ConnectionManager::RemoveConnection(Connection* connection) {
  auto entry = connections_.find(std::make_pair(
      connection->get_presentation_info().id, connection->connection_id()));
  if (entry != connections_.end()) {
    connections_.erase(entry);
  }

  RemoveValueFromMap(&awaiting_close_response_, connection);
}

void ConnectionManager::AwaitCloseResponse(uint64_t request_id,
                                           Connection* connection) {
#if OSP_DCHECK_IS_ON()
  auto emplace_result =
#endif
      awaiting_close_response_.emplace(request_id, connection);
  OSP_DCHECK(emplace_result.second);
}

ErrorOr<size_t> ConnectionManager::OnStreamMessage(uint64_t endpoint_id,
                                                   uint64_t connection_id,
                                                   msgs::Type message_type,
                                                   const uint8_t* buffer,
                                                   size_t buffer_size,
                                                   platform::TimeDelta now) {
  switch (message_type) {
    case msgs::Type::kPresentationConnectionMessage: {
      msgs::PresentationConnectionMessage message;
      ssize_t result = msgs::DecodePresentationConnectionMessage(
          buffer, buffer_size, &message);
      if (result < 0) {
        OSP_LOG_WARN << "presentation-connection-message parse error";
        return Error::Code::kParseError;
      }

      Connection* connection =
          GetConnection(message.presentation_id, message.connection_id);
      if (!connection) {
        return Error::Code::kNoItemFound;
      }

      switch (message.message.which) {
        case decltype(message.message.which)::kString:
          connection->get_delegate()->OnStringMessage(message.message.str);
          break;
        case decltype(message.message.which)::kBytes:
          connection->get_delegate()->OnBinaryMessage(message.message.bytes);
          break;
        default:
          OSP_LOG_WARN << "uninitialized message data in "
                          "presentation-connection-message";
          break;
      }
      return result;
    } break;

    case msgs::Type::kPresentationConnectionCloseRequest: {
      msgs::PresentationConnectionCloseRequest request;
      ssize_t result = msgs::DecodePresentationConnectionCloseRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-connection-close-event error: "
                     << result;
      } else {
        Connection* connection =
            GetConnection(request.presentation_id, request.connection_id);
        if (!connection) {
          return Error::Code::kNoActiveConnection;
        }

        msgs::PresentationConnectionCloseResponse response;
        response.request_id = request.request_id;
        response.result =
            static_cast<decltype(response.result)>(msgs::kSuccess);
        msgs::CborEncodeBuffer buffer;
        if (!msgs::EncodePresentationConnectionCloseResponse(response,
                                                             &buffer)) {
          OSP_LOG_WARN << "encode presentation-connection-close-response error";
        }

        Error response_error =
            SendCloseResponse(response, connection->get_protocol_connection());
        if (!response_error.ok())
          return response_error;

        // TODO(btolsch): Do we want closed/discarded/error here?
        connection->OnClosedByRemote();
      }
      return result;
    } break;

    case msgs::Type::kPresentationConnectionCloseResponse: {
      msgs::PresentationConnectionCloseResponse response;
      ssize_t result = msgs::DecodePresentationConnectionCloseResponse(
          buffer, buffer_size, &response);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-connection-close-response error "
                     << result;
        return Error::Code::kParseError;
      }

      auto request_entry = awaiting_close_response_.find(response.request_id);
      if (request_entry == awaiting_close_response_.end()) {
        OSP_DVLOG << "close response for unknown request id: "
                  << response.request_id;
      }

      // TODO(jophba): notify the closer that the close was acked.
      return result;
    } break;

    case msgs::Type::kPresentationConnectionCloseEvent: {
      msgs::PresentationConnectionCloseEvent event;
      ssize_t result = msgs::DecodePresentationConnectionCloseEvent(
          buffer, buffer_size, &event);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-connection-close-event error: "
                     << result;
        return Error::Code::kParseError;
      }

      Connection* connection =
          GetConnection(event.presentation_id, event.connection_id);
      if (!connection) {
        return Error::Code::kNoActiveConnection;
      }

      connection->OnClosedByRemote();
      return result;
    } break;

    // TODO(jophba): The spec says to close the connection if we get a message
    // we don't understand. Figure out how to honor the spec here.
    default:
      return Error::Code::kUnknownMessageType;
  }
}

Connection* ConnectionManager::GetConnection(const std::string& presentation_id,
                                             uint64_t connection_id) {
  auto entry =
      connections_.find(std::make_pair(presentation_id, connection_id));
  if (entry != connections_.end()) {
    return entry->second;
  }

  OSP_DVLOG << "unknown ID pair: (" << presentation_id << ", " << connection_id
            << ")";
  return nullptr;
}

}  // namespace presentation
}  // namespace openscreen
