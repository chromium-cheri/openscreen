// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_ENCODER_CAPABILITIES_H_
#define STREAMING_CAST_PLATFORM_API_ENCODER_CAPABILITIES_H_

#include "platform/base/error.h"
#include "streaming/cast/environment.h"
#include "platform/api/time.h"
#include "absl/strings/string_view.h"

namespace openscreen {
namespace cast_streaming {

 struct EncoderCapabilities {
   // TODO: define capabilities
   std::unordered_map<std::string, std::string> capabilities;

  std::string name;

}

// TODO(jophba): define advanced encoder interface
// TODO(jophba): add capabilities

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_ENCODER_CAPABILITIES_H_
