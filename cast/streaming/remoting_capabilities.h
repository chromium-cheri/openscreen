// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_REMOTING_CAPABILITIES_H_
#define CAST_STREAMING_REMOTING_CAPABILITIES_H_

#include <string>
#include <vector>

namespace openscreen {
namespace cast {

enum class AudioCapability {
  kBaselineSet,
  kAac,
  kOpus,
};

enum class VideoCapability {
  kBaselineSet,
  kSupports4k,
  kH264,
  kVp8,
  kVp9,
  kHevc
};

// This class is similar to the RemotingSinkMetadata in Chrome, however
// it is focused around our needs and is not mojom-based. This contains
// a rough set of capabilities of the receiver to give the sender an idea of
// what features are suppported for remoting.
// TODO(issuetracker.google.com/184189100): this object should be expanded to
// allow more specific contraint tracking.
struct RemotingCapabilities {
  // Receiver audio-specific capabilities.
  std::vector<AudioCapability> audio;

  // Receiver video-specific capabilities.
  std::vector<VideoCapability> video;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_REMOTING_CAPABILITIES_H_
