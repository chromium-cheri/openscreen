// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/demo_client.h"

#if defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)
#include "cast/standalone_receiver/sdl_audio_player.h"
#include "cast/standalone_receiver/sdl_glue.h"
#include "cast/standalone_receiver/sdl_video_player.h"
#else
#include "cast/standalone_receiver/dummy_player.h"
#endif  // defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)

namespace openscreen {
namespace cast {

void DemoClient::OnNegotiated(ReceiverSession* session,
                              ReceiverSession::ConfiguredReceivers receivers) {
#if defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)

  // Start the SDL event loop, using the task runner to poll/process events.
  const ScopedSDLSubSystem<SDL_INIT_AUDIO> sdl_audio_sub_system;
  const ScopedSDLSubSystem<SDL_INIT_VIDEO> sdl_video_sub_system;
  const SDLEventLoopProcessor sdl_event_loop(
      task_runner, [&] { task_runner->RequestStopSoon(); });

  // Create/Initialize the Audio Player and Video Player, which are responsible
  // for decoding and playing out the received media.
  constexpr int kDefaultWindowWidth = 1280;
  constexpr int kDefaultWindowHeight = 720;
  const SDLWindowUniquePtr window = MakeUniqueSDLWindow(
      "Cast Streaming Receiver Demo",
      SDL_WINDOWPOS_UNDEFINED /* initial X position */,
      SDL_WINDOWPOS_UNDEFINED /* initial Y position */, kDefaultWindowWidth,
      kDefaultWindowHeight, SDL_WINDOW_RESIZABLE);
  OSP_CHECK(window) << "Failed to create SDL window: " << SDL_GetError();
  const SDLRendererUniquePtr renderer =
      MakeUniqueSDLRenderer(window.get(), -1, 0);
  OSP_CHECK(renderer) << "Failed to create SDL renderer: " << SDL_GetError();

  audio_player_ = std::make_unique<SDLAudioPlayer>(
      &Clock::now, task_runner, &audio_receiver, [&] {
        OSP_LOG_ERROR << audio_player.error_status().message();
        task_runner->RequestStopSoon();
      });
  video_player_ = std::make_unique<SDLVideoPlayer>(
      &Clock::now, task_runner, &video_receiver, renderer.get(), [&] {
        OSP_LOG_ERROR << video_player.error_status().message();
        task_runner->RequestStopSoon();
      });
#else
  audio_player_ = std::make_unique<DummyPlayer>(receivers.audio_receiver());
  video_player_ = std::make_unique<DummyPlayer>(receivers.video_receiver());
#endif  // defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)
}

void DemoClient::OnReceiversDestroyed(ReceiverSession* session) {
  audio_player_.reset();
  video_player_.reset();
}

void DemoClient::OnError(ReceiverSession* session, Error error) {
  OSP_LOG_FATAL << "Failure reported to demo client for session " << session
                << ": " << error;
}

}  // namespace cast
}  // namespace openscreen
