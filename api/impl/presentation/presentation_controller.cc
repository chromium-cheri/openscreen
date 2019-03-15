// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_controller.h"

#include <algorithm>
#include <type_traits>

#include "api/impl/presentation/url_availability_requester.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_client.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "third_party/abseil/src/absl/types/optional.h"

namespace openscreen {
namespace presentation {

template <typename T>
using MessageDecodingFunction = ssize_t (*)(const uint8_t*, size_t, T*);

template <typename T>
struct DefaultEmbeddedRequestTraits {
 public:
  static decltype(auto) request(const T& data) { return (data.request); }
  static decltype(auto) request(T& data) { return (data.request); }
};

template <
    typename RequestData,
    MessageEncodingFunction<typename RequestData::RequestMsgType> Encoder,
    MessageDecodingFunction<typename RequestData::ResponseMsgType> Decoder,
    typename RequestTraits = DefaultEmbeddedRequestTraits<RequestData>>
class RequestResponseHandler : public MessageDemuxer::MessageCallback {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnMatchedResponse(
        RequestData* request,
        typename RequestData::ResponseMsgType* response,
        uint64_t endpoint_id) = 0;
    virtual void OnError(RequestData* request, Error error) = 0;
  };

  explicit RequestResponseHandler(Delegate* delegate) : delegate_(delegate) {}
  ~RequestResponseHandler() { Reset(); }

  void Reset() {
    connection_ = nullptr;
    for (auto& message : queue_) {
      delegate_->OnError(&message.data, Error::Code::kRequestCancelled);
    }
    queue_.clear();
    for (auto& message : sent_) {
      delegate_->OnError(&message.data, Error::Code::kRequestCancelled);
    }
    sent_.clear();
    response_watch_ = MessageDemuxer::MessageWatch();
  }

  Error WriteMessage(absl::optional<uint64_t> id, RequestData* message) {
    auto& request_msg = RequestTraits::request(*message);
    if (connection_) {
      request_msg.request_id =
          NetworkServiceManager::Get()
              ->GetProtocolConnectionClient()
              ->endpoint_request_ids()
              ->GetNextRequestId(connection_->endpoint_id());
      Error result = connection_->WriteMessage(request_msg, Encoder);
      if (!result.ok()) {
        return result;
      }
      sent_.emplace_back(Msg{id, std::move(*message)});
      EnsureResponseWatch();
    } else {
      queue_.emplace_back(Msg{id, std::move(*message)});
    }
    return Error::None();
  }

  void CancelMessage(uint64_t id) {
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                [&id](const Msg& msg) { return id == msg.id; }),
                 queue_.end());
    sent_.erase(std::remove_if(sent_.begin(), sent_.end(),
                               [&id](const Msg& msg) { return id == msg.id; }),
                sent_.end());
    if (sent_.empty()) {
      response_watch_ = MessageDemuxer::MessageWatch();
    }
  }

  void SetConnection(ProtocolConnection* connection) {
    connection_ = connection;
    for (auto& message : queue_) {
      auto& request_msg = RequestTraits::request(message.data);
      request_msg.request_id =
          NetworkServiceManager::Get()
              ->GetProtocolConnectionClient()
              ->endpoint_request_ids()
              ->GetNextRequestId(connection_->endpoint_id());
      Error result = connection_->WriteMessage(request_msg, Encoder);
      if (!result.ok()) {
        delegate_->OnError(&message.data, result);
      }
      sent_.emplace_back(std::move(message));
    }
    if (!queue_.empty()) {
      EnsureResponseWatch();
    }
    queue_.clear();
  }

  // MessageDemuxer::MessageCallback overrides.
  ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  platform::TimeDelta now) override {
    if (message_type == RequestData::kResponseType) {
      typename RequestData::ResponseMsgType response;
      ssize_t result = Decoder(buffer, buffer_size, &response);
      if (result < 0) {
        return 0;
      }
      auto it =
          std::find_if(sent_.begin(), sent_.end(), [&response](const Msg& msg) {
            return RequestTraits::request(msg.data).request_id ==
                   response.request_id;
          });
      if (it != sent_.end()) {
        delegate_->OnMatchedResponse(&it->data, &response,
                                     connection_->endpoint_id());
        sent_.erase(it);
        if (sent_.empty()) {
          response_watch_ = MessageDemuxer::MessageWatch();
        }
      } else {
        OSP_LOG_WARN << "got response for unknown request id: "
                     << response.request_id;
      }
      return result;
    }
    return 0;
  }

 private:
  struct Msg {
    absl::optional<uint64_t> id;
    RequestData data;
  };

  void EnsureResponseWatch() {
    if (!response_watch_) {
      response_watch_ =
          NetworkServiceManager::Get()
              ->GetProtocolConnectionClient()
              ->message_demuxer()
              ->WatchMessageType(connection_->endpoint_id(),
                                 RequestData::kResponseType, this);
    }
  }

  ProtocolConnection* connection_ = nullptr;
  Delegate* const delegate_;
  std::vector<Msg> queue_;
  std::vector<Msg> sent_;
  MessageDemuxer::MessageWatch response_watch_;

  DISALLOW_COPY_AND_ASSIGN(RequestResponseHandler);
};

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

struct InitiationRequest {
  using RequestMsgType = msgs::PresentationInitiationRequest;
  using ResponseMsgType = msgs::PresentationInitiationResponse;

  static constexpr msgs::Type kResponseType =
      msgs::Type::kPresentationInitiationResponse;

  msgs::PresentationInitiationRequest request;
  RequestDelegate* request_delegate;
  Connection::Delegate* connection_delegate;
};

struct TerminationRequest {
  using RequestMsgType = msgs::PresentationTerminationRequest;
  using ResponseMsgType = msgs::PresentationTerminationResponse;

  static constexpr msgs::Type kResponseType =
      msgs::Type::kPresentationTerminationResponse;

  msgs::PresentationTerminationRequest request;
};

using InitiationRequestResponseHandler =
    RequestResponseHandler<InitiationRequest,
                           &msgs::EncodePresentationInitiationRequest,
                           &msgs::DecodePresentationInitiationResponse>;
using TerminationRequestResponseHandler =
    RequestResponseHandler<TerminationRequest,
                           &msgs::EncodePresentationTerminationRequest,
                           &msgs::DecodePresentationTerminationResponse>;

struct Controller::MessageGroupStreams final
    : ProtocolConnectionClient::ConnectionRequestCallback,
      ProtocolConnection::Observer,
      InitiationRequestResponseHandler::Delegate,
      TerminationRequestResponseHandler::Delegate {
  MessageGroupStreams(Controller* controller, const std::string& service_id);

  void SendInitiationRequest(uint64_t id, InitiationRequest request);
  void SendConnectionRequest();
  void SendTerminationRequest(TerminationRequest request);

  void CancelInitiationRequest(uint64_t request_id);
  void CancelConnectionRequest(uint64_t request_id);

  // ProtocolConnectionClient::ConnectionRequestCallback overrides.
  void OnConnectionOpened(
      uint64_t request_id,
      std::unique_ptr<ProtocolConnection> connection) override;
  void OnConnectionFailed(uint64_t request_id) override;

  // ProtocolConnection::Observer overrides.
  void OnConnectionClosed(const ProtocolConnection& connection) override;

  // RequestResponseHandler overrides.
  void OnMatchedResponse(InitiationRequest* request,
                         msgs::PresentationInitiationResponse* response,
                         uint64_t endpoint_id) override;
  void OnError(InitiationRequest* request, Error error) override;
  void OnMatchedResponse(TerminationRequest* request,
                         msgs::PresentationTerminationResponse* response,
                         uint64_t endpoint_id) override;
  void OnError(TerminationRequest* request, Error error) override;

  Controller* const controller;
  std::string service_id;

  ProtocolConnectionClient::ConnectRequest initiation_stream_request;
  std::unique_ptr<ProtocolConnection> initiation_stream;

  // TODO(btolsch): Improve the ergo of QuicClient::Connect because this is
  // dumb.
  bool initiation_stream_request_stack;

  InitiationRequestResponseHandler initiation_handler;
  TerminationRequestResponseHandler termination_handler;
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

Controller::MessageGroupStreams::MessageGroupStreams(
    Controller* controller,
    const std::string& service_id)
    : controller(controller),
      service_id(service_id),
      initiation_handler(this),
      termination_handler(this) {}

void Controller::MessageGroupStreams::SendInitiationRequest(
    uint64_t request_id,
    InitiationRequest request) {
  if (!initiation_stream && !initiation_stream_request) {
    initiation_stream_request_stack = true;
    initiation_stream_request =
        NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
            controller->receiver_endpoints_[service_id], this);
    initiation_stream_request_stack = false;
  }
  initiation_handler.WriteMessage(request_id, &request);
}

void Controller::MessageGroupStreams::SendConnectionRequest() {
  OSP_UNIMPLEMENTED();
}

void Controller::MessageGroupStreams::SendTerminationRequest(
    TerminationRequest request) {
  if (!initiation_stream && !initiation_stream_request) {
    initiation_stream_request =
        NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
            controller->receiver_endpoints_[service_id], this);
  }
  termination_handler.WriteMessage(absl::nullopt, &request);
}

void Controller::MessageGroupStreams::CancelInitiationRequest(
    uint64_t request_id) {
  initiation_handler.CancelMessage(request_id);
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
      initiation_stream_request_stack) {
    initiation_stream = std::move(connection);
    initiation_stream->SetObserver(this);
    initiation_stream_request.MarkComplete();
    initiation_handler.SetConnection(initiation_stream.get());
    termination_handler.SetConnection(initiation_stream.get());
  }
}

void Controller::MessageGroupStreams::OnConnectionFailed(uint64_t request_id) {
  if (initiation_stream_request &&
      initiation_stream_request.request_id() == request_id) {
    initiation_stream_request.MarkComplete();
    initiation_handler.Reset();
    termination_handler.Reset();
  }
}

void Controller::MessageGroupStreams::OnConnectionClosed(
    const ProtocolConnection& connection) {
  if (&connection == initiation_stream.get()) {
    initiation_handler.Reset();
    termination_handler.Reset();
  }
}

void Controller::MessageGroupStreams::OnMatchedResponse(
    InitiationRequest* request,
    msgs::PresentationInitiationResponse* response,
    uint64_t endpoint_id) {
  if (response->result ==
      static_cast<decltype(response->result)>(msgs::kSuccess)) {
    OSP_LOG_INFO << "presentation started for " << request->request.url;
    Controller::ControlledPresentation& presentation =
        controller->presentations_[request->request.presentation_id];
    presentation.service_id = service_id;
    presentation.url = request->request.url;
    auto connection = std::make_unique<Connection>(
        Connection::PresentationInfo{request->request.presentation_id,
                                     request->request.url},
        request->connection_delegate, controller);
    controller->OpenConnection(request->request.connection_id, endpoint_id,
                               service_id, request->request_delegate,
                               std::move(connection),
                               NetworkServiceManager::Get()
                                   ->GetProtocolConnectionClient()
                                   ->CreateProtocolConnection(endpoint_id));
  } else {
    OSP_LOG_INFO << "presentation-initiation-response for "
                 << request->request.url << " failed: " << response->result;
    request->request_delegate->OnError(Error(Error::Code::kUnknownStartError));
  }
}

void Controller::MessageGroupStreams::OnError(InitiationRequest* request,
                                              Error error) {
  request->request_delegate->OnError(error);
}

void Controller::MessageGroupStreams::OnMatchedResponse(
    TerminationRequest* request,
    msgs::PresentationTerminationResponse* response,
    uint64_t endpoint_id) {
  OSP_VLOG << "got presentation-termination-response for "
           << request->request.presentation_id;
}

void Controller::MessageGroupStreams::OnError(TerminationRequest* request,
                                              Error error) {}

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
  request.request.url = url;
  request.request.presentation_id = MakePresentationId(url, service_id);
  request.request.headers = "";
  request.request.has_connection_id = true;
  request.request.connection_id =
      GetNextConnectionId(request.request.presentation_id);
  request.request_delegate = delegate;
  request.connection_delegate = conn_delegate;
  group_streams_[service_id]->SendInitiationRequest(request_id,
                                                    std::move(request));
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
  request.request.presentation_id = presentation_id;
  request.request.reason = msgs::kUserTerminatedViaController;
  group_streams_[presentation.service_id]->SendTerminationRequest(
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
  auto group_streams =
      std::make_unique<MessageGroupStreams>(this, info.service_id);
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
