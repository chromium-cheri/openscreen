// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/connection_namespace_handler.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/virtual_connection.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "platform/api/logging.h"
#include "util/json/json_writer.h"

namespace cast {
namespace channel {
namespace {

bool IsValidProtocolVersion(int version) {
  return version >= CastMessage_ProtocolVersion_CASTV2_1_0 &&
         version <= CastMessage_ProtocolVersion_CASTV2_1_3;
}

bool FindMaxProtocolVersion(const Json::Value* version,
                            const Json::Value* version_list,
                            int* max_version) {
  using ArrayIndex = Json::Value::ArrayIndex;
  bool has_version = false;
  if (version_list && version_list->isArray()) {
    has_version = true;
    ArrayIndex size = version_list->size();
    for (ArrayIndex i = 0; i < size; ++i) {
      const Json::Value& it = (*version_list)[i];
      if (it.isInt()) {
        int version_int = it.asInt();
        if (IsValidProtocolVersion(version_int) && version_int > *max_version) {
          *max_version = version_int;
        }
      }
    }
  }
  if (version && version->isInt()) {
    int version_int = version->asInt();
    if (IsValidProtocolVersion(version_int)) {
      has_version = true;
      if (version_int > *max_version) {
        *max_version = version_int;
      }
    }
  }
  return has_version;
}

}  // namespace

using openscreen::ErrorOr;

ConnectionNamespaceHandler::ConnectionNamespaceHandler(
    VirtualConnectionManager* vc_manager,
    VirtualConnectionPolicy* vc_policy)
    : vc_manager_(vc_manager), vc_policy_(vc_policy) {
  OSP_DCHECK(vc_manager);
  OSP_DCHECK(vc_policy);
}

ConnectionNamespaceHandler::~ConnectionNamespaceHandler() = default;

void ConnectionNamespaceHandler::OnMessage(CastSocket* socket,
                                           CastMessage&& message) {
  if (message.payload_type() !=
      CastMessage_PayloadType::CastMessage_PayloadType_STRING) {
    return;
  }
  ErrorOr<Json::Value> result = json_reader_.Read(message.payload_utf8());
  if (result.is_error()) {
    return;
  }

  Json::Value& value = result.value();
  if (!value.isObject()) {
    return;
  }

  const Json::Value* type =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyType));
  if (!type || !type->isString()) {
    return;
  }

  std::string type_str = type->asString();
  if (type_str == kTypeConnect) {
    HandleConnect(socket, std::move(message), std::move(value));
  } else if (type_str == kTypeClose) {
    HandleClose(socket, std::move(message), std::move(value));
  } else {
  }
}

void ConnectionNamespaceHandler::HandleConnect(CastSocket* socket,
                                               CastMessage&& message,
                                               Json::Value&& value) {
  if (message.destination_id() == kBroadcastId ||
      message.source_id() == kBroadcastId) {
    return;
  }

  VirtualConnection vconn{message.destination_id(), message.source_id(),
                          socket->socket_id()};
  if (!vc_policy_->IsConnectionAllowed(vconn)) {
    SendClose(socket, std::move(vconn));
    return;
  }

  const Json::Value* conn_type_value =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyConnType));
  int conn_type = static_cast<int>(VirtualConnection::Type::kStrong);
  if (conn_type_value && conn_type_value->isInt()) {
    conn_type = conn_type_value->asInt();
    if (conn_type != static_cast<int>(VirtualConnection::Type::kWeak) &&
        conn_type != static_cast<int>(VirtualConnection::Type::kInvisible)) {
      conn_type = static_cast<int>(VirtualConnection::Type::kStrong);
    }
  }

  VirtualConnection::AssociatedData data;

  data.type = static_cast<VirtualConnection::Type>(conn_type);

  const Json::Value* user_agent_value =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyUserAgent));
  if (user_agent_value && user_agent_value->isString()) {
    data.user_agent = user_agent_value->asString();
  }

  const Json::Value* sender_info_value =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeySenderInfo));
  if (!sender_info_value || !sender_info_value->isObject()) {
    // TODO(btolsch): Should this be guessed from user agent?
    OSP_DVLOG << "No sender info from protocol.";
  }

  const Json::Value* version_value =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyProtocolVersion));
  const Json::Value* version_list_value =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyProtocolVersionList));
  int negotiated_version = CastMessage_ProtocolVersion_CASTV2_1_0;
  bool has_version = FindMaxProtocolVersion(version_value, version_list_value,
                                            &negotiated_version);
  data.max_protocol_version =
      static_cast<CastMessage_ProtocolVersion>(negotiated_version);

  // TODO(btolsch): Get sanitized ip address by socket_id? from CastSocket?
  data.ip_fragment = {};

  OSP_DVLOG << "Connection opened: " << vconn.local_id << ", " << vconn.peer_id
            << ", " << vconn.socket_id;

  if (has_version) {
    SendConnectedResponse(socket, vconn, negotiated_version);
  }

  vc_manager_->AddConnection(std::move(vconn), std::move(data));
}

void ConnectionNamespaceHandler::HandleClose(CastSocket* socket,
                                             CastMessage&& message,
                                             Json::Value&& value) {
  VirtualConnection vconn{message.destination_id(), message.source_id(),
                          socket->socket_id()};
  if (!vc_manager_->HasConnection(vconn)) {
    return;
  }

  VirtualConnection::CloseReason reason =
      VirtualConnection::CloseReason::kClosedByPeer;
  const Json::Value* reason_code_value =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyReasonCode));
  if (reason_code_value && reason_code_value->isInt()) {
    int reason_code = reason_code_value->asInt();
    if (reason_code >= VirtualConnection::CloseReason::kTransportClosed &&
        reason_code <= VirtualConnection::CloseReason::kUnknown) {
      reason = static_cast<VirtualConnection::CloseReason>(reason_code);
    }
  }

  OSP_DVLOG << "Connection closed (reason: " << reason
            << "): " << vconn.local_id << ", " << vconn.peer_id << ", "
            << vconn.socket_id;
  vc_manager_->RemoveConnection(vconn, reason);
}

void ConnectionNamespaceHandler::SendClose(CastSocket* socket,
                                           VirtualConnection&& vconn) {
  Json::Value close_message(Json::ValueType::objectValue);
  close_message[kKeyType] = kTypeClose;

  openscreen::JsonWriter json_writer;
  ErrorOr<std::string> result = json_writer.Write(close_message);
  if (result.is_error()) {
    return;
  }

  std::string& payload = result.value();
  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_source_id(std::move(vconn.local_id));
  message.set_destination_id(std::move(vconn.peer_id));
  message.set_namespace_(kConnectionNamespace);
  message.set_payload_type(CastMessage_PayloadType_STRING);
  message.set_payload_utf8(std::move(payload));
  socket->SendMessage(std::move(message));
}

void ConnectionNamespaceHandler::SendConnectedResponse(
    CastSocket* socket,
    const VirtualConnection& vconn,
    int max_protocol_version) {
  Json::Value connected_message(Json::ValueType::objectValue);
  connected_message[kKeyType] = kTypeConnected;
  connected_message[kKeyProtocolVersion] =
      static_cast<int>(max_protocol_version);

  openscreen::JsonWriter json_writer;
  ErrorOr<std::string> result = json_writer.Write(connected_message);
  if (result.is_error()) {
    return;
  }

  std::string& payload = result.value();
  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_source_id(vconn.local_id);
  message.set_destination_id(vconn.peer_id);
  message.set_namespace_(kConnectionNamespace);
  message.set_payload_type(CastMessage_PayloadType_STRING);
  message.set_payload_utf8(std::move(payload));
  socket->SendMessage(std::move(message));
}

}  // namespace channel
}  // namespace cast
