// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_INTERNAL_H_
#define PLATFORM_API_TRACE_LOGGING_INTERNAL_H_

#include <atomic>
#include <cstring>
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

// Base class needed for Macro calls. It has been intentionally left with
// no instance variables so that construction is simple and fast.
class TraceBase {
 public:
  TraceBase() = default;

  // Traces an asynchronous call's end.
  // NOTE: This cannot be a void due to Macro behavior above, as the
  // ternary operator requires a return value.
  inline bool TraceAsyncEnd(const uint32_t line,
                            const char* file,
                            TraceId id,
                            Error e) {
    auto end_time = Clock::now();
    default_trace_platform->LogAsyncEnd(line, file, end_time, id, e);
    return true;
  }

 protected:
  // For now, only support one platform_layer. In future, we could add
  // more (ie: one to log to Chromium/etc, one to go to SQL, one for
  // Metrics, etc).
  static TraceLoggingPlatform* default_trace_platform;
};

// A base class for TraceLogging to allow for:
// 1) The sharing of all static and thread_local variables across template
// specializations.
// 2) Putting all template specializations into the same traces vector.
class ScopedTraceOperation : public TraceBase {
 public:
  // Define the destructor to remove this item from the stack when it's
  // destroyed.
  ~ScopedTraceOperation() { traces.pop_back(); }

  // Getters the current Trace Hierarchy.
  static TraceId GetCurrentId();
  static TraceId GetRootId();
  static TraceIdHierarchy GetHierarchy();

  // Static method to set the result of the most recent trace.
  static bool SetResult(Error error);

  // Prevent copying this class because that would mess with the Trace
  // Hierarchy stack.
  ScopedTraceOperation(const ScopedTraceOperation& other) = delete;
  ScopedTraceOperation& operator=(ScopedTraceOperation& other) = delete;

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
  // Counter to pick IDs when it is not provided.
  static std::atomic_uint64_t TraceIdCounter;

  // The FILO stack of TraceLoggers currently being watched by this
  // thread.
  static thread_local std::vector<ScopedTraceOperation*> traces;
  static thread_local TraceIdSetter root_node;
};

// The class which does actual trace logging.
template <TraceCategory::Value TCategory,
          uint32_t TLine,
          size_t TFileSize,
          size_t TNameSize>
class TraceLoggerBase : public ScopedTraceOperation {
 public:
  TraceLoggerBase(const char name[TNameSize],
                  const char file[TFileSize],
                  TraceId current = kUnsetTraceId,
                  TraceId parent = kUnsetTraceId,
                  TraceId root = kUnsetTraceId)
      : ScopedTraceOperation(current, parent, root) {
    this->Initialize(name, file);
  }

  // Set the result.
  bool SetTraceResult(Error error) {
    result = error;
    return true;
  }

  // Prevent copying this class, because that would mess with the Trace
  // Hierarchy stack.
  TraceLoggerBase(const TraceLoggerBase& other) = delete;
  TraceLoggerBase& operator=(const TraceLoggerBase& other) = delete;

 protected:
  // Timestamp for when the object was created.
  Clock::time_point start_time_;

  // Result of this operation.
  Error result;

  // Name of this operation.
  char name_[TNameSize];

  // Name of the file.
  char file_name_[TFileSize];

  // Trace logging layer to use for this trace log. It is overridden for
  // testing purposes, but in production code is always set to nullptr.
  TraceLoggingPlatform* platform;

 private:
  // Constructor called by others to set local variables.
  void Initialize(const char name[TNameSize], const char file_name[TFileSize]) {
    this->start_time_ = Clock::now();
    result = Error::None();
    platform = nullptr;
    strcpy(this->name_, name);
    strcpy(this->file_name_, file_name);
  }
};

template <TraceCategory::Value TCategory,
          uint32_t TLine,
          size_t TFileSize,
          size_t TNameSize>
class SynchronousTraceLogger
    : public TraceLoggerBase<TCategory, TLine, TFileSize, TNameSize> {
 public:
  SynchronousTraceLogger(const char name[TNameSize],
                         const char file[TFileSize],
                         TraceId trace_id = kUnsetTraceId,
                         TraceId parent_id = kUnsetTraceId,
                         TraceId root_id = kUnsetTraceId)
      : TraceLoggerBase<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                file,
                                                                trace_id,
                                                                parent_id,
                                                                root_id) {}
  SynchronousTraceLogger(const char name[TNameSize],
                         const char file[TFileSize],
                         TraceIdHierarchy ids)
      : TraceLoggerBase<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                file,
                                                                ids.current,
                                                                ids.parent,
                                                                ids.root) {}

  virtual ~SynchronousTraceLogger() {
    // If this object has an instance variable platform, use that. Otherwise,
    // use the static variable for the shared class. In practice, the instance
    // variable should only be set when testing, so branch prediction will
    // always pick the correct path in production code and it should be of
    // negligable cost.
    auto* current_platform = this->platform != nullptr
                                 ? this->platform
                                 : TraceBase::default_trace_platform;
    auto end_time = Clock::now();
    current_platform->LogTrace(this->name_, TLine, this->file_name_,
                               this->start_time_, end_time, this->trace_id_,
                               this->parent_id_, this->root_id_, this->result);
  }

  // Prevent copying this class, because that would mess with the Trace
  // Hierarchy stack.
  SynchronousTraceLogger(const SynchronousTraceLogger& other) = delete;
  SynchronousTraceLogger& operator=(const SynchronousTraceLogger& other) =
      delete;
};

template <TraceCategory::Value TCategory,
          uint32_t TLine,
          size_t TFileSize,
          size_t TNameSize>
class AsynchronousTraceLogger
    : public TraceLoggerBase<TCategory, TLine, TFileSize, TNameSize> {
 public:
  AsynchronousTraceLogger(const char name[TNameSize],
                          const char file[TFileSize],
                          TraceId trace_id = kUnsetTraceId,
                          TraceId parent_id = kUnsetTraceId,
                          TraceId root_id = kUnsetTraceId)
      : TraceLoggerBase<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                file,
                                                                trace_id,
                                                                parent_id,
                                                                root_id) {}
  AsynchronousTraceLogger(const char name[TNameSize],
                          const char file[TFileSize],
                          TraceIdHierarchy ids)
      : TraceLoggerBase<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                file,
                                                                ids.current,
                                                                ids.parent,
                                                                ids.root) {}

  virtual ~AsynchronousTraceLogger() {
    // If this object has an instance variable platform, use that. Otherwise,
    // use the static variable for the shared class. In practice, the instance
    // variable should only be set when testing, so branch prediction will
    // always pick the correct path in production code and it should be of
    // negligable cost.
    auto* current_platform = this->platform != nullptr
                                 ? this->platform
                                 : TraceBase::default_trace_platform;
    current_platform->LogAsyncStart(this->name_, TLine, this->file_name_,
                                    this->start_time_, this->trace_id_,
                                    this->parent_id_, this->root_id_);
  }

  // Prevent copying this class, because that would mess with the Trace
  // Hierarchy stack.
  AsynchronousTraceLogger(const AsynchronousTraceLogger& other) = delete;
  AsynchronousTraceLogger& operator=(const AsynchronousTraceLogger& other) =
      delete;
};

// Inserts a fake element into the ScopedTraceOperation stack to set
// the current TraceId Hierarchy manually.
class TraceIdSetter : public ScopedTraceOperation {
 public:
  explicit TraceIdSetter(TraceIdHierarchy ids)
      : ScopedTraceOperation(ids.current, ids.parent, ids.root) {}

  // Implement abstract method for use in Macros.
  bool SetTraceResult(Error error) { return false; }

  // Prevent copying this class because that would mess with the Trace
  // Hierarchy stack.
  TraceIdSetter(const TraceIdSetter& other) = delete;
  TraceIdSetter& operator=(const TraceIdSetter& other) = delete;
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif
