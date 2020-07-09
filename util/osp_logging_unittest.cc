// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/osp_logging.h"

#include <initializer_list>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "platform/api/logging.h"

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
  ASSERT_TRUE(g_log_messages);
  g_log_messages->push_back(absl::StrCat(level, ":", message.str()));
}

void Break() {
  g_break_was_called = true;
}

class LoggingTest : public ::testing::Test {
 public:
  LoggingTest() {}

  void SetUp() {
    g_log_messages = &log_messages;
    g_log_level = LogLevel::kInfo;
    g_break_was_called = false;
  }

  void TearDown() { g_log_messages = nullptr; }

 protected:
  void SetLogLevel(LogLevel level) { g_log_level = level; }

  void ExpectNoLogs() { EXPECT_TRUE(log_messages.empty()); }

  void ExpectLogsAndClear(std::initializer_list<std::string> messages) {
    EXPECT_EQ(std::vector<std::string>(messages), log_messages);
    log_messages.clear();
  }

  void ExpectBreakAndClear() {
    EXPECT_TRUE(g_break_was_called);
    g_break_was_called = false;
  }

  void ExpectDebugBreakAndClear() {
#if OSP_DCHECK_IS_ON()
    EXPECT_TRUE(g_break_was_called);
    g_break_was_called = false;
#else
    EXPECT_FALSE(g_break_was_called);
#endif  // OSP_DCHECK_IS_ON()
  }

 private:
  std::vector<std::string> log_messages;
};

TEST_F(LoggingTest, UnconditionalLogging) {
  SetLogLevel(LogLevel::kVerbose);
  OSP_VLOG << "Verbose";
  OSP_LOG_INFO << "Info";
  OSP_LOG_WARN << "Warning";
  OSP_LOG_ERROR << "Error";
  ExpectLogsAndClear({"0:Verbose", "1:Info", "2:Warning", "3:Error"});

  SetLogLevel(LogLevel::kWarning);
  OSP_VLOG << "Verbose";
  OSP_LOG_INFO << "Info";
  OSP_LOG_WARN << "Warning";
  OSP_LOG_ERROR << "Error";
  ExpectLogsAndClear({"2:Warning", "3:Error"});
}

TEST_F(LoggingTest, ConditionalLogging) {
  SetLogLevel(LogLevel::kVerbose);
  OSP_VLOG_IF(true) << "Verbose";
  OSP_LOG_IF(INFO, true) << "Info";
  OSP_LOG_IF(WARN, true) << "Warning";
  OSP_LOG_IF(ERROR, true) << "Error";
  ExpectLogsAndClear({"0:Verbose", "1:Info", "2:Warning", "3:Error"});

  OSP_VLOG_IF(false) << "Verbose";
  OSP_LOG_IF(INFO, false) << "Info";
  OSP_LOG_IF(WARN, false) << "Warning";
  OSP_LOG_IF(ERROR, false) << "Error";
  ExpectNoLogs();
}

TEST_F(LoggingTest, DebugUnconditionalLogging) {
  SetLogLevel(LogLevel::kVerbose);
  OSP_DVLOG << "Verbose";
  OSP_DLOG_INFO << "Info";
  OSP_DLOG_WARN << "Warning";
  OSP_DLOG_ERROR << "Error";
#if defined(_DEBUG)
  ExpectLogsAndClear({"0:Verbose", "1:Info", "2:Warning", "3:Error"});
#else
  ExpectNoLogs();
#endif  // defined(_DEBUG)

  SetLogLevel(LogLevel::kWarning);
  OSP_DVLOG << "Verbose";
  OSP_DLOG_INFO << "Info";
  OSP_DLOG_WARN << "Warning";
  OSP_DLOG_ERROR << "Error";
#if defined(_DEBUG)
  ExpectLogsAndClear({"2:Warning", "3:Error"});
#else
  ExpectNoLogs();
#endif  // defined(_DEBUG)
}

TEST_F(LoggingTest, DebugConditionalLogging) {
  SetLogLevel(LogLevel::kVerbose);
  OSP_DVLOG_IF(true) << "Verbose";
  OSP_DLOG_IF(INFO, true) << "Info";
  OSP_DLOG_IF(WARN, true) << "Warning";
  OSP_DLOG_IF(ERROR, true) << "Error";
#if defined(_DEBUG)
  ExpectLogsAndClear({"0:Verbose", "1:Info", "2:Warning", "3:Error"});
#else
  ExpectNoLogs();
#endif  // defined(_DEBUG)

  OSP_DVLOG_IF(false) << "Verbose";
  OSP_DLOG_IF(INFO, false) << "Info";
  OSP_DLOG_IF(WARN, false) << "Warning";
  OSP_DLOG_IF(ERROR, false) << "Error";
  ExpectNoLogs();
}

TEST_F(LoggingTest, CheckAndLogFatal) {
  OSP_CHECK(false);
  ExpectBreakAndClear();
  OSP_CHECK_EQ(1, 2);
  ExpectBreakAndClear();
  OSP_CHECK_NE(1, 1);
  ExpectBreakAndClear();
  OSP_CHECK_LT(2, 1);
  ExpectBreakAndClear();
  OSP_CHECK_LE(2, 1);
  ExpectBreakAndClear();
  OSP_CHECK_GT(1, 2);
  ExpectBreakAndClear();
  OSP_CHECK_GE(1, 2);
  ExpectBreakAndClear();
  OSP_LOG_FATAL << "Fatal";
  ExpectLogsAndClear({"4:Fatal"});
  ExpectBreakAndClear();
}

TEST_F(LoggingTest, DCheckAndDLogFatal) {
  OSP_DCHECK(false);
  ExpectDebugBreakAndClear();
  OSP_DCHECK_EQ(1, 2);
  ExpectDebugBreakAndClear();
  OSP_DCHECK_NE(1, 1);
  ExpectDebugBreakAndClear();
  OSP_DCHECK_LT(2, 1);
  ExpectDebugBreakAndClear();
  OSP_DCHECK_LE(2, 1);
  ExpectDebugBreakAndClear();
  OSP_DCHECK_GT(1, 2);
  ExpectDebugBreakAndClear();
  OSP_DCHECK_GE(1, 2);
  ExpectDebugBreakAndClear();
  OSP_DLOG_FATAL << "Fatal";
#if defined(_DEBUG)
  ExpectLogsAndClear({"4:Fatal"});
#else
  ExpectNoLogs();
#endif  // defined(_DEBUG)
  ExpectDebugBreakAndClear();
}

TEST_F(LoggingTest, OspUnimplemented) {
  // Default is to log once per process if the level >= kWarning.
  SetLogLevel(LogLevel::kWarning);
  for (int i = 0; i < 2; i++) {
    OSP_UNIMPLEMENTED();
  }
  ExpectLogsAndClear({"2:LoggingTest::OspUnimplemented: UNIMPLEMENTED() hit."});

  // Setting the level to kVerbose logs every time.
  SetLogLevel(LogLevel::kVerbose);
  for (int i = 0; i < 2; i++) {
    OSP_UNIMPLEMENTED();
  }
  std::string message("0:LoggingTest::OspUnimplemented: UNIMPLEMENTED() hit.");
  ExpectLogsAndClear({message, message});
}

TEST_F(LoggingTest, OspNotReached) {
  OSP_NOTREACHED();
  ExpectLogsAndClear({"4:LoggingTest::OspNotReached: NOTREACHED() hit."});
  ExpectBreakAndClear();
}

}  // namespace openscreen

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
