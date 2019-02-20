// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "api/public/mdns_service_listener_factory.h"
#include "api/public/mdns_service_publisher_factory.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/presentation/presentation_controller.h"
#include "api/public/presentation/presentation_receiver.h"
#include "api/public/protocol_connection_client.h"
#include "api/public/protocol_connection_client_factory.h"
#include "api/public/protocol_connection_server.h"
#include "api/public/protocol_connection_server_factory.h"
#include "api/public/service_listener.h"
#include "api/public/service_publisher.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"
#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen {
namespace {

bool g_done = false;
bool g_dump_services = false;

void sigusr1_dump_services(int) {
  g_dump_services = true;
}

void sigint_stop(int) {
  OSP_LOG_INFO << "caught SIGINT, exiting...";
  g_done = true;
}

void SignalThings() {
  struct sigaction usr1_sa;
  struct sigaction int_sa;
  struct sigaction unused;

  usr1_sa.sa_handler = &sigusr1_dump_services;
  sigemptyset(&usr1_sa.sa_mask);
  usr1_sa.sa_flags = 0;

  int_sa.sa_handler = &sigint_stop;
  sigemptyset(&int_sa.sa_mask);
  int_sa.sa_flags = 0;

  sigaction(SIGUSR1, &usr1_sa, &unused);
  sigaction(SIGINT, &int_sa, &unused);

  OSP_LOG_INFO << "signal handlers setup";
  OSP_LOG_INFO << "pid: " << getpid();
}

class AutoMessage final
    : public ProtocolConnectionClient::ConnectionRequestCallback {
 public:
  ~AutoMessage() override = default;

  void TakeRequest(ProtocolConnectionClient::ConnectRequest&& request) {
    request_ = std::move(request);
  }

  void OnConnectionOpened(
      uint64_t request_id,
      std::unique_ptr<ProtocolConnection>&& connection) override {
    request_ = ProtocolConnectionClient::ConnectRequest();
    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.connection_id = 0;
    message.presentation_id = "presentation-id-foo";
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from client");
    if (msgs::EncodePresentationConnectionMessage(message, &buffer))
      connection->Write(buffer.data(), buffer.size());
    connection->CloseWriteEnd();
  }

  void OnConnectionFailed(uint64_t request_id) override {
    request_ = ProtocolConnectionClient::ConnectRequest();
  }

 private:
  ProtocolConnectionClient::ConnectRequest request_;
};

class ListenerObserver final : public ServiceListener::Observer {
 public:
  ~ListenerObserver() override = default;
  void OnStarted() override { OSP_LOG_INFO << "listener started!"; }
  void OnStopped() override { OSP_LOG_INFO << "listener stopped!"; }
  void OnSuspended() override { OSP_LOG_INFO << "listener suspended!"; }
  void OnSearching() override { OSP_LOG_INFO << "listener searching!"; }

  void OnReceiverAdded(const ServiceInfo& info) override {
    OSP_LOG_INFO << "found! " << info.friendly_name;
    if (!auto_message_) {
      auto_message_ = std::make_unique<AutoMessage>();
      auto_message_->TakeRequest(
          NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
              info.v4_endpoint, auto_message_.get()));
    }
  }
  void OnReceiverChanged(const ServiceInfo& info) override {
    OSP_LOG_INFO << "changed! " << info.friendly_name;
  }
  void OnReceiverRemoved(const ServiceInfo& info) override {
    OSP_LOG_INFO << "removed! " << info.friendly_name;
  }
  void OnAllReceiversRemoved() override { OSP_LOG_INFO << "all removed!"; }
  void OnError(ServiceListenerError) override {}
  void OnMetrics(ServiceListener::Metrics) override {}

 private:
  std::unique_ptr<AutoMessage> auto_message_;
};

class PublisherObserver final : public ServicePublisher::Observer {
 public:
  ~PublisherObserver() override = default;

  void OnStarted() override { OSP_LOG_INFO << "publisher started!"; }
  void OnStopped() override { OSP_LOG_INFO << "publisher stopped!"; }
  void OnSuspended() override { OSP_LOG_INFO << "publisher suspended!"; }

  void OnError(ServicePublisherError) override {}
  void OnMetrics(ServicePublisher::Metrics) override {}
};

class ConnectionClientObserver final
    : public ProtocolConnectionServiceObserver {
 public:
  ~ConnectionClientObserver() override = default;
  void OnRunning() override {}
  void OnStopped() override {}

  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}
};

class ConnectionServerObserver final
    : public ProtocolConnectionServer::Observer {
 public:
  class ConnectionObserver final : public ProtocolConnection::Observer {
   public:
    explicit ConnectionObserver(ConnectionServerObserver* parent)
        : parent_(parent) {}
    ~ConnectionObserver() override = default;

    void OnConnectionClosed(const ProtocolConnection& connection) override {
      auto& connections = parent_->connections_;
      connections.erase(
          std::remove_if(
              connections.begin(), connections.end(),
              [this](const std::pair<std::unique_ptr<ConnectionObserver>,
                                     std::unique_ptr<ProtocolConnection>>& p) {
                return p.first.get() == this;
              }),
          connections.end());
    }

   private:
    ConnectionServerObserver* const parent_;
  };

  ~ConnectionServerObserver() override = default;

  void OnRunning() override {}
  void OnStopped() override {}
  void OnSuspended() override {}

  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}

  void OnIncomingConnection(
      std::unique_ptr<ProtocolConnection>&& connection) override {
    auto observer = std::make_unique<ConnectionObserver>(this);
    connection->SetObserver(observer.get());
    connections_.emplace_back(std::move(observer), std::move(connection));
    connections_.back().second->CloseWriteEnd();
  }

 private:
  std::vector<std::pair<std::unique_ptr<ConnectionObserver>,
                        std::unique_ptr<ProtocolConnection>>>
      connections_;
};

class RCD final : public presentation::Connection::Delegate {
 public:
  RCD() = default;
  ~RCD() override = default;

  void OnConnected() override {
    OSP_LOG_INFO << "presentation connection connected";
  }
  void OnClosedByRemote() override {
    OSP_LOG_INFO << "presentation connection closed by remote";
  }
  void OnDiscarded() override {}
  void OnError(const std::string& message) override {}
  void OnTerminated() override { OSP_LOG_INFO << "presentation terminated"; }

  void OnStringMessage(std::string message) override {
    OSP_LOG_INFO << "got message: " << message;
    connection->SendString("--echo-- " + message);
  }
  void OnBinaryMessage(std::vector<uint8_t> data) override {}

  presentation::Connection* connection;
};

class ReceiverDelegate final : public presentation::ReceiverDelegate {
 public:
  ~ReceiverDelegate() override = default;

  std::vector<msgs::PresentationUrlAvailability> OnUrlAvailabilityRequest(
      uint64_t client_id,
      uint64_t request_duration,
      std::vector<std::string>&& urls) override {
    std::vector<msgs::PresentationUrlAvailability> result;
    result.reserve(urls.size());
    for (const auto& url : urls) {
      OSP_LOG_INFO << "got availability request for: " << url;
      result.push_back(msgs::kCompatible);
    }
    return result;
  }

  bool StartPresentation(const presentation::Connection::Info& info,
                         uint64_t source_id,
                         const std::string& http_headers) override {
    presentation_id = info.id;
    connection = MakeUnique<presentation::Connection>(
        info, &cd, presentation::Connection::Role::kReceiver);
    cd.connection = connection.get();
    presentation::Receiver::Get()->OnPresentationStarted(
        info.id, connection.get(), presentation::ResponseResult::kSuccess);
    return true;
  }

  bool ConnectToPresentation(uint64_t request_id,
                             const std::string& id,
                             uint64_t source_id) override {
    connection = MakeUnique<presentation::Connection>(
        presentation::Connection::Info{id, connection->url()}, &cd,
        presentation::Connection::Role::kReceiver);
    cd.connection = connection.get();
    presentation::Receiver::Get()->OnConnectionCreated(
        request_id, connection.get(), presentation::ResponseResult::kSuccess);
    return true;
  }

  void TerminatePresentation(const std::string& id,
                             presentation::TerminationReason reason) override {
    presentation::Receiver::Get()->OnPresentationTerminated(id, reason);
  }

  std::string presentation_id;
  std::unique_ptr<presentation::Connection> connection;
  RCD cd;
};

void ListenerDemo() {
  SignalThings();

  ListenerObserver listener_observer;
  MdnsServiceListenerConfig listener_config;
  auto mdns_listener =
      MdnsServiceListenerFactory::Create(listener_config, &listener_observer);

  MessageDemuxer demuxer;
  ConnectionClientObserver client_observer;
  auto connection_client =
      ProtocolConnectionClientFactory::Create(&demuxer, &client_observer);

  auto* network_service = NetworkServiceManager::Create(
      std::move(mdns_listener), nullptr, std::move(connection_client), nullptr);

  network_service->GetMdnsServiceListener()->Start();
  network_service->GetProtocolConnectionClient()->Start();

  while (!g_done) {
    network_service->RunEventLoopOnce();
  }

  network_service->GetMdnsServiceListener()->Stop();
  network_service->GetProtocolConnectionClient()->Stop();

  NetworkServiceManager::Dispose();
}

void PublisherDemo(const std::string& friendly_name) {
  SignalThings();

  constexpr uint16_t server_port = 6667;

  PublisherObserver publisher_observer;
  // TODO(btolsch): aggregate initialization probably better?
  ScreenPublisher::Config publisher_config;
  publisher_config.friendly_name = friendly_name;
  publisher_config.hostname = "turtle-deadbeef";
  publisher_config.service_instance_name = "deadbeef";
  publisher_config.connection_server_port = server_port;
  // TODO(btolsch): Multihomed support is broken due to ScreenInfo holding
  // only one address.
  // publisher_config.network_interface_indices.push_back(3);
  auto mdns_publisher =
      MdnsScreenPublisherFactory::Create(publisher_config, &publisher_observer);

  ServerConfig server_config;
  std::vector<platform::InterfaceAddresses> interfaces =
      platform::GetInterfaceAddresses();
  for (const auto& interface : interfaces) {
    server_config.connection_endpoints.push_back(
        IPEndpoint{interface.addresses[0].address, 6667});
  }
  MessageDemuxer demuxer;
  ConnectionServerObserver server_observer;
  auto connection_server = ProtocolConnectionServerFactory::Create(
      server_config, &demuxer, &server_observer);
  auto* network_service =
      NetworkServiceManager::Create(nullptr, std::move(mdns_publisher), nullptr,
                                    std::move(connection_server));

  ReceiverDelegate receiver_delegate;
  presentation::Receiver::Get()->Init();
  presentation::Receiver::Get()->SetReceiverDelegate(&receiver_delegate);
  network_service->GetMdnsScreenPublisher()->Start();
  network_service->GetProtocolConnectionServer()->Start();

  pollfd stdin_pollfd{STDIN_FILENO, POLLIN};

  write(STDOUT_FILENO, "$ ", 2);

  while (poll(&stdin_pollfd, 1, 10) >= 0) {
    if (g_done) {
      break;
    }
    network_service->RunEventLoopOnce();

    if (stdin_pollfd.revents == 0) {
      continue;
    } else if (stdin_pollfd.revents & (POLLERR | POLLHUP)) {
      break;
    }
    std::string line;
    if (!std::getline(std::cin, line)) {
      break;
    }

    size_t i = 0;
    while (i < line.size() && line[i] != ' ') {
      ++i;
    }
    std::string command = line.substr(0, i);
    ++i;

    if (command == "avail") {
    } else if (command == "msg") {
      receiver_delegate.connection->SendString(line.substr(i));
    } else if (command == "close") {
      receiver_delegate.connection->Close(
          presentation::Connection::CloseReason::kClosed);
    } else if (command == "term") {
      presentation::Receiver::Get()->OnPresentationTerminated(
          receiver_delegate.presentation_id,
          presentation::TerminationReason::kReceiverUserTerminated);
    }

    write(STDOUT_FILENO, "$ ", 2);
  }

  presentation::Receiver::Get()->SetReceiverDelegate(nullptr);
  presentation::Receiver::Get()->Deinit();
  network_service->GetMdnsScreenPublisher()->Stop();
  network_service->GetProtocolConnectionServer()->Stop();

  NetworkServiceManager::Dispose();
}

}  // namespace
}  // namespace openscreen

int main(int argc, char** argv) {
  openscreen::platform::LogInit(argc > 1 ? "_recv_fifo" : "_cntl_fifo");
  openscreen::platform::SetLogLevel(openscreen::platform::LogLevel::kVerbose,
                                    1);
  if (argc == 1) {
    openscreen::ListenerDemo();
  } else {
    openscreen::PublisherDemo(argv[1]);
  }

  return 0;
}
