// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/trace_logging_types.h"

#include <stdint.h>

#include <cassert>
#include <limits>

namespace openscreen {

bool TraceIdHierarchy::HasCurrent() const {
  return current != kUnsetTraceId;
}
bool TraceIdHierarchy::HasParent() const {
  return parent != kUnsetTraceId;
}
bool TraceIdHierarchy::HasRoot() const {
  return root != kUnsetTraceId;
}

std::string TraceIdHierarchy::ToString() const {
  std::stringstream ss;
  ss << "[" << std::hex << root << ":" << parent << ":" << current << "]";
  return ss.str();
}

std::ostream& operator<<(std::ostream& out, const TraceIdHierarchy& ids) {
  return out << "[" << std::hex << (ids.HasRoot() ? ids.root : 0) << ":"
             << (ids.HasParent() ? ids.parent : 0) << ":"
             << (ids.HasCurrent() ? ids.current : 0) << std::dec << "]";
}

bool operator==(const TraceIdHierarchy& lhs, const TraceIdHierarchy& rhs) {
  return lhs.current == rhs.current && lhs.parent == rhs.parent &&
         lhs.root == rhs.root;
}

bool operator!=(const TraceIdHierarchy& lhs, const TraceIdHierarchy& rhs) {
  return !(lhs == rhs);
}

const char* ToString(TraceCategory::Value category) {
  switch (category) {
    case TraceCategory::Value::kAny:
      return "ANY";
    case TraceCategory::Value::kMdns:
      return "Mdns";
    case TraceCategory::Value::kQuic:
      return "Quic";
    case TraceCategory::Value::kSsl:
      return "SSL";
    case TraceCategory::Value::kPresentation:
      return "Presentation";
    case TraceCategory::Value::kStandaloneReceiver:
      return "StandaloneReceiver";
    case TraceCategory::Value::kDiscovery:
      return "Discovery";
    case TraceCategory::Value::kStandaloneSender:
      return "StandaloneSender";
    case TraceCategory::Value::kReceiver:
      return "Receiver";
    case TraceCategory::Value::kSender:
      return "Sender";
  }

  assert(false);
  return "";
}

}  // namespace openscreen
