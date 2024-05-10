// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/constants.h"

#include <ostream>

namespace openscreen::cast {

std::ostream& operator<<(std::ostream& os, VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      os << "H264";
    case VideoCodec::kVp8:
      os << "VP8";
    case VideoCodec::kHevc:
      os << "HEVC";
    case VideoCodec::kNotSpecified:
      os << "NotSpecified";
    case VideoCodec::kVp9:
      os << "VP9";
    case VideoCodec::kAv1:
      os << "AV1";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, CastMode mode) {
  switch (mode) {
    case CastMode::kMirroring:
      os << "mirroring";
    case CastMode::kRemoting:
      os << "remoting";
  }
  return os;
}

}  // namespace openscreen::cast
