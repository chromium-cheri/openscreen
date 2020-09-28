// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_APPLICATION_AGENT_H_
#define CAST_RECEIVER_APPLICATION_AGENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cast/common/channel/connection_namespace_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/public/message_port.h"
#include "cast/receiver/channel/device_auth_namespace_handler.h"
#include "cast/receiver/public/receiver_socket_factory.h"
#include "platform/api/scoped_wake_lock.h"
#include "platform/api/serial_delete_ptr.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "platform/base/tls_credentials.h"
#include "util/json/json_value.h"

namespace openscreen {
namespace cast {

class CastSocket;

// A service listening for TLS connection attempts, establishing them, and
// providing a minimal implementation of the CastV2 application control protocol
// to launch receiver applications and route messages to/from them.
//
// Workflow: Upon construction, a TCP socket is set up for listening/accepting
// TLS connection over which Cast Channel messages will be sent. Also, one or
// more Applications are registered under this ApplicationAgent (e.g., a
// "mirroring" app). Later, a remote device will connect to the socket, and
// device authentication will take place. Then, Cast V2 application messages
// asking about application availability are received and processed based on
// what Applications are registered. Finally, the remote may request the LAUNCH
// of an Application (and later a STOP).
//
// In the meantime, this ApplicationAgent provides global RECEIVER_STATUS about
// what application is running. In addition, it attempts to launch an "idle
// screen" Application whenever no other Application is running. The "idle
// screen" Application is usually some kind of screen saver or wallpaper/clock
// display. Registering the "idle screen" Application is optional, and if it's
// not registered, then nothing will be running during idle periods.
//
// Consumers of this class are expected to provide a single threaded task runner
// implementation, a network interface information struct that will be used for
// TLS listening, and a credentials provider for TLS encryption/auth.
class ApplicationAgent final : private ReceiverSocketFactory::Client,
                               private CastMessageHandler,
                               private ConnectionNamespaceHandler::VirtualConnectionPolicy,
                               private VirtualConnectionRouter::SocketErrorHandler,
                               private MessagePort {
 public:
  struct ApplicationSessionDetails {
    std::string transport_id;
    std::string session_id;
    std::string app_id;
    std::string display_name;
    std::string status_text;  // Optional. May be left empty.
    std::vector<std::string> namespaces;
  };

  class Application : public CastMessageHandler {
   public:
    // Returns the one or more application IDs that are supported.
    virtual std::vector<std::string> GetAppIds() = 0;

    // Launches the application and returns its session details if successful.
    // |app_id| is the specific ID that was used to launch the app.
    virtual absl::optional<ApplicationSessionDetails> Launch(const std::string& app_id, MessagePort* message_port) = 0;

    // Stops the application, if running.
    virtual void Stop() = 0;

   protected:
    virtual ~Application();
  };

  ApplicationAgent(
      TaskRunner* task_runner,
      const InterfaceInfo& interface,
      DeviceAuthNamespaceHandler::CredentialsProvider* credentials_provider,
      TlsCredentials tls_credentials);

  ~ApplicationAgent() final;

  // Some applications, such as Cast Streaming require knowing the local IP
  // address of a valid network interface for establishing their own network
  // communications.
  const IPAddress& local_address() const { return cast_messaging_endpoint_.address; }

  // Registers an Application for launching by this agent. |app| must outlive
  // this ApplicationAgent.
  void RegisterApplication(Application* app, bool auto_launch_for_idle_screen);

  // Stops the given |app| if it is the one currently running. This is used by
  // applications that encounter "exit" conditions where they need to STOP
  // (e.g., due to timeout of user activity, end of media playback, or fatal
  // errors).
  void StopApplicationIfRunning(Application* app);

 private:
  // ReceiverSocketFactory::Client overrides.
  void OnConnected(ReceiverSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) final;
  void OnError(ReceiverSocketFactory* factory, Error error) final;

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 ::cast::channel::CastMessage message) final;

  // ConnectionNamespaceHandler::VirtualConnectionPolicy overrides.
  bool IsConnectionAllowed(const VirtualConnection& virtual_conn) const final;

  // VirtualConnectionRouter::SocketErrorHandler overrides.
  void OnClose(CastSocket* cast_socket) final;
  void OnError(CastSocket* socket, Error error) final;

  // Stops the currently-running Application and attempts to launch the
  // Application referred to by |app_id|. If this fails, the "idle screen"
  // Application will be automatically launched as a failure fall-back. |socket|
  // is non-null only when the application switch was caused by a remote LAUNCH
  // request.
  Error SwitchToApplication(const std::string& app_id, CastSocket* socket);

  // Stops the currently-running Application and launches the "idle screen."
  void GoIdle();

  // Populates the given |message| object with the RECEIVER_STATUS fields,
  // reflecting the currently-launched app (if any), and a fake volume level
  // status.
  void PopulateReceiverStatus(Json::Value* message);

  // Broadcasts new RECEIVER_STATUS to all endpoints. This is called after an
  // Application LAUNCH or STOP.
  void BroadcastReceiverStatus();

  // MessagePort overrides.
  void SetClient(MessagePort::Client* client) final;
  void PostMessage(const std::string& destination_id,
                   const std::string& message_namespace,
                   const std::string& message) final;

  TaskRunner* const task_runner_;
  DeviceAuthNamespaceHandler::CredentialsProvider* credentials_provider_;
  TlsCredentials tls_credentials_;
  const IPEndpoint cast_messaging_endpoint_;
  const SerialDeletePtr<ScopedWakeLock> wake_lock_;
  DeviceAuthNamespaceHandler auth_handler_;
  ConnectionNamespaceHandler connection_handler_;
  VirtualConnectionManager connection_manager_;
  VirtualConnectionRouter router_;
  ReceiverSocketFactory socket_factory_;
  const std::unique_ptr<TlsConnectionFactory> connection_factory_;

  std::map<std::string, Application*> registered_applications_;
  Application* idle_screen_app_ = nullptr;

  CastSocket* message_port_socket_ = nullptr;
  Application* launched_app_ = nullptr;
  ApplicationSessionDetails launched_app_details_{};
  MessagePort::Client* message_port_client_ = nullptr;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_CAST_AGENT_H_
