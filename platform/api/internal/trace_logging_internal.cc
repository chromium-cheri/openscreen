// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/internal/trace_logging_internal.h"

#include "absl/types/optional.h"

namespace openscreen {
namespace platform {
namespace internal {

ScopedTraceOperation::ScopedTraceOperation(TraceId trace_id,
                                           TraceId parent_id,
                                           TraceId root_id) {
  // Setting trace id fields.
  root_id_ = root_id != kUnsetTraceId ? root_id : traces_.back()->root_id_;
  parent_id_ =
      parent_id != kUnsetTraceId ? parent_id : traces_.back()->trace_id_;
  trace_id_ =
      trace_id != kUnsetTraceId ? trace_id : trace_id_counter_.fetch_add(1);

  // Add this item to the stack.
  traces_.push_back(this);
}

// static
TraceId ScopedTraceOperation::GetCurrentId() {
  return traces_.back()->trace_id_;
}

// static
TraceId ScopedTraceOperation::GetRootId() {
  return traces_.back()->root_id_;
}

// static
TraceIdHierarchy ScopedTraceOperation::GetHierarchy() {
  return {traces_.back()->trace_id_, traces_.back()->parent_id_,
          traces_.back()->root_id_};
}

// static
bool ScopedTraceOperation::SetResult(Error error) {
  return traces_.back()->SetTraceResult(error);
}

// Suppress exit time destructor warning, since we require a stack initialized
// with a TraceId for perf reasons. In practice, deconstructing these 2 objects
// should take minimal time, so the benefits of using them outweigh the costs.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

// static
thread_local TraceIdSetter ScopedTraceOperation::root_node_{
    TraceIdHierarchy::Empty()};

// static
thread_local std::vector<ScopedTraceOperation*> ScopedTraceOperation::traces_{
    &ScopedTraceOperation::root_node_};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// static
std::atomic_uint64_t ScopedTraceOperation::trace_id_counter_{
    uint64_t{0x01} << (sizeof(TraceId) * 8 - 1)};

TraceLoggerBase::TraceLoggerBase(const char* name,
                                 const char* file,
                                 uint32_t line,
                                 TraceCategory::Value category,
                                 TraceId current,
                                 TraceId parent,
                                 TraceId root)
    : ScopedTraceOperation(current, parent, root),
      start_time_(Clock::now()),
      result_(Error::Code::kNone),
      name_(name),
      file_name_(file),
      line_number_(line),
      category_(category) {}

bool TraceLoggerBase::SetTraceResult(Error error) {
  result_ = error;
  return true;
}

SynchronousTraceLogger::SynchronousTraceLogger(const char* name,
                                               const char* file,
                                               uint32_t line,
                                               TraceCategory::Value category,
                                               TraceId trace_id,
                                               TraceId parent_id,
                                               TraceId root_id)
    : TraceLoggerBase(name,
                      file,
                      line,
                      category,
                      trace_id,
                      parent_id,
                      root_id) {}
SynchronousTraceLogger::SynchronousTraceLogger(const char* name,
                                               const char* file,
                                               uint32_t line,
                                               TraceCategory::Value category,
                                               TraceIdHierarchy ids)
    : TraceLoggerBase(name,
                      file,
                      line,
                      category,
                      ids.current,
                      ids.parent,
                      ids.root) {}

SynchronousTraceLogger::~SynchronousTraceLogger() {
  // If this object has an instance variable platform, use that. Otherwise,
  // use the static variable for the shared class. In practice, the instance
  // variable should only be set when testing, so branch prediction will
  // always pick the correct path in production code and it should be of
  // negligable cost.
  auto* current_platform = this->platform_ != nullptr
                               ? this->platform_
                               : TraceBase::default_trace_platform_;
  auto end_time = Clock::now();
  current_platform->LogTrace(this->name_, this->line_number_, this->file_name_,
                             this->start_time_, end_time, this->trace_id_,
                             this->parent_id_, this->root_id_, this->result_);
}

AsynchronousTraceLogger::AsynchronousTraceLogger(const char* name,
                                                 const char* file,
                                                 uint32_t line,
                                                 TraceCategory::Value category,
                                                 TraceId trace_id,
                                                 TraceId parent_id,
                                                 TraceId root_id)
    : TraceLoggerBase(name,
                      file,
                      line,
                      category,
                      trace_id,
                      parent_id,
                      root_id) {}
AsynchronousTraceLogger::AsynchronousTraceLogger(const char* name,
                                                 const char* file,
                                                 uint32_t line,
                                                 TraceCategory::Value category,
                                                 TraceIdHierarchy ids)
    : TraceLoggerBase(name,
                      file,
                      line,
                      category,
                      ids.current,
                      ids.parent,
                      ids.root) {}

AsynchronousTraceLogger::~AsynchronousTraceLogger() {
  // If this object has an instance variable platform, use that. Otherwise,
  // use the static variable for the shared class. In practice, the instance
  // variable should only be set when testing, so branch prediction will
  // always pick the correct path in production code and it should be of
  // negligable cost.
  auto* current_platform = this->platform_ != nullptr
                               ? this->platform_
                               : TraceBase::default_trace_platform_;
  current_platform->LogAsyncStart(
      this->name_, this->line_number_, this->file_name_, this->start_time_,
      this->trace_id_, this->parent_id_, this->root_id_);
}

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
