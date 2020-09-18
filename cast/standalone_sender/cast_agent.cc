// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/cast_agent.h"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "cast/common/channel/message_util.h"
#include "cast/standalone_sender/looping_file_sender.h"
#include "cast/standalone_sender/simulated_capturer.h"
#include "cast/standalone_sender/streaming_opus_encoder.h"
#include "cast/standalone_sender/streaming_vp8_encoder.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/cast_socket_message_port.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "platform/impl/platform_client_posix.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

using DeviceMediaPolicy = SenderSocketFactory::DeviceMediaPolicy;

}  // namespace

CastAgent::CastAgent(TaskRunner* task_runner) : task_runner_(task_runner) {
  environment_ =
      std::make_unique<Environment>(&Clock::now, task_runner_, IPEndpoint{});
  router_ = MakeSerialDelete<VirtualConnectionRouter>(task_runner_,
                                                      &connection_manager_);
  socket_factory_ =
      MakeSerialDelete<SenderSocketFactory>(task_runner_, this, task_runner_);
  connection_factory_ = SerialDeletePtr<TlsConnectionFactory>(
      task_runner_,
      TlsConnectionFactory::CreateFactory(socket_factory_.get(), task_runner_)
          .release());
  socket_factory_->set_factory(connection_factory_.get());
}

CastAgent::~CastAgent() = default;

void CastAgent::Connect(ConnectionSettings settings) {
  connection_settings_ = std::move(settings);
  const auto policy = connection_settings_->should_include_video
                          ? DeviceMediaPolicy::kIncludesVideo
                          : DeviceMediaPolicy::kAudioOnly;

  task_runner_->PostTask([this, policy] {
    socket_factory_->Connect(connection_settings_->receiver_endpoint, policy,
                             router_.get());
  });
}

void CastAgent::Stop() {
  task_runner_->PostTask([this] {
    StopCurrentSession();

    connection_factory_.reset();
    connection_settings_.reset();
    socket_factory_.reset();
    wake_lock_.reset();
  });
}

void CastAgent::OnConnected(SenderSocketFactory* factory,
                            const IPEndpoint& endpoint,
                            std::unique_ptr<CastSocket> socket) {
  if (current_session_) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }

  OSP_LOG_INFO << "Received connection from peer at: " << endpoint;
  message_port_.SetSocket(socket->GetWeakPtr());
  CreateAndStartSession();
}

void CastAgent::OnError(SenderSocketFactory* factory,
                        const IPEndpoint& endpoint,
                        Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
  StopCurrentSession();
}

void CastAgent::OnClose(CastSocket* cast_socket) {
  OSP_VLOG << "Cast agent socket closed.";
  StopCurrentSession();
}

void CastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
  StopCurrentSession();
}

void CastAgent::OnNegotiated(
    const SenderSession* session,
    SenderSession::ConfiguredSenders senders,
    capture_recommendations::Recommendations capture_recommendations) {
  if (senders.audio_sender == nullptr || senders.video_sender == nullptr) {
    OSP_LOG_ERROR << "Missing either audio or video, so exiting...";
    return;
  }

  OSP_VLOG << "Successfully negotiated with sender.";

  file_sender_ = std::make_unique<LoopingFileSender>(
      task_runner_, connection_settings_->path_to_file.c_str(),
      connection_settings_->receiver_endpoint, senders,
      connection_settings_->max_bitrate);
}

// Currently, we just kill the session if an error is encountered.
void CastAgent::OnError(const SenderSession* session, Error error) {
  OSP_LOG_ERROR << "Cast agent received sender session error: " << error;
  StopCurrentSession();
}

void CastAgent::CreateAndStartSession() {
  current_session_ = std::make_unique<SenderSession>(
      connection_settings_->receiver_endpoint.address, this, environment_.get(),
      &message_port_);

  AudioCaptureOption audio_option;
  VideoCaptureOption video_option;
  // Use default display resolution of 1080P.
  video_option.resolutions.emplace_back(DisplayResolution{});
  current_session_->Negotiate({audio_option}, {video_option});
}

void CastAgent::StopCurrentSession() {
  current_session_.reset();
  file_sender_.reset();
  message_port_.SetSocket(nullptr);
}

}  // namespace cast
}  // namespace openscreen
