// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/authentication_alice.h"

#include <utility>

#include "util/base64.h"

namespace openscreen::osp {

AuthenticationAlice::AuthenticationAlice(MessageDemuxer& demuxer,
                                         Delegate& delegate,
                                         std::vector<uint8_t> fingerprint,
                                         std::string_view auth_token,
                                         std::string_view password)
    : AuthenticationBase(demuxer, delegate, std::move(fingerprint)),
      auth_token_(auth_token),
      password_(password) {}

AuthenticationAlice::~AuthenticationAlice() = default;

void AuthenticationAlice::StartAuthentication(uint64_t instance_id) {
  auto data_entry = auth_data_.find(instance_id);
  if (data_entry == auth_data_.end() || !data_entry->second.sender) {
    delegate_.OnAuthenticationFailed(instance_id);
    return;
  }

  msgs::AuthSpake2Handshake message = {
      .initiation_token =
          msgs::AuthInitiationToken{
              .has_token = true,
              .token = auth_token_,
          },
      .psk_status = msgs::AuthSpake2PskStatus::kPskShown,
      .public_value = ComputePublicValue(fingerprint_)};
  data_entry->second.sender->WriteMessage(message,
                                          &msgs::EncodeAuthSpake2Handshake);
}

ErrorOr<size_t> AuthenticationAlice::OnStreamMessage(uint64_t instance_id,
                                                     uint64_t connection_id,
                                                     msgs::Type message_type,
                                                     const uint8_t* buffer,
                                                     size_t buffer_size,
                                                     Clock::time_point now) {
  auto data_entry = auth_data_.find(instance_id);
  if (data_entry == auth_data_.end() || !data_entry->second.sender) {
    delegate_.OnAuthenticationFailed(instance_id);
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
        delegate_.OnAuthenticationFailed(instance_id);
        return Error::Code::kCborParsing;
      } else {
        auto& initiation_token = handshake.initiation_token;
        if (!initiation_token.has_token ||
            initiation_token.token != auth_token_) {
          OSP_LOG_WARN << "Authentication failed: initiation token mismatch.";
          delegate_.OnAuthenticationFailed(instance_id);
          return result;
        }

        if (handshake.psk_status ==
            msgs::AuthSpake2PskStatus::kPskNeedsPresentation) {
          // Compute and save the shared key for verification later.
          data_entry->second.shared_key =
              ComputeSharedKey(fingerprint_, handshake.public_value, password_);
          msgs::AuthSpake2Handshake message = {
              .initiation_token =
                  msgs::AuthInitiationToken{
                      .has_token = true,
                      .token = auth_token_,
                  },
              .psk_status = msgs::AuthSpake2PskStatus::kPskInput,
              .public_value = ComputePublicValue(fingerprint_)};
          data_entry->second.sender->WriteMessage(
              message, &msgs::EncodeAuthSpake2Handshake);
        } else if (handshake.psk_status ==
                   msgs::AuthSpake2PskStatus::kPskInput) {
          msgs::AuthSpake2Confirmation message = {
              .confirmation_value = ComputeSharedKey(
                  fingerprint_, handshake.public_value, password_)};
          data_entry->second.sender->WriteMessage(
              message, &msgs::EncodeAuthSpake2Confirmation);
        } else {
          OSP_LOG_WARN << "Authentication failed: receive wrong PSK status.";
          delegate_.OnAuthenticationFailed(instance_id);
        }
        return result;
      }
    }

    case msgs::Type::kAuthSpake2Confirmation: {
      msgs::AuthSpake2Confirmation confirmation;
      ssize_t result =
          msgs::DecodeAuthSpake2Confirmation(buffer, buffer_size, confirmation);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        OSP_LOG_WARN << "parse error: " << result;
        delegate_.OnAuthenticationFailed(instance_id);
        return Error::Code::kCborParsing;
      } else {
        if (std::equal(data_entry->second.shared_key.begin(),
                       data_entry->second.shared_key.end(),
                       confirmation.confirmation_value.begin())) {
          msgs::AuthStatus status = {
              .result = msgs::AuthStatusResult::kAuthenticated};
          data_entry->second.sender->WriteMessage(status,
                                                  &msgs::EncodeAuthStatus);
          delegate_.OnAuthenticationSucceed(instance_id);
        } else {
          OSP_LOG_WARN << "Authentication failed: shared key mismatch";
          msgs::AuthStatus status = {.result =
                                         msgs::AuthStatusResult::kProofInvalid};
          data_entry->second.sender->WriteMessage(status,
                                                  &msgs::EncodeAuthStatus);
          delegate_.OnAuthenticationFailed(instance_id);
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
        delegate_.OnAuthenticationFailed(instance_id);
        return Error::Code::kCborParsing;
      } else {
        if (status.result == msgs::AuthStatusResult::kAuthenticated) {
          delegate_.OnAuthenticationSucceed(instance_id);
        } else {
          OSP_LOG_WARN << "Authentication failed: " << status.result;
          delegate_.OnAuthenticationFailed(instance_id);
        }
        return result;
      }
    }

    default: {
      OSP_LOG_WARN
          << "Receives authentication message with unprocessable type.";
      delegate_.OnAuthenticationFailed(instance_id);
      return Error::Code::kCborParsing;
    }
  }
}

}  // namespace openscreen::osp
