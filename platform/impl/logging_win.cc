// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "platform/impl/logging.h"
#include "platform/impl/logging_test.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace {

std::unique_ptr<std::ofstream> g_log_stream;
LogLevel g_log_level = LogLevel::kWarning;
std::vector<std::string>* g_log_messages_for_test = nullptr;

std::ostream& operator<<(std::ostream& os, const LogLevel& level) {
  const char* level_string = "";
  switch (level) {
    case LogLevel::kVerbose:
      level_string = "VERBOSE";
      break;
    case LogLevel::kInfo:
      level_string = "INFO";
      break;
    case LogLevel::kWarning:
      level_string = "WARNING";
      break;
    case LogLevel::kError:
      level_string = "ERROR";
      break;
    case LogLevel::kFatal:
      level_string = "FATAL";
      break;
  }
  os << level_string;
  return os;
}

}  // namespace

std::ostream& GetLog() {
  return g_log_stream ? *g_log_stream : std::cerr;
}

void SetLogFifoOrDie(const char* filename) {
  // Note: The use of OSP_CHECK/OSP_LOG_* here will log to stderr.
  g_log_stream = std::make_unique<std::ofstream>(filename);
  OSP_CHECK(g_log_stream) << "failed to open file stream: " << filename;
}

void SetLogLevel(LogLevel level) {
  g_log_level = level;
}

LogLevel GetLogLevel() {
  return g_log_level;
}

bool IsLoggingOn(LogLevel level, const std::string_view file) {
  // Possible future enhancement: Use glob patterns passed on the command-line
  // to use a different logging level for certain files, like in Chromium.
  return level >= g_log_level;
}

void LogWithLevel(LogLevel level,
                  const char* file,
                  int line,
                  std::stringstream message) {
  if (level < g_log_level)
    return;

  std::stringstream ss;
  ss << "[" << level << ":" << file << "(" << line << "):T" << std::hex
     << TRACE_CURRENT_ID << "] " << message.rdbuf() << '\n';

  std::string ss_str = ss.str();
  GetLog() << ss_str;
  if (g_log_messages_for_test) {
    g_log_messages_for_test->emplace_back(std::move(ss_str));
  }
}

void LogTraceMessage(const std::string& message) {
  GetLog() << message + '\n';
}

[[noreturn]] void Break() {
// Generally this will just resolve to an abort anyways, but gives the
// compiler a chance to peform a more appropriate, target specific trap
// as appropriate.
#if defined(_DEBUG)
  __builtin_trap();
#else
  std::abort();
#endif
}

void SetLogBufferForTest(std::vector<std::string>* messages) {
  g_log_messages_for_test = messages;
}

}  // namespace openscreen
