// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RECEIVER_DEMO_DUMMY_PLAYER_H_
#define STREAMING_CAST_RECEIVER_DEMO_DUMMY_PLAYER_H_

#include <stdint.h>

#include <vector>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "streaming/cast/receiver.h"

namespace openscreen {
namespace cast_streaming {

// Consumes frames from a Receiver, but does nothing other than OSP_LOG_INFO
// each one's FrameId, timestamp and size. This is only useful for confirming a
// Receiver is successfully receiving a stream, for platforms where
// SDLVideoPlayer cannot be built.
class DummyPlayer : public Receiver::Consumer {
 public:
  DummyPlayer(platform::ClockNowFunctionPtr now_function,
              platform::TaskRunner* task_runner,
              Receiver* receiver);

  ~DummyPlayer() final;

 private:
  // Receiver::Consumer implementation.
  void OnFrameComplete(FrameId frame_id) final;

  // Consumes zero or more frames from the Receiver and logs each.
  void ConsumeMoreFrames();

  const platform::ClockNowFunctionPtr now_;
  Receiver* const receiver_;

  Alarm consume_alarm_;
  std::vector<uint8_t> buffer_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RECEIVER_DEMO_DUMMY_PLAYER_H_
