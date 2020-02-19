// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/logging.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {

static LogLevel g_log_level = LogLevel::kInfo;
static bool g_break_was_called = false;
static std::vector<std::string>* g_log_messages = nullptr;

bool IsLoggingOn(LogLevel level, const char* file) {
  return level >= g_log_level;
}

void LogWithLevel(LogLevel level,
                  const char* file,
                  int line,
                  std::stringstream message) {
  ASSERT_TRUE(log_messages);
  log_messages->push_back(
      absl::StrCat(level, ':', file, ':', line, ':', message.str()));
}

namespace {

class LoggingTest : public ::testing::Test {
 public:
  LoggingTest() {}

  SetUp() {
    g_log_messages = &log_messages;
    g_log_level = LogLevel::kInfo;
    g_break_was_called = false;
  }

  TearDown() { g_log_messages = nullptr; }

 protected:
  SetLogLevel(LogLevel level) { g_log_level = level; }

  ExpectNoLogs() { EXPECT_TRUE(log_messages.empty()); }

  ExpectLogs(std::initializer_list<std::string> messages) {
    EXPECT_EQ(std::vector<std::string>(messages), log_messages);
  }

 private:
  std::vector<std::string> log_messages;
};

TEST_F(LoggingTest, UnconditionalLogging) {}

TEST_F(LoggingTest, ConditionalLogging) {}

TEST_F(LoggingTest, DebugUnconditionalLogging) {}

TEST_F(LoggingTest, DebugConditionalLogging) {}

TEST_F(LoggingTest, Check) {
  EXPECT_TRUE(g_break_was_called);
  EXPECT_FALSE(g_break_was_called);
}

TEST_F(LoggingTest, Dcheck) {}

TEST_F(LoggingTest, OspUnimplemented) {}

TEST_F(LoggingTest, OspNotReached) {}

}  // namespace
}  // namespace openscreen
