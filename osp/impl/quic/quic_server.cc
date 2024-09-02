// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_server.h"

#include <functional>
#include <random>
#include <utility>

#include "quiche/quic/core/quic_utils.h"
#include "util/base64.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

QuicServer::QuicServer(
    const ServiceConfig& config,
    std::unique_ptr<QuicConnectionFactoryServer> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner,
    size_t buffer_limit)
    : QuicServiceBase(config,
                      std::move(connection_factory),
                      observer,
                      InstanceRequestIds::Role::kServer,
                      now_function,
                      task_runner,
                      buffer_limit),
      instance_name_(config.instance_name),
      password_(config.password) {
  auth_token_ = GenerateToken(16);
}

QuicServer::~QuicServer() = default;

bool QuicServer::Start() {
  bool result = StartImpl();
  if (result) {
    static_cast<QuicConnectionFactoryServer*>(connection_factory_.get())
        ->SetServerDelegate(this, connection_endpoints_);
  }
  return result;
}

bool QuicServer::Stop() {
  bool result = StopImpl();
  if (result) {
    static_cast<QuicConnectionFactoryServer*>(connection_factory_.get())
        ->SetServerDelegate(nullptr, {});
  }
  return result;
}

bool QuicServer::Suspend() {
  return SuspendImpl();
}

bool QuicServer::Resume() {
  return ResumeImpl();
}

ProtocolConnectionEndpoint::State QuicServer::GetState() {
  return state_;
}

MessageDemuxer& QuicServer::GetMessageDemuxer() {
  return demuxer_;
}

InstanceRequestIds& QuicServer::GetInstanceRequestIds() {
  return instance_request_ids_;
}

std::unique_ptr<ProtocolConnection> QuicServer::CreateProtocolConnection(
    uint64_t instance_id) {
  return CreateProtocolConnectionImpl(instance_id);
}

std::string QuicServer::GetAgentFingerprint() {
  return GetAgentCertificate().GetAgentFingerprint();
}

std::string QuicServer::GetAuthToken() {
  return auth_token_;
}

ErrorOr<size_t> QuicServer::OnStreamMessage(uint64_t instance_id,
                                            uint64_t connection_id,
                                            msgs::Type message_type,
                                            const uint8_t* buffer,
                                            size_t buffer_size,
                                            Clock::time_point now) {
  auto authentication_entry = pending_authentications_.find(instance_id);
  if (authentication_entry == pending_authentications_.end()) {
    return Error::Code::kNoActiveConnection;
  }

  switch (message_type) {
    case msgs::Type::kAuthSpake2Handshake: {
      msgs::AuthSpake2Handshake handshake;
      ssize_t result =
          msgs::DecodeAuthSpake2Handshake(buffer, buffer_size, handshake);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        OSP_LOG_WARN << "parse error: " << result;
        pending_authentications_.erase(authentication_entry);
        return Error::Code::kCborParsing;
      } else {
        OSP_CHECK(handshake.initiation_token.has_token);
        OSP_CHECK_EQ(handshake.psk_status,
                     msgs::AuthSpake2PskStatus::kPskInput);
        if (handshake.initiation_token.token == auth_token_) {
          std::string fingerprint = GetAgentFingerprint();
          std::vector<uint8_t> decoded_fingerprint;
          base64::Decode(fingerprint, &decoded_fingerprint);
          msgs::AuthSpake2Confirmation message = {
              .confirmation_value = ComputeSharedKey(
                  decoded_fingerprint, handshake.public_value, password_)};
          auto& sender = authentication_entry->second.data.sender;
          if (!sender) {
            sender = QuicProtocolConnection::FromExisting(
                *this, *authentication_entry->second.data.connection,
                *authentication_entry->second.data.stream_manager, instance_id);
          }
          sender->WriteMessage(message, &msgs::EncodeAuthSpake2Confirmation);
        } else {
          OSP_LOG_WARN << "Authentication failed: initiation token mismatch";
          pending_authentications_.erase(authentication_entry);
        }
        return result;
      }
    }
    case msgs::Type::kAuthStatus: {
      msgs::AuthStatus status;
      ssize_t result = msgs::DecodeAuthStatus(buffer, buffer_size, status);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        OSP_LOG_WARN << "parse error: " << result;
        pending_authentications_.erase(authentication_entry);
        return Error::Code::kCborParsing;
      } else {
        if (status.result == msgs::AuthStatusResult::kAuthenticated) {
          connections_.emplace(instance_id,
                               std::move(authentication_entry->second.data));
        } else {
          OSP_LOG_WARN << "Authentication failed: " << status.result;
        }
        pending_authentications_.erase(authentication_entry);
        return result;
      }
    }
    default: {
      OSP_LOG_WARN << "QuicServer receives message with unprocessable type.";
      pending_authentications_.erase(authentication_entry);
      break;
    }
  }
  return Error::Code::kCborParsing;
}

void QuicServer::OnClientCertificates(std::string_view instance_name,
                                      const std::vector<std::string>& certs) {
  fingerprint_map_.emplace(instance_name,
                           base64::Encode(quic::RawSha256(certs[0])));
}

void QuicServer::OnIncomingConnection(
    std::unique_ptr<QuicConnection> connection) {
  if (state_ != State::kRunning) {
    return;
  }

  const std::string& instance_name = connection->instance_name();
  pending_connections_.emplace(
      instance_name,
      PendingConnectionData(ServiceConnectionData(
          std::move(connection), std::make_unique<QuicStreamManager>(*this))));
}

void QuicServer::StartAuthentication(uint64_t instance_id) {
  auto authentication_entry = pending_authentications_.find(instance_id);
  if (authentication_entry == pending_authentications_.end()) {
    return;
  }

  std::string fingerprint = GetAgentFingerprint();
  std::vector<uint8_t> decoded_fingerprint;
  base64::Decode(fingerprint, &decoded_fingerprint);
  msgs::AuthSpake2Handshake message = {
      .initiation_token =
          msgs::AuthInitiationToken{
              .has_token = true,
              .token = auth_token_,
          },
      .psk_status = msgs::AuthSpake2PskStatus::kPskShown,
      .public_value = ComputePublicValue(decoded_fingerprint)};

  auto& sender = authentication_entry->second.data.sender;
  if (!sender) {
    sender = QuicProtocolConnection::FromExisting(
        *this, *authentication_entry->second.data.connection,
        *authentication_entry->second.data.stream_manager, instance_id);
  }

  sender->WriteMessage(message, &msgs::EncodeAuthSpake2Handshake);
}

std::string QuicServer::GenerateToken(int length) {
  constexpr char characters[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::random_device dev;
  std::mt19937 gen(dev());
  std::uniform_int_distribution<> dis(0, strlen(characters) - 1);
  std::string token;
  token.resize(length);
  for (int i = 0; i < length; i++) {
    token[i] = characters[dis(gen)];
  }

  return token;
}

}  // namespace openscreen::osp
