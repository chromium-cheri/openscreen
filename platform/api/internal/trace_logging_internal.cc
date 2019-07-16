// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/internal/trace_logging_internal.h"

#include "absl/types/optional.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace internal {

// static
bool TraceBase::TraceAsyncEnd(const uint32_t line,
                              const char* file,
                              TraceId id,
                              Error::Code e) {
  auto end_time = Clock::now();
  auto current_platform = TraceLoggingPlatform::GetDefaultTracingPlatform();
  if (current_platform == nullptr) {
    return false;
  }
  current_platform->LogAsyncEnd(line, file, end_time, id, e);
  return true;
}

ScopedTraceOperation::ScopedTraceOperation(TraceId trace_id,
                                           TraceId parent_id,
                                           TraceId root_id,
                                           bool push_to_stack) {
  // Setting trace id fields.
  root_id_ =
      root_id != kUnsetTraceId ? root_id : GetTraceStack()->top()->root_id_;
  parent_id_ = parent_id != kUnsetTraceId ? parent_id
                                          : GetTraceStack()->top()->trace_id_;
  trace_id_ =
      trace_id != kUnsetTraceId ? trace_id : trace_id_counter_.fetch_add(1);

  // Add this item to the stack.
  if (push_to_stack) {
    GetTraceStack()->push(this);
    OSP_DCHECK(GetTraceStack()->size() < 1024);
  }
}

ScopedTraceOperation::~ScopedTraceOperation() {
  GetTraceStack()->pop();
}

// Suppress exit time destructor warning, since we require a stack initialized
// with a TraceId for perf reasons. In practice, deconstructing these 2 objects
// should take minimal time, so the benefits of using them outweigh the costs.
//
// TODO(rwkeane): Once OSP has imported base/lazy_instance.h or
// base/no_desctructor.h, remove this suppression.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

// static
ScopedTraceOperation::TraceStack* ScopedTraceOperation::GetTraceStack() {
  static thread_local std::unique_ptr<TraceIdSetter> root_node(
      TraceIdSetter::CreateStackRootNode());
  static thread_local ScopedTraceOperation::TraceStack traces{
      {root_node.get()}};
  return &traces;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// static
std::atomic<std::uint64_t> ScopedTraceOperation::trace_id_counter_{
    uint64_t{0x01} << (sizeof(TraceId) * 8 - 1)};

TraceLoggerBase::TraceLoggerBase(TraceCategory::Value category,
                                 const char* name,
                                 const char* file,
                                 uint32_t line,
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

TraceLoggerBase::TraceLoggerBase(TraceCategory::Value category,
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

SynchronousTraceLogger::~SynchronousTraceLogger() {
  // If this object has an instance variable platform, use that. Otherwise,
  // use the static variable for the shared class. In practice, the instance
  // variable should only be set when testing, so branch prediction will
  // always pick the correct path in production code and it should be of
  // negligable cost.
  auto* current_platform =
      this->platform_override_ != nullptr
          ? this->platform_override_
          : TraceLoggingPlatform::GetDefaultTracingPlatform();
  if (current_platform == nullptr) {
    return;
  }
  auto end_time = Clock::now();
  current_platform->LogTrace(this->name_, this->line_number_, this->file_name_,
                             this->start_time_, end_time, this->trace_id_,
                             this->parent_id_, this->root_id_, this->result_);
}

AsynchronousTraceLogger::~AsynchronousTraceLogger() {
  // If this object has an instance variable platform, use that. Otherwise,
  // use the static variable for the shared class. In practice, the instance
  // variable should only be set when testing, so branch prediction will
  // always pick the correct path in production code and it should be of
  // negligable cost.
  auto* current_platform =
      this->platform_override_ != nullptr
          ? this->platform_override_
          : TraceLoggingPlatform::GetDefaultTracingPlatform();
  if (current_platform == nullptr) {
    return;
  }
  current_platform->LogAsyncStart(
      this->name_, this->line_number_, this->file_name_, this->start_time_,
      this->trace_id_, this->parent_id_, this->root_id_);
}

TraceIdSetter::~TraceIdSetter() = default;

// static
TraceIdSetter* TraceIdSetter::CreateStackRootNode() {
  return new TraceIdSetter(TraceIdHierarchy::Empty(), false);
}

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
