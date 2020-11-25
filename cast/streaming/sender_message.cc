// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_message.h"

#include <utility>

#include "cast/streaming/message_fields.h"
#include "util/base64.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace {

SenderMessage::Type GetTypeFromString(absl::string_view s) {
  if (s == "OFFER")
    return SenderMessage::Type::kOffer;
  if (s == "GET_STATUS")
    return SenderMessage::Type::kGetStatus;
  if (s == "GET_CAPABILITIES")
    return SenderMessage::Type::kGetCapabilities;
  if (s == "RPC")
    return SenderMessage::Type::kRpc;
  return SenderMessage::Type::kUnknown;
}

std::string GetStringFromType(SenderMessage::Type type) {
  if (type == SenderMessage::Type::kOffer)
    return "OFFER";
  if (type == SenderMessage::Type::kGetStatus)
    return "GET_STATUS";
  if (type == SenderMessage::Type::kGetCapabilities)
    return "GET_CAPABILITIES";
  if (type == SenderMessage::Type::kRpc)
    return "RPC";
  return "UNKNOWN";
}

}  // namespace

// static
ErrorOr<SenderMessage> SenderMessage::Parse(const Json::Value& value) {
  SenderMessage message;
  std::string raw_message_type;
  if (!json::ParseAndValidateString(value[kMessageType], &raw_message_type)) {
    return Error(Error::Code::kParameterInvalid, "Missing message type field");
  }
  message.type = GetTypeFromString(raw_message_type);

  if (!json::ParseAndValidateInt(value[kSequenceNumber],
                                 &(message.sequence_number))) {
    message.sequence_number = -1;
  }

  if (message.type == SenderMessage::Type::kOffer) {
    ErrorOr<Offer> offer = Offer::Parse(value[kOfferMessageBody]);
    if (offer.is_value()) {
      message.body = std::move(offer.value());
      message.valid = true;
    }
  } else if (message.type == SenderMessage::Type::kRpc) {
    std::string rpc_body;
    if (json::ParseAndValidateString(value[kRpcMessageBody], &rpc_body) &&
        base64::Decode(rpc_body, &rpc_body)) {
      message.body = rpc_body;
      message.valid = true;
    }
  } else if (message.type == SenderMessage::Type::kGetStatus ||
             message.type == SenderMessage::Type::kGetCapabilities) {
    // These types of messages just don't have a body.
    message.valid = true;
  } else if (message.type == SenderMessage::Type::kUnknown) {
    // We can't do anything meaningful with this broken message, because
    // we don't have any type information about it.
    return Error(Error::Code::kJsonParseError, "Unknown sender message type");
  }

  return message;
}

ErrorOr<Json::Value> SenderMessage::ToJson() const {
  Json::Value root;
  root[kMessageType] = GetStringFromType(type);
  if (sequence_number >= 0) {
    root[kSequenceNumber] = sequence_number;
  }

  if (type == SenderMessage::Type::kOffer) {
    ErrorOr<Json::Value> offer_body = absl::get<Offer>(body).ToJson();
    if (offer_body.is_error()) {
      return offer_body.error();
    }
    root[kOfferMessageBody] = offer_body.value();
  } else if (type == SenderMessage::Type::kRpc) {
    root[kRpcMessageBody] = base64::Encode(absl::get<std::string>(body));
  } else if (type == SenderMessage::Type::kUnknown) {
    return Error(Error::Code::kParameterInvalid, "Unknown message type");
  }
  return root;
}

}  // namespace cast
}  // namespace openscreen
