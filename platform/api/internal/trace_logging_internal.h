// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_

#include <atomic>
#include <cstring>
#include <stack>
#include <vector>

#include "absl/types/optional.h"
#include "platform/api/internal/trace_logging_user_args.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "platform/api/trace_logging_platform.h"
#include "platform/api/trace_logging_types.h"
#include "platform/base/error.h"

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
  static bool TraceAsyncEnd(const uint32_t line,
                            const char* file,
                            TraceId id,
                            Error::Code e);
};

// A base class for all trace logging objects which will create new entries in
// the Trace Hierarchy.
// 1) The sharing of all static and thread_local variables across template
// specializations.
// 2) Including all children in the same traces vector.
class ScopedTraceOperation : public TraceBase {
 public:
  ScopedTraceOperation(ScopedTraceOperation&& other);

  // Define the destructor to remove this item from the stack when it's
  // destroyed.
  ~ScopedTraceOperation() override;

  ScopedTraceOperation& operator=(ScopedTraceOperation&& other);

  // Getters the current Trace Hierarchy. If the traces_ stack hasn't been
  // created yet, return as if the empty root node is there.
  static TraceId current_id() {
    return traces_ == nullptr ? kEmptyTraceId : traces_->top()->trace_id_;
  }

  static TraceId root_id() {
    return traces_ == nullptr ? kEmptyTraceId : traces_->top()->root_id_;
  }

  static TraceIdHierarchy hierarchy() {
    if (traces_ == nullptr) {
      return TraceIdHierarchy::Empty();
    }

    return traces_->top()->to_hierarchy();
  }

  // Static method to set the result of the most recent trace.
  static void set_result(const Error& error) { set_result(error.code()); }
  static void set_result(Error::Code error) {
    if (traces_ == nullptr) {
      return;
    }
    traces_->top()->SetTraceResult(error);
  }

 protected:
  // Sets the result of this trace log.
  // NOTE: this must be define in this class rather than TraceLogger so that it
  // can be called on traces.back() without a potentially unsafe cast or type
  // checking at runtime.
  virtual void SetTraceResult(Error::Code error) = 0;

  // Constructor to set all trace id information.
  ScopedTraceOperation();
  ScopedTraceOperation(TraceId current_id, TraceId parent_id, TraceId root_id);

  // Current TraceId information.
  TraceId trace_id_;
  TraceId parent_id_;
  TraceId root_id_;

  // Determines whether this object should trace or not;
  bool is_enabled_;

  TraceIdHierarchy to_hierarchy() { return {trace_id_, parent_id_, root_id_}; }

 private:
  // NOTE: A std::vector is used for backing the stack because it provides the
  // best perf. Further perf improvement could be achieved later by swapping
  // this out for a circular buffer once OSP supports that. Additional details
  // can be found here:
  // https://www.codeproject.com/Articles/1185449/Performance-of-a-Circular-Buffer-vs-Vector-Deque-a
  using TraceStack =
      std::stack<ScopedTraceOperation*, std::vector<ScopedTraceOperation*>>;

  // Counter to pick IDs when it is not provided.
  static std::atomic<std::uint64_t> trace_id_counter_;

  // The LIFO stack of TraceLoggers currently being watched by this
  // thread.
  static thread_local TraceStack* traces_;
  static thread_local ScopedTraceOperation* root_node_;

  OSP_DISALLOW_COPY_AND_ASSIGN(ScopedTraceOperation);
};

// The class which does actual trace logging.
template <typename TArg1 = void, typename TArg2 = void>
class TraceLoggerBase : public ScopedTraceOperation {
 public:
  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  TraceId current = kUnsetTraceId,
                  TraceId parent = kUnsetTraceId,
                  TraceId root = kUnsetTraceId)
      : ScopedTraceOperation(current, parent, root),
        start_time_(Clock::now()),
        result_(Error::Code::kNone),
        name_(name),
        file_name_(file),
        line_number_(line),
        category_(category) {}

  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  const char* arg1_name,
                  absl::optional<TArg1> arg1_value,
                  TraceId current = kUnsetTraceId,
                  TraceId parent = kUnsetTraceId,
                  TraceId root = kUnsetTraceId)
      : TraceLoggerBase(category, name, file, line, current, parent, root),
        user_arg_1_(
            TraceLoggingArgumentFactory(arg1_name, arg1_value.value())) {}

  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  const char* arg1_name,
                  absl::optional<TArg1> arg1_value,
                  const char* arg2_name,
                  absl::optional<TArg1> arg2_value,
                  TraceId current = kUnsetTraceId,
                  TraceId parent = kUnsetTraceId,
                  TraceId root = kUnsetTraceId)
      : TraceLoggerBase(category,
                        name,
                        file,
                        line,
                        arg1_name,
                        arg1_value,
                        current,
                        parent,
                        root),
        user_arg_2_(
            TraceLoggingArgumentFactory(arg2_name, arg2_value.value())) {}

  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  TraceIdHierarchy ids)
      : TraceLoggerBase(category,
                        name,
                        file,
                        line,
                        ids.current,
                        ids.parent,
                        ids.root) {}

  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  const char* arg1_name,
                  absl::optional<TArg1> arg1_value,
                  TraceIdHierarchy ids)
      : TraceLoggerBase(category, name, file, line, ids),
        user_arg_1_(
            TraceLoggingArgumentFactory(arg1_name, arg1_value.value())) {}

  TraceLoggerBase(TraceCategory::Value category,
                  const char* name,
                  const char* file,
                  uint32_t line,
                  const char* arg1_name,
                  absl::optional<TArg1> arg1_value,
                  const char* arg2_name,
                  absl::optional<TArg1> arg2_value,
                  TraceIdHierarchy ids)
      : TraceLoggerBase(category, name, file, line, arg1_name, arg1_value, ids),
        user_arg_2_(
            TraceLoggingArgumentFactory(arg2_name, arg2_value.value())) {}

  TraceLoggerBase() : ScopedTraceOperation() {}

  TraceLoggerBase(TraceLoggerBase&& other)
      : ScopedTraceOperation(std::forward<ScopedTraceOperation&&>(other)) {
    if (is_enabled_) {
      start_time_ = other.start_time_;
      result_ = other.result_;
      name_ = other.name_;
      file_name_ = other.file_name_;
      line_number_ = other.line_number_;
      category_ = other.category_;
      user_arg_1_ = std::move(other.user_arg_1_);
      user_arg_2_ = std::move(other.user_arg_2_);
      other.is_enabled_ = false;
    }
  }

  ~TraceLoggerBase() override = default;

  TraceLoggerBase& operator=(TraceLoggerBase&& other) {
    ScopedTraceOperation::operator=(
        std::forward<ScopedTraceOperation&&>(other));
    if (is_enabled_) {
      start_time_ = other.start_time_;
      result_ = other.result_;
      name_ = other.name_;
      file_name_ = other.file_name_;
      line_number_ = other.line_number_;
      category_ = other.category_;
      user_arg_1_ = std::move(other.user_arg_1_);
      user_arg_2_ = std::move(other.user_arg_2_);
      other.is_enabled_ = false;
    }
    return *this;
  };

 protected:
  // Set the result.
  void SetTraceResult(Error::Code error) override { result_ = error; }

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

  // User arguments
  TraceLoggingArgInternal<TArg1> user_arg_1_;
  TraceLoggingArgInternal<TArg2> user_arg_2_;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggerBase);
};

template <typename TArg1 = void, typename TArg2 = void>
class SynchronousTraceLogger : public TraceLoggerBase<TArg1, TArg2> {
 public:
  using TraceLoggerBase<TArg1, TArg2>::TraceLoggerBase;

  SynchronousTraceLogger(SynchronousTraceLogger&& other)
      : TraceLoggerBase<TArg1, TArg2>(
            std::forward<TraceLoggerBase<TArg1, TArg2>>(other)) {}

  virtual ~SynchronousTraceLogger() override {
    if (!this->is_enabled_) {
      return;
    }
    auto* current_platform = TraceLoggingPlatform::GetDefaultTracingPlatform();
    if (current_platform == nullptr) {
      return;
    }
    auto end_time = Clock::now();
    current_platform->LogTrace(this->name_, this->line_number_,
                               this->file_name_, this->start_time_, end_time,
                               this->to_hierarchy(), this->result_);
  }

  SynchronousTraceLogger& operator=(SynchronousTraceLogger&& other) {
    TraceLoggerBase<TArg1, TArg2>::operator=(
        std::forward<TraceLoggerBase<TArg1, TArg2>>(other));
    return *this;
  };

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(SynchronousTraceLogger);
};

template <typename TArg1 = void, typename TArg2 = void>
class AsynchronousTraceLogger : public TraceLoggerBase<TArg1, TArg2> {
 public:
  using TraceLoggerBase<TArg1, TArg2>::TraceLoggerBase;

  AsynchronousTraceLogger(AsynchronousTraceLogger&& other)
      : TraceLoggerBase<TArg1, TArg2>(
            std::forward<TraceLoggerBase<TArg1, TArg2>>(other)) {}

  virtual ~AsynchronousTraceLogger() override {
    if (!this->is_enabled_) {
      return;
    }
    auto* current_platform = TraceLoggingPlatform::GetDefaultTracingPlatform();
    if (current_platform == nullptr) {
      return;
    }
    current_platform->LogAsyncStart(this->name_, this->line_number_,
                                    this->file_name_, this->start_time_,
                                    this->to_hierarchy());
  }

  AsynchronousTraceLogger& operator=(AsynchronousTraceLogger&& other) {
    TraceLoggerBase<TArg1, TArg2>::operator=(
        std::forward<TraceLoggerBase<TArg1, TArg2>>(other));
    return *this;
  };

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(AsynchronousTraceLogger);
};

// Inserts a fake element into the ScopedTraceOperation stack to set
// the current TraceId Hierarchy manually.
class TraceIdSetter : public ScopedTraceOperation {
 public:
  explicit TraceIdSetter(TraceIdHierarchy ids)
      : ScopedTraceOperation(ids.current, ids.parent, ids.root) {}
  TraceIdSetter() : ScopedTraceOperation() {}
  ~TraceIdSetter() override final;
  TraceIdSetter(TraceIdSetter&& other)
      : ScopedTraceOperation(std::forward<ScopedTraceOperation&&>(other)) {}
  TraceIdSetter& operator=(TraceIdSetter&& other) {
    ScopedTraceOperation::operator=(
        std::forward<ScopedTraceOperation&&>(other));
    return *this;
  };

  // Creates a new TraceIdSetter to set the full TraceId Hierarchy to default
  // values and does not push it to the traces stack.
  static TraceIdSetter* CreateStackRootNode();

 private:
  // Implement abstract method for use in Macros.
  void SetTraceResult(Error::Code error) override {}

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceIdSetter);
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_INTERNAL_H_
