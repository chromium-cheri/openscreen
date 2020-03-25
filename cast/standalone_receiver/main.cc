// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>
#include <uuid/uuid.h>

#include <array>
#include <chrono>  // NOLINT

#include "cast/common/public/service_info.h"
#include "cast/standalone_receiver/cast_agent.h"
#include "cast/streaming/ssrc.h"
#include "discovery/common/config.h"
#include "discovery/common/reporting_client.h"
#include "discovery/public/dns_sd_service_factory.h"
#include "discovery/public/dns_sd_service_publisher.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace cast {
namespace {

static constexpr char kCastV2ServiceId[] = "_googlecast._tcp";

// TODO(jophba): set this based on actual values when the cast agent is
// complete.
static constexpr int kCastTlsPort = 80;

class DiscoveryReportingClient : public discovery::ReportingClient {
  void OnFatalError(Error error) override {
    OSP_LOG_FATAL << "Encountered fatal discovery error: " << error;
  }

  void OnRecoverableError(Error error) override {
    OSP_LOG_ERROR << "Encountered recoverable discovery error: " << error;
  }
};

struct DiscoveryState {
  SerialDeletePtr<discovery::DnsSdService> service;
  std::unique_ptr<DiscoveryReportingClient> reporting_client;
  std::unique_ptr<discovery::DnsSdServicePublisher<ServiceInfo>> publisher;
};

std::string GenerateUuid() {
  uuid_t unique_id;
  uuid_generate(unique_id);

  // UUIDs are encoded as a 36-byte string plus a tailing '\0'.
  constexpr int kUuidSize = 37;
  char uuid_string[kUuidSize];
  uuid_unparse(unique_id, uuid_string);

  return std::string(uuid_string);
}

std::unique_ptr<DiscoveryState> StartDiscovery(TaskRunner* task_runner,
                                               const InterfaceInfo& interface) {
  discovery::Config config;

  config.interface = interface;

  auto state = std::make_unique<DiscoveryState>();
  state->reporting_client = std::make_unique<DiscoveryReportingClient>();
  state->service = discovery::CreateDnsSdService(
      task_runner, state->reporting_client.get(), config);

  ServiceInfo info;
  if (interface.GetIpAddressV4()) {
    info.v4_endpoint = IPEndpoint{interface.GetIpAddressV4(), kCastTlsPort};
  }
  if (interface.GetIpAddressV6()) {
    info.v6_endpoint = IPEndpoint{interface.GetIpAddressV6(), kCastTlsPort};
  }

  info.unique_id = GenerateUuid();

  // TODO(jophba): add command line arguments to set these fields.
  info.model_name = "Cast Standalone Receiver";
  info.friendly_name = "Cast Standalone Receiver";

  state->publisher =
      std::make_unique<discovery::DnsSdServicePublisher<ServiceInfo>>(
          state->service.get(), kCastV2ServiceId, ServiceInfoToDnsSdRecord);
  state->publisher->Register(info);
  return state;
}

void RunStandaloneReceiver(TaskRunnerImpl* task_runner,
                           InterfaceInfo interface) {
  CastAgent agent(task_runner, interface);
  agent.Start();

  // Run the event loop until an exit is requested (e.g., the video player GUI
  // window is closed, a SIGINT or SIGTERM is received, or whatever other
  // appropriate user indication that shutdown is requested).
  task_runner->RunUntilSignaled();
}

}  // namespace
}  // namespace cast
}  // namespace openscreen

namespace {

void LogUsage(const char* argv0) {
  constexpr char kExecutableTag[] = "argv[0]";
  constexpr char kUsageMessage[] = R"(
    usage: argv[0] <options>

      -i, --interface= <interface, e.g. wlan0, eth0>
           Specify the network interface to bind to. The interface is looked
           up from the system interface registry. This argument is optional, and
           omitting it causes the standalone receiver to attempt to bind to
           ANY (0.0.0.0) on default port 2344. Note that this mode does not
           work reliably on some platforms.

      -t, --tracing: Enable performance tracing logging.

      -h, --help: Show this help message.
  )";
  std::string message = kUsageMessage;
  message.replace(message.find(kExecutableTag), strlen(kExecutableTag), argv0);
  OSP_LOG_ERROR << message;
}

}  // namespace

int main(int argc, char* argv[]) {
  // TODO(jophba): refactor into separate method and make main a one-liner.
  using openscreen::Clock;
  using openscreen::ErrorOr;
  using openscreen::InterfaceInfo;
  using openscreen::IPAddress;
  using openscreen::IPEndpoint;
  using openscreen::PlatformClientPosix;
  using openscreen::TaskRunnerImpl;

  openscreen::SetLogLevel(openscreen::LogLevel::kInfo);

  const struct option argument_options[] = {
      {"interface", required_argument, nullptr, 'i'},
      {"tracing", no_argument, nullptr, 't'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  InterfaceInfo interface_info;
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "i:th", argument_options, nullptr)) !=
         -1) {
    switch (ch) {
      case 'i': {
        std::vector<InterfaceInfo> network_interfaces =
            openscreen::GetNetworkInterfaces();
        for (auto& interface : network_interfaces) {
          if (interface.name == optarg) {
            interface_info = std::move(interface);
          }
        }

        if (interface_info.name.empty()) {
          OSP_LOG_ERROR << "Invalid interface specified: " << optarg;
          return 1;
        }
        break;
      }
      case 't':
        trace_logger = std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'h':
        LogUsage(argv[0]);
        return 1;
    }
  }

  auto* const task_runner = new TaskRunnerImpl(&Clock::now);
  PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                              std::unique_ptr<TaskRunnerImpl>(task_runner));

  auto discovery_state =
      openscreen::cast::StartDiscovery(task_runner, interface_info);

  // Runs until the process is interrupted.  Safe to pass |task_runner| as it
  // will not be destroyed by ShutDown() until this exits.
  openscreen::cast::RunStandaloneReceiver(task_runner, interface_info);
  PlatformClientPosix::ShutDown();
  return 0;
}
