// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <string>

#include "gtest/gtest.h"
#include "platform/impl/logging.h"
#include "platform/impl/text_trace_logging_platform.h"
#include "util/trace_logging.h"

namespace {
void LogUsage(const char* argv0) {
  constexpr char kExecutableTag[] = "argv[0]";
  constexpr char kUsageMessage[] = R"(
usage: argv[0] <options>

options:
  -t, --tracing: Enable performance tracing logging.

  -v, --verbose: Enable verbose logging.

  -h, --help: Show this help message.

see below for additional Google Test related help:
)";
  std::string message = kUsageMessage;
  message.replace(message.find(kExecutableTag), strlen(kExecutableTag), argv0);
  OSP_LOG_INFO << message;
}

struct GlobalTestState {
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
  bool args_are_valid = false;
};

GlobalTestState InitFromArgs(int argc, char** argv) {
  const struct option argument_options[] = {
      {"tracing", no_argument, nullptr, 't'},
      {"verbose", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  GlobalTestState state;
  std::unique_ptr<openscreen::TextTraceLoggingPlatform> trace_logger;
  int ch = -1;
  while ((ch = getopt_long(argc, argv, "tvh", argument_options, nullptr)) !=
         -1) {
    switch (ch) {
      case 't':
        OSP_LOG_INFO << "Trace logging enabled...";
        state.trace_logger =
            std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'v':
        OSP_LOG_INFO << "Verbose logging enabled...";
        openscreen::SetLogLevel(openscreen::LogLevel::kVerbose);
        break;
      case 'h':
        LogUsage(argv[0]);
        return state;
    }
  }

  state.args_are_valid = true;
  return state;
}
}  // namespace

// Googletest strongly recommends that we roll our own main
// function if we want to do global test environment setup.
// See the below link for more info;
// https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-case
int main(int argc, char** argv) {
  // We have to set logging level first, otherwise we can't log from here.
  openscreen::SetLogLevel(openscreen::LogLevel::kInfo);
  auto state = InitFromArgs(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
