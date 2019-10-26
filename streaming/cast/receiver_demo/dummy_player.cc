// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver_demo/dummy_player.h"

#include <chrono>

#include "absl/types/span.h"
#include "platform/api/logging.h"
#include "streaming/cast/encoded_frame.h"

using std::chrono::microseconds;
using std::chrono::milliseconds;

using openscreen::platform::ClockNowFunctionPtr;
using openscreen::platform::TaskRunner;

namespace openscreen {
namespace cast_streaming {

DummyPlayer::DummyPlayer(ClockNowFunctionPtr now_function,
                         TaskRunner* task_runner,
                         Receiver* receiver)
    : now_(now_function),
      receiver_(receiver),
      consume_alarm_(now_, task_runner) {
  OSP_DCHECK(receiver_);
  receiver_->SetConsumer(this);
  consume_alarm_.Schedule([this] { ConsumeMoreFrames(); }, {});
}

DummyPlayer::~DummyPlayer() {
  receiver_->SetConsumer(nullptr);
}

void DummyPlayer::OnFrameComplete(FrameId frame_id) {
  consume_alarm_.Schedule([this] { ConsumeMoreFrames(); }, {});
}

void DummyPlayer::ConsumeMoreFrames() {
  for (;;) {
    // See if a frame is ready to be consumed. If not, reschedule a later check.
    // The extra "poll" is necessary because it's possible the Receiver will
    // decide to skip frames at a later time, to unblock things.
    const int buffer_size = receiver_->AdvanceToNextFrame();
    if (buffer_size == Receiver::kNoFramesReady) {
      // TODO: This interval shouldn't be a constant, but should be 1/max_fps.
      constexpr auto kConsumeKickstartInterval = milliseconds(10);
      consume_alarm_.Schedule([this] { ConsumeMoreFrames(); },
                              now_() + kConsumeKickstartInterval);
      return;
    }

    // Consume the next frame.
    buffer_.resize(buffer_size);
    EncodedFrame frame;
    frame.data = absl::Span<uint8_t>(buffer_);
    receiver_->ConsumeNextFrame(&frame);

    // Convert the RTP timestamp to a human-readable timestamp (in µs) and log
    // some short information about the frame.
    const auto media_timestamp =
        frame.rtp_timestamp.ToTimeSinceOrigin<microseconds>(
            receiver_->rtp_timebase());
    OSP_LOG_INFO << "[SSRC " << receiver_->ssrc() << "] "
                 << (frame.dependency == EncodedFrame::KEY_FRAME ? "KEY " : "")
                 << frame.frame_id << " at " << media_timestamp.count()
                 << "µs, " << buffer_size << " bytes";
  }
}

}  // namespace cast_streaming
}  // namespace openscreen
