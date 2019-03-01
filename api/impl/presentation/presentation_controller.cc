// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_controller.h"

#include <algorithm>

#include "api/impl/presentation/url_availability_requester.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_client.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace presentation {

struct Controller::TerminateListener final : MessageDemuxer::MessageCallback {
  TerminateListener(Controller* controller,
                    const std::string& presentation_id,
                    uint64_t endpoint_id);
  ~TerminateListener() override;

  // MessageDemuxer::MessageCallback overrides.
  ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  platform::TimeDelta now) override;

  Controller* const controller;
  std::string presentation_id;
  uint64_t endpoint_id;
  MessageDemuxer::MessageWatch event_watch;
};

struct Controller::InitiationRequest {
  bool SendRequest(ProtocolConnection* connection) {
    msgs::PresentationInitiationRequest request;
    request.request_id = cbor_request_id;
    request.presentation_id = presentation_id;
    request.url = url;
    request.headers = "";
    request.has_connection_id = true;
    request.connection_id = connection_id;
    if (connection
            ->WriteMessage(request, &msgs::EncodePresentationInitiationRequest)
            .ok()) {
      return true;
    } else {
      request_delegate->OnError(Error(Error::Code::kUnknownStartError));
      return false;
    }
  }

  uint64_t internal_request_id;
  uint64_t cbor_request_id;
  std::string url;
  std::string presentation_id;
  uint64_t connection_id;
  RequestDelegate* request_delegate;
  Connection::Delegate* connection_delegate;
};

struct Controller::TerminationRequest {
  bool SendRequest(ProtocolConnection* connection) {
    msgs::PresentationTerminationRequest request;
    request.request_id = cbor_request_id;
    request.presentation_id = presentation_id;
    request.reason = reason;
    if (connection
            ->WriteMessage(request, &msgs::EncodePresentationTerminationRequest)
            .ok()) {
      return true;
    } else {
      OSP_LOG_WARN << "encode presentation-termination-request error";
      return false;
    }
  }

  uint64_t cbor_request_id;
  std::string presentation_id;
  msgs::PresentationTerminationRequest_reason reason;
};

struct Controller::MessageGroupStreams final
    : ProtocolConnectionClient::ConnectionRequestCallback,
      ProtocolConnection::Observer,
      MessageDemuxer::MessageCallback {
  explicit MessageGroupStreams(Controller* controller);

  void SendOrQueueInitiationRequest(InitiationRequest request);
  void SendOrQueueConnectionRequest();
  void SendOrQueueTerminationRequest(TerminationRequest request);

  void CancelInitiationRequest(uint64_t request_id);
  void CancelConnectionRequest(uint64_t request_id);

  // ProtocolConnectionClient::ConnectionRequestCallback overrides.
  void OnConnectionOpened(
      uint64_t request_id,
      std::unique_ptr<ProtocolConnection> connection) override;
  void OnConnectionFailed(uint64_t request_id) override;

  // ProtocolConnection::Observer overrides.
  void OnConnectionClosed(const ProtocolConnection& connection) override;

  // MessasgeDemuxer::MessageCallback overrides.
  ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  platform::TimeDelta now) override;

  Controller* const controller;
  std::string service_id;
  ProtocolConnectionClient::ConnectRequest initiation_stream_request;
  std::unique_ptr<ProtocolConnection> initiation_stream;

  // TODO(btolsch): Order isn't preserved between initiation/termination
  // requests this way.  Should this use a d-union for these two?
  std::vector<InitiationRequest> queued_initiation_requests;
  std::vector<TerminationRequest> queued_termination_requests;

  std::vector<InitiationRequest> sent_initiation_requests;
  std::vector<TerminationRequest> sent_termination_requests;

  MessageDemuxer::MessageWatch initiation_response_watch;
  MessageDemuxer::MessageWatch termination_response_watch;
};

Controller::TerminateListener::TerminateListener(
    Controller* controller,
    const std::string& presentation_id,
    uint64_t endpoint_id)
    : controller(controller),
      presentation_id(presentation_id),
      endpoint_id(endpoint_id) {
  event_watch =
      NetworkServiceManager::Get()
          ->GetProtocolConnectionClient()
          ->message_demuxer()
          ->WatchMessageType(endpoint_id,
                             msgs::Type::kPresentationTerminationEvent, this);
}

Controller::TerminateListener::~TerminateListener() = default;

ErrorOr<size_t> Controller::TerminateListener::OnStreamMessage(
    uint64_t endpoint_id,
    uint64_t connection_id,
    msgs::Type message_type,
    const uint8_t* buffer,
    size_t buffer_size,
    platform::TimeDelta now) {
  switch (message_type) {
    case msgs::Type::kPresentationTerminationEvent: {
      msgs::PresentationTerminationEvent event;
      ssize_t result =
          msgs::DecodePresentationTerminationEvent(buffer, buffer_size, &event);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-termination-event error: "
                     << result;
        return 0;
      } else if (event.presentation_id != presentation_id) {
        OSP_LOG_WARN << "got presentation-termination-event for wrong id: "
                     << presentation_id << " vs. " << event.presentation_id;
        return result;
      } else {
        OSP_LOG_INFO << "termination event";
        auto presentation_entry =
            controller->presentations_.find(event.presentation_id);
        if (presentation_entry != controller->presentations_.end()) {
          for (auto* connection : presentation_entry->second.connections)
            connection->OnTerminated();
          controller->presentations_.erase(presentation_entry);
        }
        controller->terminate_listeners_.erase(event.presentation_id);
        return result;
      }
    } break;
    default:
      return 0;
  }
}

Controller::MessageGroupStreams::MessageGroupStreams(Controller* controller)
    : controller(controller) {}

void Controller::MessageGroupStreams::SendOrQueueInitiationRequest(
    InitiationRequest request) {
  if (initiation_stream) {
    request.cbor_request_id =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionClient()
            ->endpoint_request_ids()
            ->GetNextRequestId(initiation_stream->endpoint_id());
    if (request.SendRequest(initiation_stream.get())) {
      sent_initiation_requests.emplace_back(std::move(request));
      if (!initiation_response_watch) {
        initiation_response_watch =
            NetworkServiceManager::Get()
                ->GetProtocolConnectionClient()
                ->message_demuxer()
                ->WatchMessageType(initiation_stream->endpoint_id(),
                                   msgs::Type::kPresentationInitiationResponse,
                                   this);
      }
    }
  } else {
    queued_initiation_requests.emplace_back(std::move(request));
    if (!initiation_stream_request) {
      initiation_stream_request =
          NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
              controller->receiver_endpoints_[service_id], this);
    }
  }
}

void Controller::MessageGroupStreams::SendOrQueueConnectionRequest() {
  OSP_UNIMPLEMENTED();
}

void Controller::MessageGroupStreams::SendOrQueueTerminationRequest(
    TerminationRequest request) {
  if (initiation_stream) {
    request.cbor_request_id =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionClient()
            ->endpoint_request_ids()
            ->GetNextRequestId(initiation_stream->endpoint_id());
    if (request.SendRequest(initiation_stream.get())) {
      sent_termination_requests.emplace_back(std::move(request));
      if (!termination_response_watch) {
        termination_response_watch =
            NetworkServiceManager::Get()
                ->GetProtocolConnectionClient()
                ->message_demuxer()
                ->WatchMessageType(initiation_stream->endpoint_id(),
                                   msgs::Type::kPresentationTerminationResponse,
                                   this);
      }
    }
  } else {
    queued_termination_requests.emplace_back(std::move(request));
    if (!initiation_stream_request) {
      initiation_stream_request =
          NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
              controller->receiver_endpoints_[service_id], this);
    }
  }
}

void Controller::MessageGroupStreams::CancelInitiationRequest(
    uint64_t request_id) {
  sent_initiation_requests.erase(
      std::remove_if(sent_initiation_requests.begin(),
                     sent_initiation_requests.end(),
                     [request_id](const InitiationRequest& request) {
                       return request.internal_request_id == request_id;
                     }),
      sent_initiation_requests.end());
}

void Controller::MessageGroupStreams::CancelConnectionRequest(
    uint64_t request_id) {
  OSP_UNIMPLEMENTED();
}

void Controller::MessageGroupStreams::OnConnectionOpened(
    uint64_t request_id,
    std::unique_ptr<ProtocolConnection> connection) {
  if ((initiation_stream_request &&
       initiation_stream_request.request_id() == request_id) ||
      (!queued_initiation_requests.empty() ||
       !queued_termination_requests.empty())) {
    initiation_stream_request.MarkComplete();
    initiation_stream = std::move(connection);
    initiation_stream->SetObserver(this);
    ProtocolConnectionClient* protocol_client =
        NetworkServiceManager::Get()->GetProtocolConnectionClient();
    for (auto& request : queued_initiation_requests) {
      request.cbor_request_id =
          protocol_client->endpoint_request_ids()->GetNextRequestId(
              initiation_stream->endpoint_id());
      if (request.SendRequest(initiation_stream.get()))
        sent_initiation_requests.emplace_back(std::move(request));
    }
    if (!initiation_response_watch && !queued_initiation_requests.empty()) {
      initiation_response_watch =
          protocol_client->message_demuxer()->WatchMessageType(
              initiation_stream->endpoint_id(),
              msgs::Type::kPresentationInitiationResponse, this);
    }
    queued_initiation_requests.clear();

    for (auto& request : queued_termination_requests) {
      request.cbor_request_id =
          protocol_client->endpoint_request_ids()->GetNextRequestId(
              initiation_stream->endpoint_id());
      if (request.SendRequest(initiation_stream.get()))
        sent_termination_requests.emplace_back(std::move(request));
    }
    if (!termination_response_watch && !queued_termination_requests.empty()) {
      termination_response_watch =
          protocol_client->message_demuxer()->WatchMessageType(
              initiation_stream->endpoint_id(),
              msgs::Type::kPresentationTerminationResponse, this);
    }
    queued_termination_requests.clear();
  }
}

void Controller::MessageGroupStreams::OnConnectionFailed(uint64_t request_id) {
  if (initiation_stream_request &&
      initiation_stream_request.request_id() == request_id) {
    for (auto& request : queued_initiation_requests) {
      request.request_delegate->OnError(Error(Error::Code::kUnknownStartError));
    }
    queued_initiation_requests.clear();
    queued_termination_requests.clear();
  }
}

void Controller::MessageGroupStreams::OnConnectionClosed(
    const ProtocolConnection& connection) {
  if (initiation_stream && initiation_stream->id() == connection.id()) {
    initiation_stream.reset();
    for (auto& request : queued_initiation_requests) {
      request.request_delegate->OnError(Error(Error::Code::kUnknownStartError));
    }
    sent_initiation_requests.clear();
    sent_termination_requests.clear();
  }
}

ErrorOr<size_t> Controller::MessageGroupStreams::OnStreamMessage(
    uint64_t endpoint_id,
    uint64_t connection_id,
    msgs::Type message_type,
    const uint8_t* buffer,
    size_t buffer_size,
    platform::TimeDelta now) {
  switch (message_type) {
    case msgs::Type::kPresentationInitiationResponse: {
      msgs::PresentationInitiationResponse response;
      ssize_t result = msgs::DecodePresentationInitiationResponse(
          buffer, buffer_size, &response);
      if (result < 0) {
        OSP_LOG_WARN << "presentation-initiation-response parse error "
                     << result;
        return 0;
      }
      auto it = std::find_if(
          sent_initiation_requests.begin(), sent_initiation_requests.end(),
          [&response](const InitiationRequest& request) {
            return request.cbor_request_id == response.request_id;
          });
      if (it == sent_initiation_requests.end()) {
        OSP_LOG_WARN << "got initiation response for unknown request_id";
        return result;
      }
      if (response.result ==
          static_cast<decltype(response.result)>(msgs::kSuccess)) {
        OSP_LOG_INFO << "presentation started for " << it->url;
        Controller::ControlledPresentation& presentation =
            controller->presentations_[it->presentation_id];
        presentation.service_id = service_id;
        presentation.url = it->url;
        auto connection = std::make_unique<Connection>(
            Connection::PresentationInfo{it->presentation_id, it->url},
            it->connection_delegate, controller);
        controller->OpenConnection(it->connection_id, endpoint_id, service_id,
                                   it->request_delegate, std::move(connection),
                                   NetworkServiceManager::Get()
                                       ->GetProtocolConnectionClient()
                                       ->CreateProtocolConnection(endpoint_id));
      } else {
        OSP_LOG_INFO << "presentation-initiation-response for " << it->url
                     << " failed: " << response.result;
        it->request_delegate->OnError(Error(Error::Code::kUnknownStartError));
      }
      sent_initiation_requests.erase(it);
      if (sent_initiation_requests.empty())
        initiation_response_watch = MessageDemuxer::MessageWatch();
      return result;
    } break;
    case msgs::Type::kPresentationTerminationResponse: {
      msgs::PresentationTerminationResponse response;
      ssize_t result = msgs::DecodePresentationTerminationResponse(
          buffer, buffer_size, &response);
      if (result < 0) {
        OSP_LOG_WARN << "decode presentation-termination-response error: "
                     << result;
        return 0;
      }
      auto it = std::find_if(
          sent_termination_requests.begin(), sent_termination_requests.end(),
          [&response](const TerminationRequest& request) {
            return request.cbor_request_id == response.request_id;
          });
      if (it == sent_termination_requests.end()) {
        OSP_LOG_WARN << "got termination response for unknown request_id";
        return result;
      }
      OSP_VLOG << "got presentation-termination-response for "
               << it->presentation_id;
      sent_termination_requests.erase(it);
      if (sent_termination_requests.empty())
        termination_response_watch = MessageDemuxer::MessageWatch();
      return result;
    } break;
    default:
      return 0;
  }
}

Controller::ReceiverWatch::ReceiverWatch() = default;
Controller::ReceiverWatch::ReceiverWatch(const std::vector<std::string>& urls,
                                         ReceiverObserver* observer,
                                         Controller* parent)
    : urls_(urls), observer_(observer), parent_(parent) {}

Controller::ReceiverWatch::ReceiverWatch(Controller::ReceiverWatch&& other) {
  swap(*this, other);
}

Controller::ReceiverWatch::~ReceiverWatch() {
  if (observer_) {
    parent_->CancelReceiverWatch(urls_, observer_);
  }
  observer_ = nullptr;
}

Controller::ReceiverWatch& Controller::ReceiverWatch::operator=(
    Controller::ReceiverWatch other) {
  swap(*this, other);
  return *this;
}

void swap(Controller::ReceiverWatch& a, Controller::ReceiverWatch& b) {
  using std::swap;
  swap(a.urls_, b.urls_);
  swap(a.observer_, b.observer_);
  swap(a.parent_, b.parent_);
}

Controller::ConnectRequest::ConnectRequest() : request_id_(0) {}
Controller::ConnectRequest::ConnectRequest(const std::string& service_id,
                                           bool is_reconnect,
                                           uint64_t request_id,
                                           Controller* parent)
    : service_id_(service_id),
      is_reconnect_(is_reconnect),
      request_id_(request_id),
      parent_(parent) {}

Controller::ConnectRequest::ConnectRequest(ConnectRequest&& other) {
  swap(*this, other);
}

Controller::ConnectRequest::~ConnectRequest() {
  if (request_id_) {
    parent_->CancelConnectRequest(service_id_, is_reconnect_, request_id_);
  }
  request_id_ = 0;
}

Controller::ConnectRequest& Controller::ConnectRequest::operator=(
    ConnectRequest other) {
  swap(*this, other);
  return *this;
}

void swap(Controller::ConnectRequest& a, Controller::ConnectRequest& b) {
  using std::swap;
  swap(a.service_id_, b.service_id_);
  swap(a.is_reconnect_, b.is_reconnect_);
  swap(a.request_id_, b.request_id_);
  swap(a.parent_, b.parent_);
}

Controller::Controller(std::unique_ptr<Clock> clock) {
  availability_requester_ =
      std::make_unique<UrlAvailabilityRequester>(std::move(clock));
  connection_manager_ =
      std::make_unique<ConnectionManager>(NetworkServiceManager::Get()
                                              ->GetProtocolConnectionClient()
                                              ->message_demuxer());
  const std::vector<ServiceInfo>& receivers =
      NetworkServiceManager::Get()->GetMdnsServiceListener()->GetReceivers();
  for (const auto& info : receivers) {
    receiver_endpoints_.emplace(info.service_id, info.v4_endpoint.port
                                                     ? info.v4_endpoint
                                                     : info.v6_endpoint);
    availability_requester_->AddReceiver(info);
  }
  NetworkServiceManager::Get()->GetMdnsServiceListener()->AddObserver(this);
}

Controller::~Controller() {
  connection_manager_.reset();
  NetworkServiceManager::Get()->GetMdnsServiceListener()->RemoveObserver(this);
}

Controller::ReceiverWatch Controller::RegisterReceiverWatch(
    const std::vector<std::string>& urls,
    ReceiverObserver* observer) {
  availability_requester_->AddObserver(urls, observer);
  return ReceiverWatch(urls, observer, this);
}

Controller::ConnectRequest Controller::StartPresentation(
    const std::string& url,
    const std::string& service_id,
    RequestDelegate* delegate,
    Connection::Delegate* conn_delegate) {
  uint64_t request_id = GetNextInternalRequestId();
  InitiationRequest request;
  request.internal_request_id = request_id;
  request.url = url;
  request.presentation_id = MakePresentationId(url, service_id);
  request.connection_id = GetNextConnectionId(request.presentation_id);
  request.request_delegate = delegate;
  request.connection_delegate = conn_delegate;
  group_streams_[service_id]->SendOrQueueInitiationRequest(std::move(request));
  return ConnectRequest(service_id, false, request_id, this);
}

Controller::ConnectRequest Controller::ReconnectPresentation(
    const std::vector<std::string>& urls,
    const std::string& presentation_id,
    const std::string& service_id,
    RequestDelegate* delegate,
    Connection::Delegate* conn_delegate) {
  OSP_UNIMPLEMENTED();
  return ConnectRequest();
}

Controller::ConnectRequest Controller::ReconnectConnection(
    std::unique_ptr<Connection> connection,
    RequestDelegate* delegate) {
  OSP_UNIMPLEMENTED();
  return ConnectRequest();
}

Error Controller::CloseConnection(Connection* connection,
                                  Connection::CloseReason reason) {
  OSP_UNIMPLEMENTED();
  return Error::None();
}

Error Controller::OnPresentationTerminated(const std::string& presentation_id,
                                           TerminationReason reason) {
  auto presentation_entry = presentations_.find(presentation_id);
  if (presentation_entry == presentations_.end())
    return Error::Code::kNoPresentationFound;
  ControlledPresentation& presentation = presentation_entry->second;
  for (auto* connection : presentation.connections)
    connection->OnTerminated();

  TerminationRequest request;
  request.presentation_id = presentation_id;
  request.reason = msgs::kUserTerminatedViaController;
  group_streams_[presentation.service_id]->SendOrQueueTerminationRequest(
      std::move(request));
  presentations_.erase(presentation_entry);
  terminate_listeners_.erase(presentation_id);
  return Error::None();
}

void Controller::OnConnectionDestroyed(Connection* connection) {
  auto presentation_entry = presentations_.find(connection->info().id);
  if (presentation_entry == presentations_.end())
    return;

  std::vector<Connection*>& connections =
      presentation_entry->second.connections;

  connections.erase(
      std::remove(connections.begin(), connections.end(), connection),
      connections.end());

  connection_manager_->RemoveConnection(connection);
}

std::string Controller::GetServiceIdForPresentationId(
    const std::string& presentation_id) const {
  auto presentation_entry = presentations_.find(presentation_id);
  if (presentation_entry == presentations_.end())
    return "";
  return presentation_entry->second.service_id;
}

ProtocolConnection* Controller::GetConnectionRequestGroupStream(
    const std::string& service_id) {
  OSP_UNIMPLEMENTED();
  return nullptr;
}

void Controller::OnError(ServiceListenerError) {}
void Controller::OnMetrics(ServiceListener::Metrics) {}

// static
std::string Controller::MakePresentationId(const std::string& url,
                                           const std::string& service_id) {
  OSP_UNIMPLEMENTED();
  // TODO(btolsch): This is just a placeholder for the demo.
  std::string safe_id = service_id;
  for (auto& c : safe_id)
    if (c < ' ' || c > '~')
      c = '.';
  return safe_id + ":" + url;
}

// TODO(btolsch): This should be per-endpoint since spec now omits presentation
// ID in many places.
uint64_t Controller::GetNextConnectionId(const std::string& id) {
  return next_connection_id_[id]++;
}

uint64_t Controller::GetNextInternalRequestId() {
  return ++next_internal_request_id_;
}

void Controller::OpenConnection(uint64_t connection_id,
                                uint64_t endpoint_id,
                                const std::string& service_id,
                                RequestDelegate* request_delegate,
                                std::unique_ptr<Connection>&& connection,
                                std::unique_ptr<ProtocolConnection>&& stream) {
  connection->OnConnected(connection_id, endpoint_id, std::move(stream));
  const std::string& presentation_id = connection->info().id;
  auto presentation_entry = presentations_.find(presentation_id);
  if (presentation_entry == presentations_.end()) {
    auto emplace_entry = presentations_.emplace(
        presentation_id,
        ControlledPresentation{service_id, connection->info().url, {}});
    presentation_entry = emplace_entry.first;
  }
  ControlledPresentation& presentation = presentation_entry->second;
  presentation.connections.push_back(connection.get());
  connection_manager_->AddConnection(connection.get());

  auto terminate_entry = terminate_listeners_.find(presentation_id);
  if (terminate_entry == terminate_listeners_.end()) {
    terminate_listeners_.emplace(presentation_id,
                                 std::make_unique<TerminateListener>(
                                     this, presentation_id, endpoint_id));
  }
  request_delegate->OnConnection(std::move(connection));
}

void Controller::CancelReceiverWatch(const std::vector<std::string>& urls,
                                     ReceiverObserver* observer) {
  availability_requester_->RemoveObserverUrls(urls, observer);
}

void Controller::CancelConnectRequest(const std::string& service_id,
                                      bool is_reconnect,
                                      uint64_t request_id) {
  auto group_streams_entry = group_streams_.find(service_id);
  if (group_streams_entry == group_streams_.end())
    return;
  group_streams_entry->second->CancelInitiationRequest(request_id);
}

void Controller::OnStarted() {}
void Controller::OnStopped() {}
void Controller::OnSuspended() {}
void Controller::OnSearching() {}

void Controller::OnReceiverAdded(const ServiceInfo& info) {
  receiver_endpoints_.emplace(info.service_id, info.v4_endpoint.port
                                                   ? info.v4_endpoint
                                                   : info.v6_endpoint);
  auto group_streams = std::make_unique<MessageGroupStreams>(this);
  group_streams->service_id = info.service_id;
  group_streams_[info.service_id] = std::move(group_streams);
  availability_requester_->AddReceiver(info);
}

void Controller::OnReceiverChanged(const ServiceInfo& info) {
  receiver_endpoints_[info.service_id] =
      info.v4_endpoint.port ? info.v4_endpoint : info.v6_endpoint;
  availability_requester_->ChangeReceiver(info);
}

void Controller::OnReceiverRemoved(const ServiceInfo& info) {
  receiver_endpoints_.erase(info.service_id);
  group_streams_.erase(info.service_id);
  availability_requester_->RemoveReceiver(info);
}

void Controller::OnAllReceiversRemoved() {
  receiver_endpoints_.clear();
  availability_requester_->RemoveAllReceivers();
}

}  // namespace presentation
}  // namespace openscreen
