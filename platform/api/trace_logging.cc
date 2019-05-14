// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging.h"

#include "absl/types/optional.h"

namespace openscreen {
namespace platform {

// static
TraceIdHierarchy TraceIdHierarchy::Empty() {
  return {EmptyTraceId, EmptyTraceId, EmptyTraceId};
}

TraceIdHierarchyHandler::TraceIdHierarchyHandler(
    absl::optional<TraceId> trace_id,
    absl::optional<TraceId> parent_id,
    absl::optional<TraceId> root_id) {
  // Setting trace id fields.
  root_id_ =
      root_id != absl::nullopt ? root_id.value() : traces.back()->root_id_;
  parent_id_ =
      parent_id != absl::nullopt ? parent_id.value() : traces.back()->trace_id_;
  trace_id_ = trace_id != absl::nullopt ? trace_id.value()
                                        : TraceIdCounter.fetch_add(1);

  // Add this item to the stack.
  traces.push_back(this);
}

// static
TraceId TraceIdHierarchyHandler::GetCurrentId() {
  return traces.back()->trace_id_;
}

// static
TraceId TraceIdHierarchyHandler::GetRootId() {
  return traces.back()->root_id_;
}

// static
TraceIdHierarchy TraceIdHierarchyHandler::GetHierarchy() {
  return {traces.back()->trace_id_, traces.back()->parent_id_,
          traces.back()->root_id_};
}

// static
bool TraceIdHierarchyHandler::SetResult(Error error) {
  return traces.back()->SetTraceResult(error);
}

// static
std::vector<TraceIdHierarchyHandler*>
TraceIdHierarchyHandler::InitializeTraces() {
  auto vector = std::vector<TraceIdHierarchyHandler*>();
  vector.push_back(&TraceIdHierarchyHandler::root_node);
  return vector;
}

// Suppress exit time destructor warning, since we require a stack initialized
// with a TraceId for perf reasons. In practice, deconstructing these 2 objects
// should take minimal time, so the benefits of using them outweigh the costs.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

// static
thread_local TraceIdSetter TraceIdHierarchyHandler::root_node{
    TraceIdHierarchy::Empty()};

// static
thread_local auto TraceIdHierarchyHandler::traces =
    TraceIdHierarchyHandler::InitializeTraces();

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// static
std::atomic_uint64_t TraceIdHierarchyHandler::TraceIdCounter{
    uint64_t{0x01} << (sizeof(TraceId) * 8 - 1)};

}  // namespace platform
}  // namespace openscreen
