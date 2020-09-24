// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/message_fields.h"

#include <array>
#include <utility>

#include "absl/strings/string_view.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr std::array<std::pair<absl::string_view, AudioCodec>, 2>
    kAudioCodecNames{{{"aac", AudioCodec::kAac}, {"opus", AudioCodec::kOpus}}};

constexpr std::array<std::pair<absl::string_view, VideoCodec>, 4>
    kVideoCodecNames{{{"h264", VideoCodec::kH264},
                      {"vp8", VideoCodec::kVp8},
                      {"hevc", VideoCodec::kHevc},
                      {"vp9", VideoCodec::kVp9}}};

constexpr char kUnknownCodecError[] = "Codec not accounted for in name array.";

template <typename T, size_t size>
std::string GetCodecName(
    const std::array<std::pair<absl::string_view, T>, size>& codecs,
    T codec) {
  for (auto pair : codecs) {
    if (pair.second == codec) {
      return std::string(pair.first);
    }
  }
  OSP_NOTREACHED() << kUnknownCodecError;
  return {};
}

template <typename T, size_t size>
T GetCodec(const std::array<std::pair<absl::string_view, T>, size>& codecs,
           absl::string_view name) {
  for (auto pair : codecs) {
    if (pair.first == name) {
      return pair.second;
    }
  }
  OSP_NOTREACHED() << kUnknownCodecError;
  return {};
}

}  // namespace

std::string CodecToString(AudioCodec codec) {
  return GetCodecName(kAudioCodecNames, codec);
}

AudioCodec StringToAudioCodec(const std::string& name) {
  return GetCodec(kAudioCodecNames, name);
}

std::string CodecToString(VideoCodec codec) {
  return GetCodecName(kVideoCodecNames, codec);
}

VideoCodec StringToVideoCodec(const std::string& name) {
  return GetCodec(kVideoCodecNames, name);
}

}  // namespace cast
}  // namespace openscreen
