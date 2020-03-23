// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_CAST_AGENT_H_
#define CAST_STANDALONE_RECEIVER_CAST_AGENT_H_

#include <openssl/x509.h>

#include <memory>

#include "cast/common/channel/cast_socket.h"
#include "cast/receiver/channel/receiver_socket_factory.h"
#include "cast/standalone_receiver/streaming_playback_controller.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/receiver_session.h"
#include "platform/api/scoped_wake_lock.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "platform/impl/task_runner.h"
#include "util/serial_delete_ptr.h"

namespace openscreen {
namespace cast {

class CastAgent : public CastSocket::Client,
                  public ReceiverSocketFactory::Client,
                  public ReceiverSession::Client {
 public:
  CastAgent(TaskRunnerImpl* task_runner, InterfaceInfo interface);
  ~CastAgent();

  // Initialization occurs as part of construction, however to actually bind
  // for discovery and listening over TLS, the CastAgent must be started.
  Error Start();

  // Starts a dumb/legacy session without tls socketing, using a dummy offer
  // message.
  // TODO(jophba): remove once we have the new scenario completely working.
  Error StartLegacy();
  Error Stop();

  // CastSocket::Client overrides.
  void OnError(CastSocket* socket, Error error) override;
  void OnMessage(CastSocket* socket,
                 ::cast::channel::CastMessage message) override;

  // ReceiverSocketFactory::Client overrides.
  void OnConnected(ReceiverSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) override;
  void OnError(ReceiverSocketFactory* factory, Error error) override;

  // ReceiverSession::Client overrides.
  void OnNegotiated(const ReceiverSession* session,
                    ReceiverSession::ConfiguredReceivers receivers) override;
  void OnReceiversDestroyed(const ReceiverSession* session) override;
  void OnError(const ReceiverSession* session, Error error) override;

 private:
  // Member variables set as part of construction.
  std::unique_ptr<Environment> environment_;
  TaskRunnerImpl* const task_runner_;
  IPEndpoint receive_endpoint_;

  // Member variables set as part of starting up.
  std::unique_ptr<TlsConnectionFactory> connection_factory_;
  std::unique_ptr<ReceiverSocketFactory> socket_factory_;
  std::unique_ptr<ScopedWakeLock> wake_lock_;

  // Member variables set as part of a sender connection.
  // NOTE: currently we only support a single session.
  std::unique_ptr<ReceiverSession> current_session_;
  std::unique_ptr<StreamingPlaybackController> playback_controller_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_CAST_AGENT_H_
