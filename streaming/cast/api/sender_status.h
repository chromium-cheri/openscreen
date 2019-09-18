// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_SENDER_STATUS_H_
#define STREAMING_CAST_PLATFORM_API_SENDER_STATUS_H_

namespace openscreen {
namespace cast_streaming {

enum class SenderStatus {
  // Client should not send frames yet.
  kUninitialized = 0,

  // Client may now send frames.
  kInitialized,

  // Codec is being reinitialized. Frames may be queued, but may be dropped
  // if flooded before return to kStatusInitialized.
  kCodecReinitPending,

  // Sender has halted due to an issue.
  kHalted
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_SENDER_STATUS_H_
