// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_

#include <atomic>
#include <cstring>
#include <stack>
#include <vector>

#include "osp_base/error.h"
#include "platform/api/time.h"
#include "platform/api/trace_logging_platform.h"
#include "platform/api/trace_logging_types.h"

namespace openscreen {
namespace platform {
namespace internal {

// Forward declaration so that it can be used in below classes.
class TraceIdSetter;

// Base class needed for macro calls. It has been intentionally left with
// no instance variables so that construction is simple and fast.
class TraceBase {
 public:
  TraceBase() = default;
  virtual ~TraceBase() = default;

  // Traces the end of an asynchronous call.
  // NOTE: This cannot be a void due to Macro behavior above, as the
  // ternary operator requires a return value.
  inline bool TraceAsyncEnd(const uint32_t line,
                            const char* file,
                            TraceId id,
                            Error e) {
    auto end_time = Clock::now();
    default_trace_platform_->LogAsyncEnd(line, file, end_time, id, e);
    return true;
  }

 protected:
  // For now, only support one TraceLoggingPlatform. In future, we could add
  // support for multiple (i.e.: one to log to Chromium/etc, one to go to SQL,
  // one for metrics, etc).
  static TraceLoggingPlatform* default_trace_platform_;
};

// A base class for TraceLogging to allow for:
// 1) The sharing of all static and thread_local variables across template
// specializations.
// 2) Putting all template specializations into the same traces vector.
class ScopedTraceOperation : public TraceBase {
 public:
  // Define the destructor to remove this item from the stack when it's
  // destroyed.
  ~ScopedTraceOperation() override;

  // Getters the current Trace Hierarchy.
  static TraceId GetCurrentId();
  static TraceId GetRootId();
  static TraceIdHierarchy GetHierarchy();

  // Static method to set the result of the most recent trace.
  static bool SetResult(Error error);

  // Sets the result of this trace log.
  // NOTE: this must be define in this class rather than TraceLogger so that it
  // can be called on traces.back() without a potentially unsafe cast or type
  // checking at runtime.
  virtual bool SetTraceResult(Error error) = 0;

 protected:
  // Constructor to set all trace id information.
  ScopedTraceOperation(TraceId current_id = kUnsetTraceId,
                       TraceId parent_id = kUnsetTraceId,
                       TraceId root_id = kUnsetTraceId);

  // Current TraceId information.
  TraceId trace_id_;
  TraceId root_id_;
  TraceId parent_id_;

 private:
  // NOTE: A std::vector is used for backing the stack because it provides the
  // best perf. Further perf improvement could be achieved later by swapping
  // this out for a circular buffer once OSP supports that. Additional details
  // can be found here:
  // https://www.codeproject.com/Articles/1185449/Performance-of-a-Circular-Buffer-vs-Vector-Deque-a
  using TraceStack =
      std::stack<ScopedTraceOperation*, std::vector<ScopedTraceOperation*>>;

  // Counter to pick IDs when it is not provided.
  static std::atomic_uint64_t trace_id_counter_;

  // The LIFO stack of TraceLoggers currently being watched by this
  // thread.
  static thread_local TraceStack traces_;
  static thread_local std::unique_ptr<ScopedTraceOperation> root_node_;

  OSP_DISALLOW_COPY_AND_ASSIGN(ScopedTraceOperation);
};

// The class which does actual trace logging.
class TraceLoggerBase : public ScopedTraceOperation {
 public:
  TraceLoggerBase(const char* name,
                  const char* file,
                  uint32_t line,
                  TraceCategory::Value category,
                  TraceId current = kUnsetTraceId,
                  TraceId parent = kUnsetTraceId,
                  TraceId root = kUnsetTraceId);

  TraceLoggerBase(const char* name,
                  const char* file,
                  uint32_t line,
                  TraceCategory::Value category,
                  TraceIdHierarchy ids);

  // Set the result.
  bool SetTraceResult(Error error);

 protected:
  // Timestamp for when the object was created.
  Clock::time_point start_time_;

  // Result of this operation.
  Error result_;

  // Name of this operation.
  const char* name_;

  // Name of the file.
  const char* file_name_;

  // Line number the log was generated from.
  uint32_t line_number_;

  // Category of this trace log.
  TraceCategory::Value category_;

  // Trace logging layer to use for this trace log. It is overridden for
  // testing purposes, but in production code is always set to nullptr.
  TraceLoggingPlatform* platform_override_ = nullptr;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggerBase);
};

class SynchronousTraceLogger : public TraceLoggerBase {
 public:
  using TraceLoggerBase::TraceLoggerBase;

  virtual ~SynchronousTraceLogger() override;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(SynchronousTraceLogger);
};

class AsynchronousTraceLogger : public TraceLoggerBase {
 public:
  using TraceLoggerBase::TraceLoggerBase;

  virtual ~AsynchronousTraceLogger() override;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(AsynchronousTraceLogger);
};

// Inserts a fake element into the ScopedTraceOperation stack to set
// the current TraceId Hierarchy manually.
class TraceIdSetter : public ScopedTraceOperation {
 public:
  explicit TraceIdSetter(TraceIdHierarchy ids)
      : ScopedTraceOperation(ids.current, ids.parent, ids.root) {}

  // Implement abstract method for use in Macros.
  bool SetTraceResult(Error error) { return false; }

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(TraceIdSetter);
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_
