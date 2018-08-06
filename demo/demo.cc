// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "api/public/mdns_screen_listener_factory.h"
#include "api/public/network_service_manager.h"
#include "api/public/screen_listener.h"
#include "api/public/screen_publisher.h"
#include "base/make_unique.h"
#include "discovery/mdns/mdns_responder_adapter_impl.h"
#include "platform/api/error.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace {

bool g_done = false;
bool g_dump_services = false;

struct Service {
  Service(mdns::DomainName service_instance)
      : service_instance(std::move(service_instance)) {}
  ~Service() = default;

  mdns::DomainName service_instance;
  mdns::DomainName domain_name;
  IPv4Address v4_address;
  IPv6Address v6_address;
  uint16_t port;
  std::vector<std::string> txt;
};
std::vector<Service> g_services;

void sigusr1_dump_services(int) {
  g_dump_services = true;
}

void sigint_stop(int) {
  LOG_INFO << "caught SIGINT, exiting...";
  g_done = true;
}

std::vector<std::string> SplitByDot(const std::string& domain_part) {
  std::vector<std::string> result;
  auto copy_it = domain_part.begin();
  for (auto it = domain_part.begin(); it != domain_part.end(); ++it) {
    if (*it == '.') {
      result.emplace_back(copy_it, it);
      copy_it = it + 1;
    }
  }
  if (copy_it != domain_part.end()) {
    result.emplace_back(copy_it, domain_part.end());
  }
  return result;
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

  LOG_INFO << "signal handlers setup";
  LOG_INFO << "pid: " << getpid();
}

// void LogService(const Service& s) {
//   LOG_INFO << "PTR: (" << s.service_instance << ")";
//   LOG_INFO << "SRV: " << s.domain_name << ":" << s.port;
//   LOG_INFO << "TXT:";
//   for (const auto& l : s.txt) {
//     LOG_INFO << " | " << l;
//   }
//   LOG_INFO << "A: " << static_cast<int>(s.v4_address.bytes[0]) << "."
//            << static_cast<int>(s.v4_address.bytes[1]) << "."
//            << static_cast<int>(s.v4_address.bytes[2]) << "."
//            << static_cast<int>(s.v4_address.bytes[3]);
// }

class ListenerObserver final : public openscreen::ScreenListenerObserver {
 public:
  ~ListenerObserver() override = default;
  void OnStarted() override { LOG_INFO << "started!"; }
  void OnStopped() override { LOG_INFO << "stopped!"; }
  void OnSuspended() override { LOG_INFO << "suspended!"; }
  void OnSearching() override { LOG_INFO << "searching!"; }

  void OnScreenAdded(const ScreenInfo& info) override {
    LOG_INFO << "found! " << info.friendly_name;
  }
  void OnScreenChanged(const ScreenInfo& info) override {
    LOG_INFO << "changed! " << info.friendly_name;
  }
  void OnScreenRemoved(const ScreenInfo& info) override {
    LOG_INFO << "removed! " << info.friendly_name;
  }
  void OnAllScreensRemoved() override {
    LOG_INFO << "all removed!";
  }
  void OnError(ScreenListenerErrorInfo) override {}
  void OnMetrics(ScreenListenerMetrics) override {}
};

// class PublisherObserver final : openscreen::ScreenPublisherObserver {
//  public:
// };

void EmbedderDemo(const mdns::DomainName& service_type,
                  const std::string& service_name) {
  SignalThings();

  {
    ListenerObserver listener_observer;
    MdnsScreenListenerConfig config;
    // PublisherObserver publisher_observer;
    auto mdns_listener = openscreen::MdnsScreenListenerFactory::Create(
        config, &listener_observer);
    // auto mdns_publisher = openscreen::MdnsScreenPublisherFactory::Create(
    //     MdnsScreenPublisherConfig, &publisher_observer);
    auto* network_service = openscreen::NetworkServiceManager::Create(
        std::move(mdns_listener), nullptr, nullptr, nullptr);

    network_service->GetMdnsScreenListener()->Start();
    // network_service->GetMdnsScreenPublisher()->Start();

    while (!g_done) {
      network_service->RunEventLoopOnce();
    }

    openscreen::NetworkServiceManager::Dispose();
  }
}

}  // namespace
}  // namespace openscreen

int main(int argc, char** argv) {
  openscreen::platform::SetLogLevel(openscreen::platform::LogLevel::kVerbose,
                                    0);
  std::string service_type("_openscreen._udp");
  std::string service_name("turtle");
  if (argc >= 2) {
    service_type = argv[1];
  }
  if (argc >= 3) {
    service_name = argv[2];
  }

  openscreen::mdns::DomainName d;
  if (!openscreen::mdns::DomainName::FromLabels(
          openscreen::SplitByDot(service_type), &d)) {
    return 1;
  }
  openscreen::EmbedderDemo(d, service_name);

  return 0;
}
