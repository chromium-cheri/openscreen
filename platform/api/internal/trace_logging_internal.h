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

// Base class needed for macro calls. It has been intentionally left with
// no instance variables so that construction is simple and fast.
class TraceBase {
 public:
  TraceBase() = default;
  virtual ~TraceBase() = default;

  // Traces the end of an asynchronous call.
  // NOTE: This returns a bool rather than a void because it keeps the syntax of
  // the ternary operator in the macros simpler.
  inline bool TraceAsyncEnd(const uint32_t line,
                            const char* file,
                            TraceId id,
                            Error::Code e) {
    auto end_time = Clock::now();
    TraceLoggingPlatform::GetDefaultTracingPlatform()->LogAsyncEnd(
        line, file, end_time, id, e);
    return true;
  }
};

// A base class for all trace logging objects which will create new entries in
// the Trace Hierarchy.
// 1) The sharing of all static and thread_local variables across template
// specializations.
// 2) Including all children in the same traces vector.
class ScopedTraceOperation : public TraceBase {
 public:
  // Define the destructor to remove this item from the stack when it's
  // destroyed.
  ~ScopedTraceOperation() override;

  // Getters the current Trace Hierarchy.
  static TraceId GetCurrentId() { return GetTraceStack()->top()->trace_id_; }
  static TraceId GetRootId() { return GetTraceStack()->top()->root_id_; }
  static TraceIdHierarchy GetHierarchy() {
    auto* top_of_stack = GetTraceStack()->top();
    return {top_of_stack->trace_id_, top_of_stack->parent_id_,
            top_of_stack->root_id_};
  }

  // Static method to set the result of the most recent trace.
  static void SetResult(Error error) { SetResult(error.code()); }
  static void SetResult(Error::Code error) {
    GetTraceStack()->top()->SetTraceResult(error);
  }

 protected:
  // Sets the result of this trace log.
  // NOTE: this must be define in this class rather than TraceLogger so that it
  // can be called on traces.back() without a potentially unsafe cast or type
  // checking at runtime.
  virtual void SetTraceResult(Error::Code error) = 0;

  // Constructor to set all trace id information.
  ScopedTraceOperation(TraceId current_id = kUnsetTraceId,
                       TraceId parent_id = kUnsetTraceId,
                       TraceId root_id = kUnsetTraceId,
                       bool push_to_stack = true);

  // Current TraceId information.
  TraceId trace_id_;
  TraceId parent_id_;
  TraceId root_id_;

 private:
  // NOTE: A std::vector is used for backing the stack because it provides the
  // best perf. Further perf improvement could be achieved later by swapping
  // this out for a circular buffer once OSP supports that. Additional details
  // can be found here:
  // https://www.codeproject.com/Articles/1185449/Performance-of-a-Circular-Buffer-vs-Vector-Deque-a
  using TraceStack =
      std::stack<ScopedTraceOperation*, std::vector<ScopedTraceOperation*>>;

  // The LIFO stack of TraceLoggers currently being watched by this
  // thread.
  static TraceStack* GetTraceStack();

  // Counter to pick IDs when it is not provided.
  static std::atomic<std::uint64_t> trace_id_counter_;

  OSP_DISALLOW_COPY_AND_ASSIGN(ScopedTraceOperation);
};

// The class which does actual trace logging.
class TraceLoggerBase : public ScopedTraceOperation {
 public:
  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  TraceId current = kUnsetTraceId,
                  TraceId parent = kUnsetTraceId,
                  TraceId root = kUnsetTraceId);

  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  TraceIdHierarchy ids);

 protected:
  // Set the result.
  void SetTraceResult(Error::Code error) { result_ = error; }

  // Timestamp for when the object was created.
  Clock::time_point start_time_;

  // Result of this operation.
  Error::Code result_;

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
  explicit TraceIdSetter(TraceIdHierarchy ids) : TraceIdSetter(ids, true) {}
  ~TraceIdSetter() final;

  // Creates a new TraceIdSetter to set the full TraceId Hierarchy to default
  // values and does not push it to the traces stack.
  static TraceIdSetter* CreateStackRootNode();

 private:
  explicit TraceIdSetter(TraceIdHierarchy ids, bool push_to_stack)
      : ScopedTraceOperation(ids.current, ids.parent, ids.root, push_to_stack) {
  }

  // Implement abstract method for use in Macros.
  void SetTraceResult(Error::Code error) {}

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceIdSetter);
};

// This helper object allows us to delete objects allocated on the stack in a
// unique_ptr.
class TraceBaseStackDeleter {
 public:
  void operator()(TraceBase* ptr) { ptr->~TraceBase(); }
};
using TraceInstance = std::unique_ptr<TraceBase, TraceBaseStackDeleter>;

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_
