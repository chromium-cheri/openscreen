// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/constants.h"

namespace openscreen {
namespace cast {

std::ostream& operator<<(std::ostream& os, AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAac:
      return os << "Aac";
    case AudioCodec::kOpus:
      return os << "Opus";
    case AudioCodec::kNotSpecified:
      return os << "Not Specified";
  }
}

std::ostream& operator<<(std::ostream& os, VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return os << "H264";
    case VideoCodec::kVp8:
      return os << "Vp8";
    case VideoCodec::kHevc:
      return os << "Hevc";
    case VideoCodec::kVp9:
      return os << "Vp9";
    case VideoCodec::kAv1:
      return os << "Av1";
    case VideoCodec::kNotSpecified:
      return os << "Not Specified";
  }
}

std::ostream& operator<<(std::ostream& os, CastMode cast_mode) {
  switch (cast_mode) {
    case CastMode::kMirroring:
      return os << "Mirroring";
    case CastMode::kRemoting:
      return os << "Remoting";
  }
}

}  // namespace cast
}  // namespace openscreen
