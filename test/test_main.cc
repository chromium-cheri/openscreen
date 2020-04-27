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

  // We only show the help message if the user specifies help, so it should
  // always show up.
  OSP_LOG_WARN << message;
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
        state.trace_logger =
            std::make_unique<openscreen::TextTraceLoggingPlatform>();
        break;
      case 'v':
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
//
// This main method is a drop-in replacement for anywhere that currently
// depends on gtest_main, meaning it can be linked into any test-only binary
// to provide a main implementation that supports setting flags and other
// state that we want shared between all tests.
int main(int argc, char** argv) {
  // Logging by default is enabled, but only for major issues.
  openscreen::SetLogLevel(openscreen::LogLevel::kWarning);
  auto state = InitFromArgs(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
