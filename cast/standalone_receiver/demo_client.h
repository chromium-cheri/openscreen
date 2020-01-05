// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_DEMO_CLIENT_H_
#define CAST_STANDALONE_RECEIVER_DEMO_CLIENT_H_

#include <memory>

#include "cast/streaming/receiver_session.h"

#if defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)
#include "cast/standalone_receiver/sdl_audio_player.h"
#include "cast/standalone_receiver/sdl_glue.h"
#include "cast/standalone_receiver/sdl_video_player.h"
#else
#include "cast/standalone_receiver/dummy_player.h"
#endif  // defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)

namespace openscreen {
namespace cast {

class DemoClient : public ReceiverSession::Client {
 public:
  // ReceiverSession::Client overrides.
  void OnNegotiated(ReceiverSession* session,
                    ReceiverSession::ConfiguredReceivers receivers) override;

  void OnReceiversDestroyed(ReceiverSession* session) override;

  void OnError(ReceiverSession* session, Error error) override;

 private:
#if defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)
  std::unique_ptr<SDLAudioPlayer> audio_player_;
  std::unique_ptr<SDLVideoPlayer> video_player_;
#else
  std::unique_ptr<DummyPlayer> audio_player_;
  std::unique_ptr<DummyPlayer> video_player_;
#endif  // defined(CAST_STREAMING_HAVE_EXTERNAL_LIBS_FOR_DEMO_APPS)
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_DEMO_CLIENT_H_
