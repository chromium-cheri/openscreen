// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_PLATFORM_H_
#define PLATFORM_API_TRACE_LOGGING_PLATFORM_H_

#include "osp_base/error.h"
#include "platform/api/time.h"
#include "platform/api/trace_logging_platform.h"
#include "platform/api/trace_logging_types.h"

namespace openscreen {
namespace platform {

// Macro to set the default platform implementation used for Trace Logging.
#define TRACE_SET_DEFAULT_PLATFORM(platform)                               \
  TraceLoggingPlatform* traceinternal::TraceBase::default_trace_platform = \
      platform;

// Platform API base class. The platform will be an extension of this
// method, with an instantiation of that class being provided to the below
// TracePlatformWrapper class.
class TraceLoggingPlatform {
 public:
  virtual void LogTrace(const char* name,
                        const uint32_t line,
                        const char* file,
                        Clock::time_point start_time,
                        Clock::time_point end_time,
                        TraceId trace_id,
                        TraceId parent_id,
                        TraceId root_id,
                        Error error) = 0;

  virtual void LogAsyncStart(const char* name,
                             const uint32_t line,
                             const char* file,
                             Clock::time_point timestamp,
                             TraceId trace_id,
                             TraceId parent_id,
                             TraceId root_id) = 0;

  virtual void LogAsyncEnd(const uint32_t line,
                           const char* file,
                           Clock::time_point timestamp,
                           TraceId trace_id,
                           Error error) = 0;
};

bool IsLoggingEnabled(TraceCategory::Value category);

}  // namespace platform
}  // namespace openscreen

#endif
