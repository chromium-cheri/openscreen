// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/remoting_initializer.h"

#include <utility>

#include "cast/streaming/message_fields.h"

namespace openscreen {
namespace cast {

namespace {

VideoDecoderConfig::Codec ToProtoCodec(VideoCodec value) {
  switch (value) {
    case VideoCodec::kHevc:
      return VideoDecoderConfig_Codec_kCodecHEVC;
    case VideoCodec::kH264:
      return VideoDecoderConfig_Codec_kCodecH264;
    case VideoCodec::kVp8:
      return VideoDecoderConfig_Codec_kCodecVP8;
    case VideoCodec::kVp9:
      return VideoDecoderConfig_Codec_kCodecVP9;
    default:
      return VideoDecoderConfig_Codec_kUnknownVideoCodec;
  }
}

AudioDecoderConfig::Codec ToProtoCodec(AudioCodec value) {
  switch (value) {
    case AudioCodec::kAac:
      return AudioDecoderConfig_Codec_kCodecAAC;
    case AudioCodec::kOpus:
      return AudioDecoderConfig_Codec_kCodecOpus;
    default:
      return AudioDecoderConfig_Codec_kUnknownAudioCodec;
  }
}

}  // namespace

RemotingInitializer::RemotingInitializer(RpcMessenger* messenger,
                                         AudioCodec audio_codec,
                                         VideoCodec video_codec)
    : messenger_(messenger),
      audio_codec_(audio_codec),
      video_codec_(video_codec) {
  messenger_->RegisterMessageReceiverCallback(
      RpcMessenger::kAcquireRendererHandle,
      [this](std::unique_ptr<RpcMessage> message) {
        this->OnInitializeMessage(std::move(message));
      });
}

void RemotingInitializer::SetReadyCallback(std::function<void()> ready_cb) {
  ready_cb_ = std::move(ready_cb);
}

void RemotingInitializer::OnInitializeMessage(
    std::unique_ptr<RpcMessage> message) {
  receiver_handle_ = message->integer_value();

  RpcMessage callback_message;
  callback_message.set_handle(receiver_handle_);
  callback_message.set_proc(RpcMessage::RPC_DS_INITIALIZE_CALLBACK);

  auto* callback_body =
      callback_message.mutable_demuxerstream_initializecb_rpc();

  // In Chrome, separate calls are used for the audio and video configs, but
  // for simplicity's sake we combine them here.
  callback_body->mutable_audio_decoder_config()->set_codec(
      ToProtoCodec(audio_codec_));
  callback_body->mutable_video_decoder_config()->set_codec(
      ToProtoCodec(video_codec_));

  OSP_DLOG_INFO << "Initializing receiver handle " << receiver_handle_
                << " with audio codec " << CodecToString(audio_codec_)
                << " and video codec " << CodecToString(video_codec_);
  messenger_->SendMessageToRemote(callback_message);

  if (ready_cb_) {
    ready_cb_();
  } else {
    OSP_DLOG_INFO << "Received a ready message, but no ready callback.";
  }
}

}  // namespace cast
}  // namespace openscreen
