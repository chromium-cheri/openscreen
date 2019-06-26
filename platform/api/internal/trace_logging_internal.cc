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

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
