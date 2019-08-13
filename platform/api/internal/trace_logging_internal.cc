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
ScopedTraceOperation::ScopedTraceOperation() : is_enabled_(false) {}

ScopedTraceOperation::ScopedTraceOperation(TraceId trace_id,
                                           TraceId parent_id,
                                           TraceId root_id)
    : is_enabled_(true) {
  if (traces_ == nullptr) {
    // Create the stack if it doesnt' exist.
    traces_ = new TraceStack();

    // Create a new root node. This will re-call this constructor and add the
    // root node to the stack before proceeding with the original node.
    root_node_ = new TraceIdSetter(TraceIdHierarchy::Empty());
    OSP_DCHECK(!traces_->empty());
  }

  // Setting trace id fields.
  root_id_ = root_id != kUnsetTraceId ? root_id : traces_->top()->root_id_;
  parent_id_ =
      parent_id != kUnsetTraceId ? parent_id : traces_->top()->trace_id_;
  trace_id_ =
      trace_id != kUnsetTraceId ? trace_id : trace_id_counter_.fetch_add(1);

  // Add this item to the stack.
  traces_->push(this);
  OSP_DCHECK(traces_->size() < 1024);
}

ScopedTraceOperation::ScopedTraceOperation(ScopedTraceOperation&& other) {
  is_enabled_ = other.is_enabled_;
  if (other.is_enabled_) {
    trace_id_ = other.trace_id_;
    parent_id_ = other.parent_id_;
    root_id_ = other.root_id_;
  }
}

ScopedTraceOperation::~ScopedTraceOperation() {
  if (!is_enabled_) {
    return;
  }

  OSP_CHECK(traces_ != nullptr && !traces_->empty());
  OSP_CHECK_EQ(traces_->top(), this);
  traces_->pop();

  // If there's only one item left, it must be the root node. Deleting the root
  // node will re-call this destructor and delete the traces_ stack.
  if (traces_->size() == 1) {
    OSP_CHECK_EQ(traces_->top(), root_node_);
    delete root_node_;
    root_node_ = nullptr;
  } else if (traces_->empty()) {
    delete traces_;
    traces_ = nullptr;
  }
}

ScopedTraceOperation& ScopedTraceOperation::operator=(
    ScopedTraceOperation&& other) {
  is_enabled_ = other.is_enabled_;
  if (other.is_enabled_) {
    trace_id_ = other.trace_id_;
    parent_id_ = other.parent_id_;
    root_id_ = other.root_id_;
  }
  return *this;
};

// static
thread_local ScopedTraceOperation::TraceStack* ScopedTraceOperation::traces_ =
    nullptr;

// static
thread_local ScopedTraceOperation* ScopedTraceOperation::root_node_ = nullptr;

// static
std::atomic<std::uint64_t> ScopedTraceOperation::trace_id_counter_{
    uint64_t{0x01} << (sizeof(TraceId) * 8 - 1)};

TraceIdSetter::~TraceIdSetter() = default;

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
