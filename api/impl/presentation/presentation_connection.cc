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

namespace {
using namespace openscreen;
using namespace openscreen::presentation;

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

void WriteConnectionMessage(const msgs::PresentationConnectionMessage& message,
                            ProtocolConnection* connection) {
  return WriteMessage(message, msgs::EncodePresentationConnectionMessage,
                      connection);
}

void WriteCloseMessage(const msgs::PresentationConnectionCloseEvent& message,
                       ProtocolConnection* connection) {
  return WriteMessage(message, msgs::EncodePresentationConnectionCloseEvent,
                      connection);
}

void SendCloseResponse(const msgs::PresentationConnectionCloseResponse& message,
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

namespace openscreen {
namespace presentation {

Connection::Connection(const Info& info, Role role, Delegate* delegate)
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
    std::unique_ptr<ProtocolConnection>&& protocol_connection) {
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

void Connection::OnTerminated() {
  if (state_ == State::kTerminated)
    return;
  protocol_connection_.reset();
  state_ = State::kTerminated;
  delegate_->OnTerminated();
}

void Connection::SendString(absl::string_view message) {
  if (state_ != State::kConnected)
    return;
  msgs::PresentationConnectionMessage cbor_message;
  OSP_LOG_INFO << "sending '" << message << "' to (" << presentation_.id << ", "
               << connection_id_.value() << ")";
  cbor_message.presentation_id = presentation_.id;
  cbor_message.connection_id = connection_id_.value();
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kString;

  new (&cbor_message.message.str) std::string(message);

  WriteConnectionMessage(cbor_message, protocol_connection_.get());
}

void Connection::SendBinary(std::vector<uint8_t>& data) {
  if (state_ != State::kConnected)
    return;
  msgs::PresentationConnectionMessage cbor_message;
  OSP_LOG_INFO << "sending " << data.size() << " bytes to (" << presentation_.id
               << ", " << connection_id_.value() << ")";
  cbor_message.presentation_id = presentation_.id;
  cbor_message.connection_id = connection_id_.value();
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kBytes;

  new (&cbor_message.message.bytes) std::vector<uint8_t>(data);

  WriteConnectionMessage(cbor_message, protocol_connection_.get());
}

void Connection::Close(CloseReason reason) {
  if (state_ == State::kClosed || state_ == State::kTerminated)
    return;
  state_ = State::kClosed;
  protocol_connection_.reset();
  switch (role_) {
    case Role::kController: {
      OSP_DCHECK(false) << "Controller implementation hasn't landed";
    } break;
    case Role::kReceiver: {
      // Need to open a stream pointed the other way from ours, otherwise
      // the receiver has a stream but the controller does not.
      std::unique_ptr<ProtocolConnection> stream =
          GetEndpointConnection(endpoint_id_.value());
      if (!stream)
        return;
      msgs::PresentationConnectionCloseEvent event;
      event.presentation_id = presentation_.id;
      event.connection_id = connection_id_.value();
      // TODO(btolsch): More event/request asymmetry...
      event.reason = GetEventCloseReason(reason);
      event.has_error_message = false;
      msgs::CborEncodeBuffer buffer;
      WriteCloseMessage(event, stream.get());
    } break;
  }
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
  message_watch = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionMessage, this);
  close_request_watch = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseRequest, this);
  close_response_watch = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseResponse, this);
  close_event_watch = demuxer->SetDefaultMessageTypeWatch(
      msgs::Type::kPresentationConnectionCloseEvent, this);
}

void ConnectionManager::AddConnection(Connection* connection) {
#if OSP_DCHECK_IS_ON()
  auto emplace_result =
#endif
      connections_.emplace(std::make_pair(connection->presentation_id(),
                                          connection->connection_id()),
                           ConnectionState{connection});

  OSP_DCHECK(emplace_result.second);
}

void ConnectionManager::RemoveConnection(Connection* connection) {
  auto entry = connections_.find(std::make_pair(connection->presentation_id(),
                                                connection->connection_id()));
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

      ErrorOr<ConnectionManager::ConnectionState*> connection =
          GetConnectionState(message.presentation_id, message.connection_id);
      if (!connection) {
        return connection.MoveError();
      }

#if OSP_DCHECK_IS_ON()
      // TODO(btolsch): This should probably be a more generic "group order
      // verifier" a la SequenceChecker.
      OSP_DCHECK(!connection.value()->has_stream_id ||
                 (connection.value()->message_recv_stream_id == connection_id))
          << connection_id << " "
          << ((connection.value()->has_stream_id)
                  ? connection.value()->message_recv_stream_id
                  : -1);
      connection.value()->has_stream_id = true;
      connection.value()->message_recv_stream_id = connection_id;
#endif

      switch (message.message.which) {
        case decltype(message.message.which)::kString:
          connection.value()->connection->delegate_->OnStringMessage(
              std::move(message.message.str));
          break;
        case decltype(message.message.which)::kBytes:
          connection.value()->connection->delegate_->OnBinaryMessage(
              std::move(message.message.bytes));
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
        ErrorOr<ConnectionManager::ConnectionState*> connection_state =
            GetConnectionState(request.presentation_id, request.connection_id);
        if (!connection_state) {
          return connection_state.MoveError();
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

        SendCloseResponse(
            response,
            connection_state.value()->connection->get_protocol_connection());
        // TODO(btolsch): Do we want closed/discarded/error here?
        connection_state.value()->connection->OnClosedByRemote();
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

      ErrorOr<ConnectionManager::ConnectionState*> connection =
          GetConnectionState(event.presentation_id, event.connection_id);
      if (!connection) {
        return connection.MoveError();
      }

      connection.value()->connection->OnClosedByRemote();
      return result;
    } break;

    // TODO(jophba):
    // The spec says to close the connection if we get a message we don't
    // understand. Figure out how to honor the spec here.
    default:
      return Error::Code::kUnknownMessageType;
  }
}

ErrorOr<ConnectionManager::ConnectionState*>
ConnectionManager::GetConnectionState(const std::string& presentation_id,
                                      uint64_t connection_id) {
  auto entry =
      connections_.find(std::make_pair(presentation_id, connection_id));
  if (entry != connections_.end()) {
    return &entry->second;
  }

  OSP_DVLOG << "unknown ID pair: (" << presentation_id << ", " << connection_id
            << ")";
  return Error::Code::kNoItemFound;
}

}  // namespace presentation
}  // namespace openscreen
