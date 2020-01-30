// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <functional>
#include <iostream>
#include <map>

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

    auto error = publisher_->Register(ServiceInfoToDnsSdRecord(info), this);
    if (!error.ok()) {
      std::cout << "\tFailed to publish service instance '"
                << info.friendly_name << "': " << error << "!\n";
      OSP_NOTREACHED();
    }

    return error;
  }

  Error UpdateRegistration(const ServiceInfo& info) {
    if (!info.IsValid()) {
      return Error::Code::kParameterInvalid;
    }

    return publisher_->UpdateRegistration(ServiceInfoToDnsSdRecord(info));
  }

  int DeregisterAll() { return publisher_->DeregisterAll(kCastV2ServiceId); }

  absl::optional<std::string> GetClaimedInstanceId(
      const std::string& requested_id) {
    auto it = instance_ids_.find(requested_id);
    if (it == instance_ids_.end()) {
      return absl::nullopt;
    } else {
      return it->second;
    }
  }

 private:
  // DnsSdPublisher::Client overrides.
  void OnInstanceIdClaimed(const std::string& requested_id,
                           const std::string& claimed_id) override {
    instance_ids_.emplace(requested_id, claimed_id);
  }

  discovery::DnsSdPublisher* const publisher_;
  std::map<std::string, std::string> instance_ids_;
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

  bool IsServiceFound(const std::string& name) {
    return std::find_if(service_infos_.begin(), service_infos_.end(),
                        [&name](const ServiceInfo& info) {
                          return info.friendly_name == name;
                        }) != service_infos_.end();
  }

 private:
  void ProcessResults(
      std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
    service_infos_ = std::vector<ServiceInfo>();
    for (const ServiceInfo& info : infos) {
      service_infos_.push_back(info);
    }
  }

  std::vector<ServiceInfo> service_infos_;
};

ServiceInfo GetInfoV4() {
  ServiceInfo hosted_service;
  hosted_service.v4_endpoint = IPEndpoint{{10, 0, 0, 1}, 25252};
  hosted_service.unique_id = "id";
  hosted_service.model_name = "openscreen-ModelV4";
  hosted_service.friendly_name = "DemoV4!";
  return hosted_service;
}

ServiceInfo GetInfoV6() {
  ServiceInfo hosted_service;
  hosted_service.v6_endpoint = IPEndpoint{{1, 2, 3, 4, 5, 6, 7, 8}, 25253};
  hosted_service.unique_id = "id";
  hosted_service.model_name = "openscreen-ModelV6";
  hosted_service.friendly_name = "DemoV6!";
  return hosted_service;
}

ServiceInfo GetInfoV4V6() {
  ServiceInfo hosted_service;
  hosted_service.v4_endpoint = IPEndpoint{{10, 0, 0, 1}, 25254};
  hosted_service.v6_endpoint = IPEndpoint{{1, 2, 3, 4, 5, 6, 7, 8}, 25255};
  hosted_service.unique_id = "id";
  hosted_service.model_name = "openscreen-ModelV4andV6";
  hosted_service.friendly_name = "DemoV4andV6!";
  return hosted_service;
}

void OnFatalError(Error error) {
  std::cout << "Fatal error received: '" << error << "'\n";
}

void CheckForClaimedIds(TaskRunner* task_runner,
                        Publisher* publisher,
                        const ServiceInfo& info,
                        std::atomic_bool* has_been_found,
                        int attempts) {
  constexpr Clock::duration kDelayBetweenChecks =
      Clock::duration{100 * 1000};  // 100 ms.
  constexpr int kMaxAttempts = 50;  // 5 second delay.

  if (publisher->GetClaimedInstanceId(info.GetInstanceId()) == absl::nullopt) {
    if (attempts++ > kMaxAttempts) {
      OSP_NOTREACHED() << "Service " << info.friendly_name
                       << " publication failed.";
    }
    task_runner->PostTaskWithDelay(
        [task_runner, publisher, &info, has_been_found, attempts]() {
          CheckForClaimedIds(task_runner, publisher, info, has_been_found,
                             attempts);
        },
        kDelayBetweenChecks);
  } else {
    *has_been_found = true;
    std::cout << "\tInstance '" << info.friendly_name << "' published...\n";
  }
}

void CheckForPublishedService(TaskRunner* task_runner,
                              Receiver* receiver,
                              const ServiceInfo& service_info,
                              std::atomic_bool* has_been_seen,
                              int attempts) {
  constexpr Clock::duration kDelayBetweenChecks =
      Clock::duration{100 * 1000};  // 100 ms.
  constexpr int kMaxAttempts = 50;  // 5 second delay.

  if (!receiver->IsServiceFound(service_info.friendly_name)) {
    if (attempts++ > kMaxAttempts) {
      OSP_NOTREACHED() << "Service " << service_info.friendly_name
                       << " discovery failed.";
    }
    task_runner->PostTaskWithDelay(
        [task_runner, receiver, &service_info, has_been_seen, attempts]() {
          CheckForPublishedService(task_runner, receiver, service_info,
                                   has_been_seen, attempts);
        },
        kDelayBetweenChecks);
  } else {
    std::cout << "\tFound instance '" << service_info.friendly_name << "'!\n";
    *has_been_seen = true;
  }
}

}  // namespace cast
}  // namespace openscreen

int main(int argc, const char* argv[]) {
  // Get the loopback interface to run on.
  std::vector<openscreen::InterfaceInfo> interfaces =
      openscreen::GetNetworkInterfaces(true);
  openscreen::InterfaceInfo* loopback = nullptr;
  for (auto it = interfaces.begin(); it != interfaces.end(); it++) {
    if (it->name == "lo") {
      loopback = &(*it);
    }
  }
  OSP_DCHECK(loopback);

  // Start up the background utils to run on posix.
  openscreen::PlatformClientPosix::Create(openscreen::Clock::duration{50},
                                          openscreen::Clock::duration{50});
  auto* task_runner_ptr =
      openscreen::PlatformClientPosix::GetInstance()->GetTaskRunner();

  // Set up demo infra.
  auto service = openscreen::discovery::DnsSdService::Create(
      task_runner_ptr, openscreen::cast::OnFatalError, loopback->index);
  openscreen::cast::Receiver receiver(service.get());
  openscreen::cast::Publisher publisher(service.get());
  auto v4 = openscreen::cast::GetInfoV4();
  auto v6 = openscreen::cast::GetInfoV6();
  auto both = openscreen::cast::GetInfoV4V6();

  // Start discovery and publication.
  task_runner_ptr->PostTask([&receiver]() { receiver.StartDiscovery(); });
  task_runner_ptr->PostTask([&publisher, &v4]() { publisher.Register(v4); });
  task_runner_ptr->PostTask([&publisher, &v6]() { publisher.Register(v6); });
  task_runner_ptr->PostTask(
      [&publisher, &both]() { publisher.Register(both); });

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  std::cout << "Service publication in progress...\n";
  std::atomic_bool v4_found{false};
  std::atomic_bool v6_found{false};
  std::atomic_bool both_found{false};
  task_runner_ptr->PostTask(
      [task_runner_ptr, &publisher, &v4, &v4_found]() mutable {
        CheckForClaimedIds(task_runner_ptr, &publisher, v4, &v4_found, 0);
      });
  task_runner_ptr->PostTask(
      [task_runner_ptr, &publisher, &v6, &v6_found]() mutable {
        CheckForClaimedIds(task_runner_ptr, &publisher, v6, &v6_found, 0);
      });
  task_runner_ptr->PostTask(
      [task_runner_ptr, &publisher, &both, &both_found]() mutable {
        CheckForClaimedIds(task_runner_ptr, &publisher, both, &both_found, 0);
      });
  while (!(v4_found && v6_found && both_found)) {
    std::cout << "\tWaiting...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  std::cout << "\tAll services successfully published!\n";
  std::cout.flush();

  // Make sure all services are found through discovery.
  std::cout << "Service discovery in progress...\n";
  v4_found = false;
  v6_found = false;
  both_found = false;
  task_runner_ptr->PostTask(
      [task_runner_ptr, &receiver, &v4, &v4_found]() mutable {
        CheckForPublishedService(task_runner_ptr, &receiver, v4, &v4_found, 0);
      });
  task_runner_ptr->PostTask(
      [task_runner_ptr, &receiver, &v6, &v6_found]() mutable {
        CheckForPublishedService(task_runner_ptr, &receiver, v6, &v6_found, 0);
      });
  task_runner_ptr->PostTask([task_runner_ptr, &receiver, &both,
                             &both_found]() mutable {
    CheckForPublishedService(task_runner_ptr, &receiver, both, &both_found, 0);
  });
  while (!(v4_found && v6_found && both_found)) {
    std::cout << "\tWaiting...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  std::cout << "\tAll services successfully discovered!\n";
  std::cout.flush();

  std::cout << "TEST COMPLETE!";
  std::cout.flush();

  openscreen::PlatformClientPosix::ShutDown();
  return 0;
}
