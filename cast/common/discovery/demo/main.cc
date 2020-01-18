// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <iostream>

#include "cast/common/discovery/service_info.h"
#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "discovery/public/dns_sd_service_watcher.h"
#include "platform/api/network_interface.h"
#include "platform/api/udp_socket.h"
#include "platform/base/interface_info.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace cast {

// Publishes new service instances.
class Publisher : public discovery::DnsSdPublisher::Client {
 public:
  Publisher(discovery::DnsSdService* service)
      : publisher_(service->GetPublisher()) {
    std::cout << "Initializing Publisher...\n";
  }

  ~Publisher() override = default;

  Error Register(const ServiceInfo& info) {
    if (!info.IsValid()) {
      std::cout << "Invalid record provided to Register\n";
      return Error::Code::kParameterInvalid;
    }

    std::cout << "Publishing service instance '" << info.friendly_name
              << "'...\n";
    auto error = publisher_->Register(ServiceInfoToDnsSdRecord(info), this);
    if (!error.ok()) {
      std::cout << "\tFailed!\n";
    }

    return error;
  }

  Error UpdateRegistration(const ServiceInfo& info) {
    if (!info.IsValid()) {
      std::cout << "Invalid record provided to Update\n";
      return Error::Code::kParameterInvalid;
    }

    std::cout << "Updating service instance '" << info.friendly_name << "'\n";
    return publisher_->UpdateRegistration(ServiceInfoToDnsSdRecord(info));
  }

  int DeregisterAll() {
    std::cout << "Deregistering all services\n";
    return publisher_->DeregisterAll(kCastV2ServiceId);
  }

 private:
  // DnsSdPublisher::Client overrides.
  void OnInstanceIdClaimed(const std::string& requested_id,
                           const std::string& claimed_id) override {
    std::cout << "\tDone!\n";
  }

  discovery::DnsSdPublisher* const publisher_;
};

// Receives incoming services and outputs their results to stdout.
class Receiver : public discovery::DnsSdServiceWatcher<ServiceInfo> {
 public:
  Receiver(discovery::DnsSdService* service)
      : discovery::DnsSdServiceWatcher<ServiceInfo>(
            service,
            kCastV2ServiceId,
            DnsSdRecordToServiceInfo,
            [this](
                std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
              ProcessResults(std::move(infos));
            }) {
    std::cout << "Initializing Receiver...\n";
  }

 private:
  void ProcessResults(
      std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
    std::cout << "\n\nFound services:\n";
    for (const auto& info : infos) {
      std::cout << "\t- '" << info.get().friendly_name << "'\n";
    }
  }
};

ServiceInfo GetInfo() {
  ServiceInfo hosted_service;
  hosted_service.v4_endpoint = IPEndpoint{{10, 0, 0, 1}, 80};
  hosted_service.unique_id = "id";
  hosted_service.model_name = "openscreen";
  hosted_service.friendly_name = "Demo!";
  return hosted_service;
}

void OnFatalError(Error error) {
  std::cout << "Fatal error received: '" << error << "'\n";
}

}  // namespace cast
}  // namespace openscreen

int main(int argc, const char* argv[]) {
  class PlatformClientExposingTaskRunner
      : public openscreen::PlatformClientPosix {
   public:
    explicit PlatformClientExposingTaskRunner(
        std::unique_ptr<openscreen::TaskRunner> task_runner)
        : openscreen::PlatformClientPosix(openscreen::Clock::duration{50},
                                          openscreen::Clock::duration{50},
                                          std::move(task_runner)) {
      SetInstance(this);
    }
  };

  auto task_runner =
      std::make_unique<openscreen::TaskRunnerImpl>(&openscreen::Clock::now);
  openscreen::TaskRunnerImpl* task_runner_ptr = task_runner.get();
  auto* const platform_client =
      new PlatformClientExposingTaskRunner(std::move(task_runner));

  std::vector<openscreen::InterfaceInfo> interfaces =
      openscreen::GetNetworkInterfaces(true);
  openscreen::InterfaceInfo* loopback = nullptr;
  for (auto it = interfaces.begin(); it != interfaces.end(); it++) {
    std::cout << "Found interface '" << it->name << "'\n";
    if (it->name == "lo") {
      loopback = &(*it);
    }
  }
  OSP_DCHECK(loopback);

  auto service = openscreen::discovery::DnsSdService::Create(
      task_runner_ptr, openscreen::cast::OnFatalError, loopback->index);
  openscreen::cast::Receiver receiver(service.get());
  openscreen::cast::Publisher publisher(service.get());

  auto info = openscreen::cast::GetInfo();
  task_runner_ptr->PostTask([&receiver]() {
    receiver.StartDiscovery();
    std::cout << "Discovery started!\n";
  });
  task_runner_ptr->PostTask(
      [&publisher, &info]() { publisher.Register(info); });

  task_runner_ptr->RunUntilStopped();
  platform_client->ShutDown();  // Deletes |platform_client|.
  return 0;
}
