// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver_demo/sdl_video_player.h"

#include <chrono>
#include <sstream>

#include "absl/types/span.h"
#include "platform/api/logging.h"
#include "streaming/cast/encoded_frame.h"
#include "streaming/cast/receiver_demo/avcodec_glue.h"

using std::chrono::duration_cast;
using std::chrono::milliseconds;

using openscreen::platform::Clock;
using openscreen::platform::ClockNowFunctionPtr;
using openscreen::platform::TaskRunner;

namespace openscreen {
namespace cast_streaming {

SDLVideoPlayer::SDLVideoPlayer(ClockNowFunctionPtr now_function,
                               TaskRunner* task_runner,
                               Receiver* receiver,
                               SDL_Renderer* renderer,
                               std::function<void()> error_callback)
    : now_(now_function),
      receiver_(receiver),
      renderer_(renderer),
      error_callback_(std::move(error_callback)),
      decode_alarm_(now_, task_runner),
      render_alarm_(now_, task_runner),
      presentation_alarm_(now_, task_runner) {
  OSP_DCHECK(receiver_);
  OSP_DCHECK(renderer_);

  // Render (and Present) the "waiting to start" blue screen.
  Render();

  // Begin immediately.
  receiver_->SetConsumer(this);
  decoder_.set_client(this);
  ResumeDecodingIfNothingToPlay();
}

SDLVideoPlayer::~SDLVideoPlayer() {
  decoder_.set_client(nullptr);
  receiver_->SetConsumer(nullptr);
}

void SDLVideoPlayer::OnFrameComplete(FrameId frame_id) {
  TryDecodingAnotherFrame();
}

void SDLVideoPlayer::ResumeDecodingIfNothingToPlay() {
  if (frames_to_render_.empty()) {
    decode_alarm_.Schedule([this] { TryDecodingAnotherFrame(); }, {});
  }
}

void SDLVideoPlayer::ResumeRendering() {
  render_alarm_.Schedule([this] { Render(); }, {});
}

void SDLVideoPlayer::TryDecodingAnotherFrame() {
  // Do not consume anything if too many frames are already queued for
  // rendering. It is more resource-efficient for frames to remain in the
  // Receiver's queue until this player is ready to consume them.
  constexpr size_t kMaxFramesInRenderPipeline = 8;
  if (frames_to_render_.size() > kMaxFramesInRenderPipeline) {
    return;
  }

  // See if a frame is ready to be consumed. If not, reschedule a later check.
  // The extra "poll" is necessary because it's possible the Receiver will
  // decide to skip frames at a later time, to unblock things.
  const int buffer_size = receiver_->AdvanceToNextFrame();
  if (buffer_size == Receiver::kNoFramesReady) {
    // TODO: This interval shouldn't be a constant, but should be 1/max_fps.
    constexpr auto kConsumeKickstartInterval = milliseconds(10);
    decode_alarm_.Schedule([this] { TryDecodingAnotherFrame(); },
                           now_() + kConsumeKickstartInterval);
    return;
  }

  // Consume the next frame.
  const auto start_time = now_();
  buffer_.Resize(buffer_size);
  EncodedFrame frame;
  frame.data = buffer_.GetSpan();
  receiver_->ConsumeNextFrame(&frame);

  // Create the tracking state for the frame in the player pipeline.
  OSP_DCHECK_EQ(frames_to_render_.count(frame.frame_id), 0);
  PendingFrame& pending_frame = frames_to_render_[frame.frame_id];
  pending_frame.start_time = start_time;

  // Determine the presentation time of the frame. Ideally, this will occur
  // based on the time progression of the media, given by the RTP timestamps.
  // However, if this falls too far out-of-sync with the system reference clock,
  // re-syrchronize, possibly causing user-visible "jank."
  constexpr auto kMaxPlayoutDrift = milliseconds(100);
  const auto media_time_since_last_sync =
      (frame.rtp_timestamp - last_sync_rtp_timestamp_)
          .ToDuration<Clock::duration>(receiver_->rtp_timebase());
  pending_frame.presentation_time =
      last_sync_reference_time_ + media_time_since_last_sync;
  const auto drift = duration_cast<milliseconds>(
      frame.reference_time - pending_frame.presentation_time);
  if (drift > kMaxPlayoutDrift || drift < -kMaxPlayoutDrift) {
    // Only log if not the very first frame.
    OSP_LOG_IF(INFO, frame.frame_id != FrameId::first())
        << "Playout drift (" << drift.count() << " ms) exceeded threshold ("
        << kMaxPlayoutDrift.count() << " ms). Re-synchronizing...";
    // This is the "big-stick" way to re-synchronize. If the amount of drift
    // is small, a production-worthy player should "nudge" things gradually
    // back into sync over several frames.
    last_sync_rtp_timestamp_ = frame.rtp_timestamp;
    last_sync_reference_time_ = frame.reference_time;
    pending_frame.presentation_time = frame.reference_time;
  }

  // Start decoding the frame. This call may synchronously call back into the
  // AVCodecDecoder::Client methods in this class.
  decoder_.Decode(frame.frame_id, buffer_);

  ResumeDecodingIfNothingToPlay();
}

void SDLVideoPlayer::OnFrameDecoded(FrameId frame_id, const AVFrame& frame) {
  const auto it = frames_to_render_.find(frame_id);
  if (it == frames_to_render_.end()) {
    return;
  }
  OSP_DCHECK(!it->second.decoded_frame);
  // av_clone_frame() does a shallow copy here, incrementing a ref-count on the
  // memory backing the frame.
  it->second.decoded_frame = AVFrameUniquePtr(av_frame_clone(&frame));
  ResumeRendering();
}

void SDLVideoPlayer::OnDecodeError(FrameId frame_id, std::string message) {
  const auto it = frames_to_render_.find(frame_id);
  if (it != frames_to_render_.end()) {
    frames_to_render_.erase(it);
  }
  OSP_LOG_WARN << "Requesting key frame because of error decoding " << frame_id
               << ": " << message;
  receiver_->RequestKeyFrame();
  ResumeDecodingIfNothingToPlay();
}

void SDLVideoPlayer::OnFatalError(std::string message) {
  state_ = kError;
  error_status_ = Error(Error::Code::kUnknownError, std::move(message));

  // Halt decoding and clear the rendering queue.
  receiver_->SetConsumer(nullptr);
  decoder_.set_client(nullptr);
  decode_alarm_.Cancel();
  frames_to_render_.clear();

  // Resume rendering, to display the "red splash" screen.
  ResumeRendering();

  if (error_callback_) {
    const auto callback = std::move(error_callback_);
    callback();
  }
}

void SDLVideoPlayer::Render() {
  // If something has already been scheduled to present at an exact time point,
  // don't render anything new yet.
  if (state_ == kScheduledToPresent) {
    return;
  }

  // If no frames are available, just re-render the currently-presented frame.
  auto it =
      (state_ == kError) ? frames_to_render_.end() : frames_to_render_.begin();
  if (it == frames_to_render_.end() || !it->second.decoded_frame) {
    IdleRender();
    return;
  }

  // Skip late frames, to render the first not-late frame. If all decoded frames
  // are late, skip-forward to the least-late frame.
  const auto now = now_();
  while (it->second.presentation_time < now) {
    const auto next_it = std::next(it);
    if (next_it == frames_to_render_.end() || !next_it->second.decoded_frame) {
      break;
    }
    frames_to_render_.erase(it);  // Drop the late frame.
    it = next_it;
  }

  // Render the frame and, if successful, schedule its presentation.
  picture_ = std::move(it->second.decoded_frame);
  if (Draw()) {
    state_ = kScheduledToPresent;
    presentation_alarm_.Schedule([this] { Present(); },
                                 it->second.presentation_time);
  } else {
    state_ = kError;
    IdleRender();
  }

  // Compute how long it took to decode/render this frame, and notify the
  // Receiver of the recent-average per-frame processing time. This is used by
  // the Receiver to determine when to drop late frames.
  const auto end_time = now_();
  const auto measured_processing_time = end_time - it->second.start_time;
  constexpr int kCumulativeAveragePoints = 8;
  recent_processing_time_ =
      ((kCumulativeAveragePoints - 1) * recent_processing_time_ +
       1 * measured_processing_time) /
      kCumulativeAveragePoints;
  receiver_->SetPlayerProcessingTime(recent_processing_time_);

  // Remove the frame from the queue now that it has been rendered (and will be
  // presented), and start preparing another frame for rendering.
  frames_to_render_.erase(it);
  ResumeDecodingIfNothingToPlay();
}

void SDLVideoPlayer::IdleRender() {
  if (state_ == kPresented) {
    if (!Draw()) {
      state_ = kError;
    }
  }
  if (state_ == kError) {
    // Paint "red splash" to indicate an error state.
    SDL_SetRenderDrawColor(renderer_, 128, 0, 0, 255);
    SDL_RenderClear(renderer_);
  } else if (state_ == kWaitingForFirstFrame) {
    // Paint "blue splash" to indicate the "waiting for first frame" state.
    SDL_SetRenderDrawColor(renderer_, 0, 0, 128, 255);
    SDL_RenderClear(renderer_);
  }

  // Schedule presentation to happen after a rather lengthy interval, to
  // minimize redraw/etc. resource usage while doing "idle" rendering. The
  // interval here, is "lengthy" from the program's perspective, but reasonably
  // "snappy" from the user's perspective.
  constexpr auto kIdlePresentInterval = milliseconds(250);
  presentation_alarm_.Schedule([this] { Present(); },
                               now_() + kIdlePresentInterval);
}

bool SDLVideoPlayer::Draw() {
  OSP_DCHECK(picture_);

  // Punt if the |picture_| format is not compatible with those supported by
  // SDL.
  const uint32_t sdl_format = GetSDLPixelFormat(*picture_);
  if (sdl_format == SDL_PIXELFORMAT_UNKNOWN) {
    std::ostringstream error;
    error << "SDL does not support AVPixelFormat " << picture_->format;
    OnFatalError(error.str());
    return false;
  }

  // If there is already a SDL texture, check that its format and size matches
  // that of |picture_|. If not, release the existing texture.
  if (texture_) {
    uint32_t texture_format = SDL_PIXELFORMAT_UNKNOWN;
    int texture_width = -1;
    int texture_height = -1;
    SDL_QueryTexture(texture_.get(), &texture_format, nullptr, &texture_width,
                     &texture_height);
    if (texture_format != sdl_format || texture_width != picture_->width ||
        texture_height != picture_->height) {
      texture_.reset();
    }
  }

  // If necessary, recreate a SDL texture having the same format and size as
  // that of |picture_|.
  if (!texture_) {
    const auto EvalDescriptionString = [&] {
      std::ostringstream error;
      error << SDL_GetPixelFormatName(sdl_format) << " at " << picture_->width
            << "×" << picture_->height;
      return error.str();
    };
    OSP_LOG_INFO << "Creating SDL texture for " << EvalDescriptionString();
    texture_ =
        MakeUniqueSDLTexture(renderer_, sdl_format, SDL_TEXTUREACCESS_STREAMING,
                             picture_->width, picture_->height);
    if (!texture_) {
      std::ostringstream error;
      error << "Unable to (re)create SDL texture for format: "
            << EvalDescriptionString();
      OnFatalError(error.str());
      return false;
    }
  }

  // Upload the |picture_| to the SDL texture.
  void* pixels = nullptr;
  int stride = 0;
  SDL_LockTexture(texture_.get(), nullptr, &pixels, &stride);
  const auto picture_format = static_cast<AVPixelFormat>(picture_->format);
  const int pixels_size = av_image_get_buffer_size(
      picture_format, picture_->width, picture_->height, stride);
  constexpr int kSDLTextureRowAlignment = 1;  // SDL doesn't use word-alignment.
  av_image_copy_to_buffer(static_cast<uint8_t*>(pixels), pixels_size,
                          picture_->data, picture_->linesize, picture_format,
                          picture_->width, picture_->height,
                          kSDLTextureRowAlignment);
  SDL_UnlockTexture(texture_.get());

  // Render the SDL texture to the render target. Quality-related issues that a
  // production-worthy player should account for that are not being done here:
  //
  // 1. Need to account for AVPicture's sample_aspect_ratio property. Otherwise,
  //    content may appear "squashed" in one direction to the user.
  //
  // 2. SDL has no concept of color space, and so the color information provided
  //    with the AVPicture might not match the assumptions being made within
  //    SDL. Content may appear with washed-out colors, not-entirely-black
  //    blacks, striped gradients, etc.
  const SDL_Rect src_rect = {
      static_cast<int>(picture_->crop_left),
      static_cast<int>(picture_->crop_top),
      picture_->width -
          static_cast<int>(picture_->crop_left + picture_->crop_right),
      picture_->height -
          static_cast<int>(picture_->crop_top + picture_->crop_bottom)};
  SDL_Rect dst_rect = {0, 0, 0, 0};
  SDL_RenderGetLogicalSize(renderer_, &dst_rect.w, &dst_rect.h);
  if (src_rect.w != dst_rect.w || src_rect.h != dst_rect.h) {
    // Make the SDL rendering size the same as the frame's visible size. This
    // lets SDL automatically handle letterboxing and scaling details, so that
    // the video fits within the on-screen window.
    dst_rect.w = src_rect.w;
    dst_rect.h = src_rect.h;
    SDL_RenderSetLogicalSize(renderer_, dst_rect.w, dst_rect.h);
  }
  // Clear with black, for the "letterboxing" borders.
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderClear(renderer_);
  SDL_RenderCopy(renderer_, texture_.get(), &src_rect, &dst_rect);

  return true;
}

void SDLVideoPlayer::Present() {
  SDL_RenderPresent(renderer_);
  if (state_ == kScheduledToPresent) {
    state_ = kPresented;
  }
  ResumeRendering();
}

// static
uint32_t SDLVideoPlayer::GetSDLPixelFormat(const AVFrame& picture) {
  switch (picture.format) {
    case AV_PIX_FMT_NONE:
      break;
    case AV_PIX_FMT_YUV420P:
      return SDL_PIXELFORMAT_IYUV;
    case AV_PIX_FMT_YUYV422:
      return SDL_PIXELFORMAT_YUY2;
    case AV_PIX_FMT_UYVY422:
      return SDL_PIXELFORMAT_UYVY;
    case AV_PIX_FMT_YVYU422:
      return SDL_PIXELFORMAT_YVYU;
    case AV_PIX_FMT_NV12:
      return SDL_PIXELFORMAT_NV12;
    case AV_PIX_FMT_NV21:
      return SDL_PIXELFORMAT_NV21;
    case AV_PIX_FMT_RGB24:
      return SDL_PIXELFORMAT_RGB24;
    case AV_PIX_FMT_BGR24:
      return SDL_PIXELFORMAT_BGR24;
    case AV_PIX_FMT_ARGB:
      return SDL_PIXELFORMAT_ARGB32;
    case AV_PIX_FMT_RGBA:
      return SDL_PIXELFORMAT_RGBA32;
    case AV_PIX_FMT_ABGR:
      return SDL_PIXELFORMAT_ABGR32;
    case AV_PIX_FMT_BGRA:
      return SDL_PIXELFORMAT_BGRA32;
    default:
      break;
  }
  return SDL_PIXELFORMAT_UNKNOWN;
}

SDLVideoPlayer::PendingFrame::PendingFrame() = default;
SDLVideoPlayer::PendingFrame::~PendingFrame() = default;

}  // namespace cast_streaming
}  // namespace openscreen
