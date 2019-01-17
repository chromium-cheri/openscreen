// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_connection.h"

#include <algorithm>

#include "api/public/network_service_manager.h"
#include "api/public/presentation/presentation_controller.h"
#include "api/public/presentation/presentation_receiver.h"
#include "api/public/protocol_connection.h"
#include "base/make_unique.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace presentation {

Connection::Connection(const Info& info, Delegate* delegate, Role role)
    : info_(info),
      state_(State::kConnecting),
      delegate_(delegate),
      role_(role),
      connection_id_(0),
      stream_(nullptr) {}

Connection::~Connection() {
  if (state_ == State::kConnected) {
    Close(CloseReason::kDiscarded);
    delegate_->OnDiscarded();
  }
  if (role_ == Role::kController)
    ;  // Controller::Get()->OnConnectionDestroyed(this);
  else
    Receiver::Get()->OnConnectionDestroyed(this);
}

void Connection::OnConnecting() {
  if (state_ != State::kClosed)
    return;
  state_ = State::kConnecting;
}

void Connection::OnConnected(uint64_t connection_id,
                             uint64_t endpoint_id,
                             std::unique_ptr<ProtocolConnection>&& stream) {
  if (state_ != State::kConnecting) {
    return;
  }
  connection_id_ = connection_id;
  endpoint_id_ = endpoint_id;
  stream_ = std::move(stream);
  state_ = State::kConnected;
  delegate_->OnConnected();
}

void Connection::OnClosedByRemote() {
  if (state_ != State::kConnecting && state_ != State::kConnected)
    return;
  stream_.reset();
  state_ = State::kClosed;
  delegate_->OnClosedByRemote();
}

void Connection::OnTerminated() {
  if (state_ == State::kTerminated)
    return;
  stream_.reset();
  state_ = State::kTerminated;
  delegate_->OnTerminated();
}

void Connection::SendString(std::string&& message) {
  if (state_ != State::kConnected)
    return;
  msgs::PresentationConnectionMessage cbor_message;
  OSP_LOG_INFO << "sending '" << message << "' to (" << info_.id << ", "
               << connection_id_ << ")";
  cbor_message.presentation_id = info_.id;
  cbor_message.connection_id = connection_id_;
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kString;
  new (&cbor_message.message.str) std::string(std::move(message));

  msgs::CborEncodeBuffer buffer;
  if (msgs::EncodePresentationConnectionMessage(cbor_message, &buffer)) {
    stream_->Write(buffer.data(), buffer.size());
  } else {
    OSP_LOG_WARN << "failed to send connection message";
  }
}

void Connection::SendBinary(std::vector<uint8_t>&& data) {
  if (state_ != State::kConnected)
    return;
  msgs::PresentationConnectionMessage cbor_message;
  OSP_LOG_INFO << "sending " << data.size() << " bytes to (" << info_.id << ", "
               << connection_id_ << ")";
  cbor_message.presentation_id = info_.id;
  cbor_message.connection_id = connection_id_;
  cbor_message.message.which =
      msgs::PresentationConnectionMessage::Message::Which::kBytes;
  new (&cbor_message.message.bytes) std::vector<uint8_t>(std::move(data));

  msgs::CborEncodeBuffer buffer;
  if (msgs::EncodePresentationConnectionMessage(cbor_message, &buffer)) {
    stream_->Write(buffer.data(), buffer.size());
  } else {
    OSP_LOG_WARN << "failed to send connection data";
  }
}

void Connection::Close(CloseReason reason) {
  if (state_ == State::kClosed || state_ == State::kTerminated)
    return;
  state_ = State::kClosed;
  stream_.reset();
  switch (role_) {
    case Role::kController: {
      OSP_DCHECK(false) << "Controller implementation hasn't landed";
    } break;
    case Role::kReceiver: {
      std::unique_ptr<ProtocolConnection> stream =
          NetworkServiceManager::Get()
              ->GetProtocolConnectionServer()
              ->CreateProtocolConnection(endpoint_id_);
      if (!stream)
        return;
      msgs::PresentationConnectionCloseEvent event;
      event.presentation_id = info_.id;
      event.connection_id = connection_id_;
      // TODO(btolsch): More event/request asymmetry...
      switch (reason) {
        case CloseReason::kClosed:
          event.reason = msgs::kCloseMethod;
          break;
        case CloseReason::kDiscarded:
          event.reason = msgs::kConnectionDestruction;
          break;
        case CloseReason::kError:
          event.reason = msgs::kUnrecoverableError;
          break;
      }
      event.has_error_message = false;
      msgs::CborEncodeBuffer buffer;
      if (msgs::EncodePresentationConnectionCloseEvent(event, &buffer))
        stream->Write(buffer.data(), buffer.size());
      else
        OSP_LOG_WARN << "failed to encode presentation-connection-close-event";
    } break;
  }
}

void Connection::Terminate(TerminationReason reason) {
  if (state_ == State::kTerminated)
    return;
  state_ = State::kTerminated;
  stream_.reset();
  switch (role_) {
    case Role::kController:
      // Controller::Get()->OnPresentationTerminated(info_.id, reason);
      break;
    case Role::kReceiver:
      Receiver::Get()->OnPresentationTerminated(info_.id, reason);
      break;
  }
}
ConnectionManager::ConnectionManager(MessageDemuxer* demuxer)
    : demuxer_(demuxer) {
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
  auto& connection_id_map = connections_[connection->id()];
#if OSP_DCHECK_IS_ON()
  auto emplace_result =
#endif
      connection_id_map.emplace(connection->connection_id(),
                                ConnectionState{connection});
  OSP_DCHECK(emplace_result.second);
}

void ConnectionManager::RemoveConnection(Connection* connection) {
  auto connection_id_entry = connections_.find(connection->id());
  if (connection_id_entry == connections_.end())
    return;
  auto& connection_id_map = connection_id_entry->second;
  auto state_entry = connection_id_map.find(connection->connection_id());
  if (state_entry == connection_id_map.end())
    return;
  connection_id_map.erase(state_entry);
  if (connection_id_map.empty())
    connections_.erase(connection_id_entry);
  for (auto it = awaiting_close_response_.begin();
       it != awaiting_close_response_.end();) {
    if (it->second == connection)
      it = awaiting_close_response_.erase(it);
    else
      ++it;
  }
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
        return 0;
      } else {
        ConnectionState* connection =
            GetConnectionState(message.presentation_id, message.connection_id);
        if (!connection) {
          OSP_DVLOG(1) << "unknown ID pair: (" << message.presentation_id
                       << ", " << message.connection_id << ")";
          return result;
        }
#if OSP_DCHECK_IS_ON()
        // TODO(btolsch): This should probably be a more generic "group order
        // verifier" a la SequenceChecker.
        OSP_DCHECK(!connection->has_stream_id ||
                   (connection->message_recv_stream_id == connection_id))
            << connection_id << " "
            << ((connection->has_stream_id) ? connection->message_recv_stream_id
                                            : -1);
        connection->has_stream_id = true;
        connection->message_recv_stream_id = connection_id;
#endif
        switch (message.message.which) {
          case decltype(message.message.which)::kString:
            connection->connection->delegate_->OnStringMessage(
                std::move(message.message.str));
            break;
          case decltype(message.message.which)::kBytes:
            connection->connection->delegate_->OnBinaryMessage(
                std::move(message.message.bytes));
            break;
          default:
            OSP_LOG_WARN << "uninitialized message data in "
                            "presentation-connection-message";
            break;
        }
        return result;
      }
    } break;
    case msgs::Type::kPresentationConnectionCloseRequest: {
      msgs::PresentationConnectionCloseRequest request;
      ssize_t result = msgs::DecodePresentationConnectionCloseRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-connection-close-event error: "
                     << result;
      } else {
        ConnectionState* connection =
            GetConnectionState(request.presentation_id, request.connection_id);
        if (!connection) {
          OSP_DVLOG(1) << "unknown ID pair: (" << request.presentation_id
                       << ", " << request.connection_id << ")";
          return result;
        }
        // TODO(btolsch): Do we want closed/discarded/error here?
        connection->connection->OnClosedByRemote();
        msgs::PresentationConnectionCloseResponse response;
        response.request_id = request.request_id;
        response.result =
            static_cast<decltype(response.result)>(msgs::kSuccess);
        msgs::CborEncodeBuffer buffer;
        if (msgs::EncodePresentationConnectionCloseResponse(response,
                                                            &buffer)) {
          NetworkServiceManager::Get()
              ->GetProtocolConnectionServer()
              ->CreateProtocolConnection(endpoint_id)
              ->Write(buffer.data(), buffer.size());
        } else {
          OSP_LOG_WARN << "encode presentation-connection-close-response error";
        }
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
        return 0;
      }
      auto request_entry = awaiting_close_response_.find(response.request_id);
      if (request_entry == awaiting_close_response_.end()) {
        OSP_DVLOG(1) << "close response for unknown request id: "
                     << response.request_id;
        return result;
      }
      return result;
    } break;
    case msgs::Type::kPresentationConnectionCloseEvent: {
      msgs::PresentationConnectionCloseEvent event;
      ssize_t result = msgs::DecodePresentationConnectionCloseEvent(
          buffer, buffer_size, &event);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-connection-close-event error: "
                     << result;
        result = 0;
      } else {
        ConnectionState* connection =
            GetConnectionState(event.presentation_id, event.connection_id);
        if (!connection) {
          OSP_DVLOG(1) << "unknown ID pair: (" << event.presentation_id << ", "
                       << event.connection_id << ")";
          return result;
        }
        connection->connection->OnClosedByRemote();
      }
      return result;
    } break;
    default:
      break;
  }
  return 0;
}

ConnectionManager::ConnectionState* ConnectionManager::GetConnectionState(
    const std::string& presentation_id,
    uint64_t connection_id) {
  auto connection_id_entry = connections_.find(presentation_id);
  if (connection_id_entry == connections_.end())
    return nullptr;
  auto& connection_id_map = connection_id_entry->second;
  auto state_entry = connection_id_map.find(connection_id);
  if (state_entry == connection_id_map.end())
    return nullptr;
  return &state_entry->second;
}

}  // namespace presentation
}  // namespace openscreen
