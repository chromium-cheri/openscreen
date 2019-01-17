// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_receiver.h"

#include <algorithm>
#include <memory>

#include "api/impl/presentation/presentation_common.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_server.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"

namespace openscreen {
namespace presentation {
namespace {

void WritePresentationInitiationResponse(
    const msgs::PresentationInitiationResponse& response,
    ProtocolConnection* connection) {
  WriteMessage(response, msgs::EncodePresentationInitiationResponse,
               connection);
}

void WritePresentationConnectionOpenResponse(
    const msgs::PresentationConnectionOpenResponse& response,
    ProtocolConnection* connection) {
  WriteMessage(response, msgs::EncodePresentationConnectionOpenResponse,
               connection);
}

void WritePresentationTerminationResponse(
    const msgs::PresentationTerminationResponse& response,
    ProtocolConnection* connection) {
  WriteMessage(response, msgs::EncodePresentationTerminationResponse,
               connection);
}

void WritePresentationUrlAvailabilityResponse(
    const msgs::PresentationUrlAvailabilityResponse& response,
    ProtocolConnection* connection) {
  WriteMessage(response, msgs::EncodePresentationUrlAvailabilityResponse,
               connection);
}
}  // namespace

ErrorOr<size_t> Receiver::OnStreamMessage(uint64_t endpoint_id,
                                          uint64_t connection_id,
                                          msgs::Type message_type,
                                          const uint8_t* buffer,
                                          size_t buffer_size,
                                          platform::TimeDelta now) {
  switch (message_type) {
    case msgs::Type::kPresentationUrlAvailabilityRequest: {
      OSP_VLOG(1) << "got presentation-url-availability-request";
      msgs::PresentationUrlAvailabilityRequest request;
      ssize_t decode_result = msgs::DecodePresentationUrlAvailabilityRequest(
          buffer, buffer_size, &request);
      if (decode_result < 0) {
        OSP_LOG_WARN << "presentation-url-availability-request parse error: "
                     << decode_result;
        return Error::Code::kParseError;
      }

      msgs::PresentationUrlAvailabilityResponse response;
      response.request_id = request.request_id;

      // TODO(jophba): properly fill these fields--we currently don't
      // have any meaningful values.
      const uint64_t client_id = 0UL;
      const uint64_t request_duration = 0UL;
      response.url_availabilities = delegate_->OnUrlAvailabilityRequest(
          client_id, request_duration, std::move(request.urls));
      msgs::CborEncodeBuffer buffer;

      WritePresentationUrlAvailabilityResponse(
          response, GetEndpointConnection(endpoint_id).get());
      return decode_result;
    }

    case msgs::Type::kPresentationInitiationRequest: {
      OSP_VLOG(1) << "got presentation-initiation-request";
      msgs::PresentationInitiationRequest request;
      const ssize_t result = msgs::DecodePresentationInitiationRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "presentation-initiation-request parse error: "
                     << result;
        return Error::Code::kParseError;
      }

      OSP_LOG_INFO << "got initiation request for: " << request.url;

      auto emplace_result = queued_initiation_responses_.emplace(
          request.presentation_id,
         QueuedResponse{GetEndpointConnection(endpoint_id),
                             request.request_id, request.connection_id,
                             endpoint_id});

      if (!emplace_result.second) {
        msgs::PresentationInitiationResponse response;
        response.request_id = request.request_id;
        response.result = static_cast<decltype(response.result)>(
            msgs::kInvalidPresentationId);
        WritePresentationInitiationResponse(
            response, GetEndpointConnection(endpoint_id).get());
        return result;
      }

      const bool starting = delegate_->StartPresentation(
          Connection::Info{request.presentation_id, request.url}, endpoint_id,
          request.headers);

      if (starting)
        return result;

      queued_initiation_responses_.erase(request.presentation_id);
      msgs::PresentationInitiationResponse response;
      response.request_id = request.request_id;
      response.result =
          static_cast<decltype(response.result)>(msgs::kUnknownError);
      WritePresentationInitiationResponse(
          response, GetEndpointConnection(endpoint_id).get());
      return result;
    }

    case msgs::Type::kPresentationConnectionOpenRequest: {
      OSP_VLOG(1) << "got presentation-connection-open-request";
      msgs::PresentationConnectionOpenRequest request;
      const ssize_t result = msgs::DecodePresentationConnectionOpenRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "presentation-connection-open-request parse error: "
                     << result;
        return Error::Code::kParseError;
      }

      if (presentations_.find(request.presentation_id) ==
          presentations_.end()) {
        msgs::PresentationConnectionOpenResponse response;
        response.request_id = request.request_id;
        response.result = static_cast<decltype(response.result)>(
            msgs::kUnknownPresentationId);
        WritePresentationConnectionOpenResponse(
            response, GetEndpointConnection(endpoint_id).get());
        return result;
      }
      // TODO(btolsch): We would also check that connection_id isn't already
      // requested/in use but since the spec has already shifted to a
      // receiver-chosen connection ID, we'll ignore that until we change our
      // CDDL messages.
      std::vector<QueuedResponse>& responses =
          queued_connection_responses_[request.presentation_id];
      responses.emplace_back(QueuedResponse{
          GetEndpointConnection(endpoint_id), request.request_id,
          request.connection_id, endpoint_id});
      bool connecting = delegate_->ConnectToPresentation(
          request.request_id, request.presentation_id, endpoint_id);
      if (connecting)
        return result;

      responses.erase(responses.end() - 1);
      if (responses.empty())
        queued_connection_responses_.erase(request.presentation_id);
      msgs::PresentationConnectionOpenResponse response;
      response.request_id = request.request_id;
      response.result =
          static_cast<decltype(response.result)>(msgs::kUnknownError);
      WritePresentationConnectionOpenResponse(
          response, GetEndpointConnection(endpoint_id).get());
      return result;
    }

    case msgs::Type::kPresentationTerminationRequest: {
      OSP_VLOG(1) << "got presentation-termination-open-request";
      msgs::PresentationTerminationRequest request;
      const ssize_t result = msgs::DecodePresentationTerminationRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "presentation-termination-request parse error: "
                     << result;
        return Error::Code::kParseError;
      }

      OSP_LOG_INFO << "got termination request for: "
                   << request.presentation_id;
      auto presentation_entry = presentations_.find(request.presentation_id);
      if (presentation_entry == presentations_.end()) {
        msgs::PresentationTerminationResponse response;
        response.request_id = request.request_id;
        response.result = static_cast<decltype(response.result)>(
            msgs::kInvalidPresentationId);
        WritePresentationTerminationResponse(
            response, GetEndpointConnection(endpoint_id).get());
        return result;
      }

      TerminationReason reason =
          (request.reason == msgs::kTerminatedByController)
              ? TerminationReason::kControllerTerminateCalled
              : TerminationReason::kControllerUserTerminated;
      presentation_entry->second.terminate_request_id = request.request_id;
      delegate_->TerminatePresentation(request.presentation_id, reason);

      return result;
    }

    default:
      return Error::Code::kUnknownMessageType;
  }
}

// static
Receiver* Receiver::Get() {
  static Receiver& receiver = *new Receiver();
  return &receiver;
}

void Receiver::Init() {
  if (!connection_manager_) {
    connection_manager_ =
        std::make_unique<ConnectionManager>(NetworkServiceManager::Get()
                                                ->GetProtocolConnectionServer()
                                                ->message_demuxer());
  }
}

void Receiver::Deinit() {
  connection_manager_.reset();
}

void Receiver::SetReceiverDelegate(ReceiverDelegate* delegate) {
  OSP_DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;
  if (delegate_) {
    MessageDemuxer* demuxer = NetworkServiceManager::Get()
                                  ->GetProtocolConnectionServer()
                                  ->message_demuxer();
    availability_watch_ = demuxer->SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationUrlAvailabilityRequest, this);
    initiation_watch_ = demuxer->SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationInitiationRequest, this);
    connection_watch_ = demuxer->SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationConnectionOpenRequest, this);
  } else {
    availability_watch_ = MessageDemuxer::MessageWatch();
    initiation_watch_ = MessageDemuxer::MessageWatch();
    connection_watch_ = MessageDemuxer::MessageWatch();
    size_t total_presentations = presentations_.size();
    for (size_t i = 0, j = 0; i < total_presentations; ++i) {
      auto it = presentations_.begin();
      std::advance(it, j);
      auto& presentation = *it;
      size_t size_before = presentations_.size();
      OnPresentationTerminated(presentation.first,
                               TerminationReason::kReceiverShuttingDown);
      size_t size_after = presentations_.size();
      j += (size_before - size_after);
    }
  }
}

void Receiver::OnUrlAvailabilityUpdate(
    uint64_t client_id,
    const std::vector<msgs::PresentationUrlAvailability>& availabilities) {}

void Receiver::OnPresentationStarted(const std::string& presentation_id,
                                     Connection* connection,
                                     ResponseResult result) {
  auto it = queued_initiation_responses_.find(presentation_id);
  if (it == queued_initiation_responses_.end())
    return;
 QueuedResponse& initiation_response = it->second;
  msgs::PresentationInitiationResponse response;
  response.request_id = initiation_response.request_id;
  ProtocolConnection* stream = initiation_response.stream.get();
  OSP_VLOG(1) << "presentation started with stream id: " << stream->id();
  if (result == ResponseResult::kSuccess) {
    response.result = static_cast<decltype(response.result)>(msgs::kSuccess);
    response.has_connection_result = true;
    response.connection_result =
        static_cast<decltype(response.connection_result)>(msgs::kSuccess);
    presentations_[presentation_id].endpoint_id =
        initiation_response.endpoint_id;
    connection->OnConnected(initiation_response.connection_id,
                            initiation_response.endpoint_id,
                            std::move(initiation_response.stream));
    presentations_[presentation_id].connections.push_back(connection);
    connection_manager_->AddConnection(connection);

    presentations_[presentation_id].terminate_watch =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionServer()
            ->message_demuxer()
            ->WatchMessageType(initiation_response.endpoint_id,
                               msgs::Type::kPresentationTerminationRequest,
                               this);
  } else {
    response.result =
        static_cast<decltype(response.result)>(msgs::kUnknownError);
  }

  WritePresentationInitiationResponse(response, stream);
  queued_initiation_responses_.erase(it);
}

void Receiver::OnConnectionCreated(uint64_t request_id,
                                   Connection* connection,
                                   ResponseResult result) {
  auto entry = queued_connection_responses_.find(connection->presentation_id());
  if (entry == queued_connection_responses_.end()) {
    OSP_LOG_WARN << "connection created for unknown request";
    return;
  }
  std::vector<QueuedResponse>& responses = entry->second;
  auto it = std::find_if(responses.begin(), responses.end(),
                         [request_id](const QueuedResponse& response) {
                           return response.request_id == request_id;
                         });
  if (it == responses.end()) {
    OSP_LOG_WARN << "connection created for unknown request";
    return;
  }
  QueuedResponse& connection_response = *it;
  connection->OnConnected(
      connection_response.connection_id, connection_response.endpoint_id,
      GetEndpointConnection(connection_response.endpoint_id));
  presentations_[connection->presentation_id()].connections.push_back(
      connection);
  connection_manager_->AddConnection(connection);

  msgs::PresentationConnectionOpenResponse response;
  response.request_id = request_id;
  response.result = static_cast<decltype(response.result)>(msgs::kSuccess);
  WritePresentationConnectionOpenResponse(response,
                                          connection_response.stream.get());
  responses.erase(
      std::remove_if(responses.begin(), responses.end(),
                     [request_id](const QueuedResponse& response) {
                       return response.request_id == request_id;
                     }));
  if (responses.empty())
    queued_connection_responses_.erase(entry);
}

void Receiver::OnPresentationTerminated(const std::string& presentation_id,
                                        TerminationReason reason) {
  auto presentation_entry = presentations_.find(presentation_id);
  if (presentation_entry == presentations_.end())
    return;
  Presentation& presentation = presentation_entry->second;
  presentation.terminate_watch = MessageDemuxer::MessageWatch();
  std::unique_ptr<ProtocolConnection> stream =
      GetEndpointConnection(presentation.endpoint_id);
  if (!stream)
    return;

  for (auto* connection : presentation.connections)
    connection->OnTerminated();
  if (presentation.terminate_request_id) {
    // TODO(btolsch): Also timeout if this point isn't reached.
    msgs::PresentationTerminationResponse response;
    response.request_id = presentation.terminate_request_id;
    response.result = static_cast<decltype(response.result)>(msgs::kSuccess);
    WritePresentationTerminationResponse(response, stream.get());
    presentations_.erase(presentation_entry);
  } else {
    // TODO(btolsch): Same request/event question as connection-close.
    msgs::PresentationTerminationEvent event;
    event.presentation_id = presentation_id;
    switch (reason) {
      case TerminationReason::kReceiverUserTerminated:
        event.reason = msgs::kUserViaReceiver;
        break;
      case TerminationReason::kReceiverTerminateCalled:
        event.reason = msgs::kTerminate;
        break;
      case TerminationReason::kReceiverShuttingDown:
        event.reason = msgs::kReceiverShuttingDown;
        break;
      case TerminationReason::kReceiverPresentationUnloaded:
        event.reason = msgs::kUnloaded;
        break;
      case TerminationReason::kReceiverPresentationReplaced:
        event.reason = msgs::kNewReplacingCurrent;
        break;
      case TerminationReason::kReceiverIdleTooLong:
        event.reason = msgs::kIdleTooLong;
        break;
      case TerminationReason::kReceiverError:
        event.reason = msgs::kReceiver;
        break;
      default:
        return;
    }
    msgs::CborEncodeBuffer buffer;
    if (msgs::EncodePresentationTerminationEvent(event, &buffer)) {
      stream->Write(buffer.data(), buffer.size());
    } else {
      OSP_LOG_WARN << "encode presentation-termination-event error";
      return;
    }
    presentations_.erase(presentation_entry);
  }
}

void Receiver::OnConnectionDestroyed(Connection* connection) {
  auto presentation_entry = presentations_.find(connection->presentation_id());
  if (presentation_entry == presentations_.end())
    return;
  Presentation& presentation = presentation_entry->second;
  presentation.connections.erase(
      std::remove(presentation.connections.begin(),
                  presentation.connections.end(), connection),
      presentation.connections.end());
  connection_manager_->RemoveConnection(connection);
}

Receiver::Receiver() = default;

Receiver::~Receiver() = default;

}  // namespace presentation
}  // namespace openscreen
