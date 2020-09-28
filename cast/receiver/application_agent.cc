// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/receiver/application_agent.h"

#include <utility>

#include "cast/common/channel/message_util.h"
#include "cast/common/channel/virtual_connection.h"
#include "cast/common/public/cast_socket.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr int kDefaultMaxBacklogSize = 64;
const TlsListenOptions kDefaultListenOptions{kDefaultMaxBacklogSize};

IPAddress GetInterfaceAddress(const InterfaceInfo& interface) {
  const IPAddress address = interface.GetIpAddressV6()
                                ? interface.GetIpAddressV6()
                                : interface.GetIpAddressV4();
  OSP_CHECK(address);
  return address;
}

// Parses the given string as a JSON object. If the parse fails, an empty object
// is returned.
Json::Value ParseAsObject(absl::string_view value) {
  ErrorOr<Json::Value> parsed = json::Parse(message.payload_utf8());
  if (parsed.ok() && parsed.value().isObject()) {
    return std::move(parsed.value());
  }
  return {Json::objectValue};
}

// Returns true if the type field in |object| is set to the given |type|.
bool HasType(const Json::Value& object, CastMessageType type) {
  OSP_DCHECK(object.isObject());
  const Json::Value& value = object.get(kMessageKeyType, Json::Value::nullSingleton());
  return value.isString() && value.asString() == CastMessageTypeToString(type);
}

}  // namespace

ApplicationAgent::ApplicationAgent(
    TaskRunner* task_runner,
    const InterfaceInfo& interface,
    DeviceAuthNamespaceHandler::CredentialsProvider* credentials_provider,
    TlsCredentials tls_credentials)
    : task_runner_(task_runner),
      credentials_provider_(credentials_provider),
      tls_credentials_(std::move(tls_credentials)),
      cast_messaging_endpoint_(IPEndpoint{GetInterfaceAddress(interface), kDefaultCastPort}),
      wake_lock_(ScopedWakeLock::Create(task_runner_)),
      auth_handler_(credentials_provider_),
      connection_handler_(&connection_manager_, this),
      router_(&connection_manager_),
      socket_factory_(this, router_.get()),
      connection_factory_(TlsConnectionFactory::CreateFactory(socket_factory_.get(), task_runner_).release()) {
  router_->AddHandlerForLocalId(kPlatformReceiverId, this);
  connection_factory_->SetListenCredentials(tls_credentials_);
  connection_factory_->Listen(cast_messaging_endpoint_, kDefaultListenOptions);
}

ApplicationAgent::~ApplicationAgent() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  idle_screen_app_ = nullptr;  // Prevent re-launching the idle screen app.
  SwitchToApplication({}, nullptr);
}

void ApplicationAgent::RegisterApplication(Application* app, bool auto_launch_for_idle_screen) {
  OSP_DCHECK(app);
  for (const std::string& app_id : app->GetAppIds()) {
    const auto insert_result = registered_applications_.insert(std::make_pair{app_id, app});
    // The insert must not fail (prior entry for same key).
    OSP_DCHECK(insert_result.second);
  }
  if (auto_launch_for_idle_screen) {
    OSP_DCHECK(!idle_screen_app_);
    idle_screen_app_ = app;
    // Launch the idle screen app, if no app was running.
    if (!launched_app_) {
      GoIdle();
    }
  }
}

void ApplicationAgent::StopApplicationIfRunning(Application* app) {
  if (launched_app_ == app) {
    GoIdle();
  }
}

void ApplicationAgent::OnConnected(ReceiverSocketFactory* factory,
                            const IPEndpoint& endpoint,
                            std::unique_ptr<CastSocket> socket) {
  router_.TakeSocket(this, std::move(socket));
}

void ApplicationAgent::OnError(ReceiverSocketFactory* factory, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
}

void ApplicationAgent::OnMessage(VirtualConnectionRouter* router,
                                 CastSocket* socket,
                                 ::cast::channel::CastMessage message) {
  if (!message.has_source_id() || message.source_id().empty()) {
    return;
  }

  if (message_port_client_ && !launched_app_details_.transport_id.empty() && launched_app_details_.transport_id == message.destination_id()) {
    message_port_client_->OnMessage(message.source_id(), message.namespace_(), std::move(message));
    return;
  }

  OSP_DCHECK(message.destination_id() == kPlatformReceiverId ||
             message.destination_id() == kBroadcastId);

  // Delegate messages for certain namespaces to other CastMessageHandlers.
  const std::string& ns = message.namespace_();
  if (ns == kConnectionNamespace) {
    connection_handler_.OnMessage(router, socket, std::move(message));
    return;
  }
  if (ns == kAuthNamespace) {
    auth_handler_.OnMessage(router, socket, std::move(message));
    return;
  }

  const Json::Value parsed = ParseAsObject(message.payload_utf8());
  Json::Value response;
  if (ns == kHeartbeatNamespace) {
    if (HasType(parsed, CastMessageType::kPing)) {
      response[kMessageKeyType] = CastMessageTypeToString(CastMessageType::kPong);
    }
  } else if (ns == kReceiverNamespace) {
    if (HasType(parsed, CastMessageType::kGetAppAvailability)) {
      const Json::Value& app_ids = parsed[kMessageKeyAppId];
      if (app_ids.isArray()) {
        response[kMessageKeyRequestId] = parsed[kMessageKeyRequestId];
        response[kMessageKeyResponseType] = parsed[kMessageKeyType];
        Json::Value& availability = response["availability"];
        for (Json::ArrayIndex i = 0; app_ids.isValidIndex(i); ++i) {
          const Json::Value& app_id = app_ids[i];
          if (app_id.isString()) {
            const auto app_id_str = app_id.asString();
            availability[app_id_str] = registered_applications_.count(app_id_str) ? kMessageValueAppAvailable : kMessageValueAppUnavailable;
          }
        }
      }
    } else if (HasType(parsed, CastMessageType::kGetStatus)) {
      PopulateReceiverStatus(&response);
      response[kMessageKeyRequestId] = parsed[kMessageKeyRequestId];
    } else if (HasType(parsed, CastMessageType::kLaunch)) {
      const Json::Value& app_id = parsed[kMessageKeyAppId];
      Error error;
      if (app_id.IsString() && !app_id.asString().empty()) {
        error = SwitchToApplication(app_id.asString(), socket);
      } else {
        error = Error(kParameterInvalid, "BAD_PARAMETER");
      }
      if (!error.ok()) {
        response[kMessageKeyRequestId] = parsed[kMessageKeyRequestId];
        response[kMessageKeyType] = CastMessageTypeToString(CastMessageType::kLaunchError);
        response[reason] = error.message();
      }
    } else if (HasType(parsed, CastMessageType::kStop)) {
      const Json::Value& session_id = parsed[kMessageKeySessionId];
      if (session_id.isNull() ||
          (session_id.isString() && session_id.asString() == launched_app_details_.session_id)) {
        GoIdle();
      } else {
        response[kMessageKeyRequestId] = parsed[kMessageKeyRequestId];
        response[kMessageKeyType] = CastMessageTypeToString(CastMessageType::kInvalidRequest);
        response[reason] = "INVALID_SESSION_ID";
      }
    } else {
      response[kMessageKeyRequestId] = parsed[kMessageKeyRequestId];
      response[kMessageKeyType] = CastMessageTypeToString(CastMessageType::kInvalidRequest);
      response[reason] = "INVALID_COMMAND";
    }
  } else {
    // Ignore messages on all other namespaces.
  }

  if (!response.empty()) {
    router_.Send(VirtualConnection{message.destination_id(), message.source_id(), ToCastSocketId(socket)}, MakeSimpleUTF8Message(ns, json::Stringify(response).value()));
  }
}

bool ApplicationAgent::IsConnectionAllowed(const VirtualConnection& virtual_conn) const {
  return true;
}

void ApplicationAgent::OnClose(CastSocket* cast_socket) {
  if (cast_socket && message_port_socket_ == cast_socket) {
    OSP_VLOG << "Cast agent socket closed.";
    GoIdle();
  }
}

void ApplicationAgent::OnError(CastSocket* socket, Error error) {
  if (socket && message_port_socket_ == socket) {
    OSP_LOG_ERROR << "Cast agent received socket error: " << error;
    if (message_port_client_) {
      message_port_client_->OnError(std::move(error));
      SetClient(nullptr);
    }
    GoIdle();
  }
}

Error ApplicationAgent::SwitchToApplication(std::string app_id, CastSocket* socket) {
  Error error = Error::Code::kNone;
  Application* desired_app = nullptr;
  Application* fallback_app = nullptr;
  if (!app_id.empty()) {
    const auto it = registered_applications_.find(app_id);
    if (it != registered_applications_.end()) {
      desired_app = it->second;
      fallback_app = (desired_app != idle_screen_app_) ? idle_screen_app_ : nullptr;
    } else {
      error = Error(kItemNotFound, "NOT_FOUND");
      desired_app = nullptr;
      fallback_app = idle_screen_app_;
    }
  }

  if (launched_app_ == desired_app) {
    return error;
  }

  if (launched_app_) {
    launched_app_->Stop();
    SetClient(nullptr);
    message_port_socket_ = nullptr;
    launched_app_ = nullptr;
    launched_app_details_ = {};
  }

  if (desired_app) {
    message_port_socket_ = socket;
    auto details_opt = desired_app->Launch(app_id, this);
    if (details_opt) {
      launched_app_ = desired_app;
      launched_app_details_ = std::move(*details_opt);
    } else {
      error = Error(kUnknownError, "SYSTEM_ERROR");
      message_port_socket_ = nullptr;
    }
  }

  if (!launched_app && fallback_app) {
    auto details_opt = fallback_app->Launch({}, this);
    if (details_opt) {
      launched_app_ = fallback_app;
      launched_app_details_ = std::move(*details_opt);
    }
  }

  BroadcastReceiverStatus();

  return error;
}

void ApplicationAgent::GoIdle() {
  if (idle_screen_app_) {
    const auto app_ids = idle_screen_app_->GetAppIds();
    if (!app_ids.empty()) {
      SwitchToApplication(app_ids.front(), nullptr);
      return;
    }
  }
  SwitchToApplication({}, nullptr);
}

void ApplicationAgent::PopulateReceiverStatus(Json::Value* out) {
  Json::Value& message = *out;
  message[kMessageKeyType] = CastMessageTypeToString(CastMessageType::kReceiverStatus);
  Json::Value& status = message["status"];

  if (launched_app_) {
    Json::Value& details = status["applications"][0];
    details[kMessageKeyTransportId] = launched_app_details_.transport_id;
    details[kMessageKeySessionId] = launched_app_details_.session_id;
    details[kMessageKeyAppId] = launched_app_details_.app_id;
    details["universalAppId"] = launched_app_details_.app_id;
    details["displayName"] = launched_app_details_.display_name;
    if (!launched_app_details_.status_text.empty()) {
      details["statusText"] = launched_app_details_.status_text;
    }
    details["isIdleScreen"] = (launched_app_ == idle_screen_app_);
    details["launchedFromCloud"] = false;
    Json::Value& namespaces = (details["namespaces"] = Json::Value(Json::objectValue));
    for (int i = 0; i != launched_app_details_.namespaces.size(); ++i) {
      namespaces[i]["name"] = launched_app_details_.namespaces[i];
    }
  }

  status["userEq"] = Json::Value(Json::objectValue));

  // Indicate a fixed 100% volume level.
  Json::Value& volume = status["volume"];
  volume["controlType"] = "attenuation";
  volume["level"] = 1.0;
  volume["muted"] = false;
  volume["stepInterval"] = 0.05;
}

void ApplicationAgent::BroadcastReceiverStatus() {
  Json::Value message;
  PopulateReceiverStatus(&message);
  message[kMessageKeyRequestId] = Json::Value(0);
  router_.Send(VirtualConnection{kPlatformReceiverId, kBroadcastId, 0}, MakeSimpleUTF8Message(kReceiverNamespace, json::Stringify(message).value()));
}

void ApplicationAgent::SetClient(MessagePort::Client* client) final {
  if (client == message_port_client_) {
    return;
  }

  if (!launched_app_details_.transport_id.empty() && message_port_client_) {
    router_.RemoveHandlerForLocalId(launched_app_details_.transport_id, this);
  }

  message_port_client_ = client;

  if (!launched_app_details_.transport_id.empty() && message_port_client_) {
    router_.AddHandlerForLocalId(launched_app_details_.transport_id, this);
  }
}

void ApplicationAgent::PostMessage(const std::string& destination_id,
                                   const std::string& message_namespace,
                                   const std::string& message) final {
  OSP_DCHECK(!launched_app_details_.transport_id.empty());
  router_.Send(VirtualConnection{launched_app_details_.transport_id, sender_id, ToCastSocketId(message_port_socket_)}, MakeSimpleUTF8Message(message_namespace, json::Stringify(message).value()));
}

ApplicationAgent::Application::~Application() = default;

}  // namespace cast
}  // namespace openscreen
