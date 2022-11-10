// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/trace_logging_types.h"

#include <stdint.h>

#include <cassert>
#include <limits>

namespace openscreen {
namespace {}

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

const char* ToString(TraceCategory category) {
  switch (category) {
    case TraceCategory::kAny:
      return "ANY";
    case TraceCategory::kMdns:
      return "Mdns";
    case TraceCategory::kQuic:
      return "Quic";
    case TraceCategory::kSsl:
      return "SSL";
    case TraceCategory::kPresentation:
      return "Presentation";
    case TraceCategory::kStandaloneReceiver:
      return "StandaloneReceiver";
    case TraceCategory::kDiscovery:
      return "Discovery";
    case TraceCategory::kStandaloneSender:
      return "StandaloneSender";
    case TraceCategory::kReceiver:
      return "Receiver";
    case TraceCategory::kSender:
      return "Sender";
  }
}

}  // namespace openscreen
