// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RECEIVER_DEMO_SDL_VIDEO_PLAYER_H_
#define STREAMING_CAST_RECEIVER_DEMO_SDL_VIDEO_PLAYER_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <string>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "streaming/cast/receiver.h"
#include "streaming/cast/receiver_demo/decoder.h"
#include "streaming/cast/receiver_demo/sdl_glue.h"

namespace openscreen {
namespace cast_streaming {

// Consumes frames from a Receiver, decodes them, and renders them to an
// SDL_Renderer.
class SDLVideoPlayer : public Receiver::Consumer, public Decoder::Client {
 public:
  // |error_callback| is run only if a fatal error occurs, at which point the
  // player has halted and set |error_status()|.
  SDLVideoPlayer(platform::ClockNowFunctionPtr now_function,
                 platform::TaskRunner* task_runner,
                 Receiver* receiver,
                 SDL_Renderer* renderer,
                 std::function<void()> error_callback);

  ~SDLVideoPlayer() final;

  // Returns OK unless a fatal error has occurred.
  const Error& error_status() const { return error_status_; }

 private:
  // Receiver::Consumer implementation.
  void OnFrameComplete(FrameId frame_id) final;

  void ResumeDecodingIfNothingToPlay();
  void ResumeRendering();

  // If the playout queue is empty and the Receiver has frames ready, consume
  // them from the Receiver, and start decoding them.
  void TryDecodingAnotherFrame();

  // AVCodecDecoder::Client implementation. These are called-back from
  // |decoder_| to provide results.
  void OnFrameDecoded(FrameId frame_id, const AVFrame& frame) final;
  void OnDecodeError(FrameId frame_id, std::string message) final;
  void OnFatalError(std::string message) final;

  // Renders a decoded frame, scheduling its presentation. If no decoded frame
  // is available, this delegates to IdleRender().
  void Render();

  // Renders the "blue splash" (if waiting) or "red splash" (on error), or
  // otherwise re-renders the currently-presented frame; scheduling presentation
  // at an "idle FPS" rate.
  void IdleRender();

  // Uploads |picture_| to a SDL texture and draws it using the SDL |renderer_|.
  // Returns false on error.
  bool Draw();

  // Makes whatever is currently drawn to the SDL |renderer_| be presented
  // on-screen.
  void Present();

  // Maps an AVFrame format enum to the SDL equivalent.
  static uint32_t GetSDLPixelFormat(const AVFrame& picture);

  const platform::ClockNowFunctionPtr now_;
  Receiver* const receiver_;
  SDL_Renderer* const renderer_;

  // Current player state, which is used to determine what to render/present,
  // and how frequently.
  enum {
    kWaitingForFirstFrame,  // Render "blue splash" screen at idle FPS.
    kScheduledToPresent,    // Render new content at an exact time point.
    kPresented,             // Render same frame at idle FPS.
    kError,                 // Render "red splash" screen at idle FPS.
  } state_ = kWaitingForFirstFrame;

  // Run once by OnFatalError().
  std::function<void()> error_callback_;

  // Set to the error code that placed the player in a fatal error state.
  Error error_status_;

  // Queue of frames currently being decoded and decoded frames awaiting
  // rendering.
  struct PendingFrame {
    platform::Clock::time_point start_time;
    platform::Clock::time_point presentation_time;
    AVFrameUniquePtr decoded_frame;

    PendingFrame();
    ~PendingFrame();
    PendingFrame(const PendingFrame&) = delete;
    PendingFrame(PendingFrame&&) = delete;
  };
  std::map<FrameId, PendingFrame> frames_to_render_;

  // Buffer for holding EncodedFrame::data.
  Decoder::Buffer buffer_;

  // Associates a RTP timestamp with a local clock time point. This is updated
  // whenever the media (RTP) timestamps drift too much away from the rate at
  // which the local clock ticks. This is important for A/V synchronization.
  RtpTimeTicks last_sync_rtp_timestamp_{};
  platform::Clock::time_point last_sync_reference_time_{};

  Decoder decoder_;

  // The decoded frame's image that should be shown at the next Present().
  AVFrameUniquePtr picture_;

  // The SDL texture to which the |picture_| is uploaded for accelerated 2D
  // rendering.
  SDLTextureUniquePtr texture_;

  // A cumulative moving average of recent single-frame processing times
  // (consume + decode + render). This is passed to the Cast Receiver so that it
  // can determine when to drop late frames.
  platform::Clock::duration recent_processing_time_{};

  // Alarms that execute the various stages of the player pipeline at certain
  // times.
  Alarm decode_alarm_;
  Alarm render_alarm_;
  Alarm presentation_alarm_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RECEIVER_DEMO_SDL_VIDEO_PLAYER_H_
