// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/message_util.h"

#include "cast/sender/channel/cast_auth_util.h"

namespace cast {
namespace channel {
namespace {

void FillCommonCastMessageFields(CastMessage* message,
                                 const std::string& source_id,
                                 const std::string& destination_id,
                                 const std::string& message_namespace) {
  message->set_protocol_version(CastMessage::CASTV2_1_0);
  message->set_source_id(source_id);
  message->set_destination_id(destination_id);
  message->set_namespace_(message_namespace);
}

}  // namespace

CastMessage CreateAuthChallengeMessage(const AuthContext& auth_context) {
  CastMessage message;
  DeviceAuthMessage auth_message;

  AuthChallenge* challenge = auth_message.mutable_challenge();
  challenge->set_sender_nonce(auth_context.nonce());
  challenge->set_hash_algorithm(SHA256);

  std::string auth_message_string;
  auth_message.SerializeToString(&auth_message_string);

  FillCommonCastMessageFields(&message, kPlatformSenderId, kPlatformReceiverId,
                              kAuthNamespace);
  message.set_payload_type(CastMessage_PayloadType_BINARY);
  message.set_payload_binary(auth_message_string);

  return message;
}

}  // namespace channel
}  // namespace cast
