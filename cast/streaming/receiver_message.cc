// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_message.h"

#include <utility>

#include "absl/strings/ascii.h"
#include "json/reader.h"
#include "json/writer.h"
#include "platform/base/error.h"
#include "util/base64.h"
#include "util/json/json_helpers.h"

namespace openscreen {
namespace cast {

namespace {

// Get the response type from the type string value in the JSON message.
ReceiverMessage::Type MessageTypeFromString(const std::string& type) {
  if (type == "ANSWER")
    return ReceiverMessage::Type::kAnswer;
  if (type == "STATUS_RESPONSE")
    return ReceiverMessage::Type::kStatusResponse;
  if (type == "CAPABILITIES_RESPONSE")
    return ReceiverMessage::Type::kCapabilitiesResponse;
  if (type == "RPC")
    return ReceiverMessage::Type::kRpc;

  return ReceiverMessage::Type::kUnknown;
}

ReceiverMessage::Type GetMessageType(const Json::Value& root_node) {
  std::string type;
  if (!json::ParseAndValidateString(root_node["type"], &type)) {
    return ReceiverMessage::Type::kUnknown;
  }

  absl::AsciiStrToUpper(&type);
  return MessageTypeFromString(type);
}

std::string GetDetails(const Json::Value& value) {
  if (!value) {
    return {};
  }

  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, value);
}

ErrorOr<ReceiverError> ParseError(const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid, "Empty JSON in error parsing");
  }

  int code;
  std::string description;
  if (!json::ParseAndValidateInt(value["code"], &code) ||
      !json::ParseAndValidateString(value["description"], &description)) {
    return Error::Code::kJsonParseError;
  }

  // We are generally pretty forgiving about details: throwing an error
  // because the Receiver didn't properly fill out the detail of an error
  // message doesn't really make sense.
  return ReceiverError{code, description, GetDetails(value["details"])};
}

ErrorOr<ReceiverCapability> ParseCapability(const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid,
                 "Empty JSON in capabilities parsing");
  }

  int remoting_version;
  if (!json::ParseAndValidateInt(value["remoting"], &remoting_version)) {
    remoting_version = ReceiverCapability::kRemotingVersionUnknown;
  }

  std::vector<std::string> media_capabilities;
  if (!json::ParseAndValidateStringArray(value["mediaCaps"],
                                         &media_capabilities)) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse media capabilities");
  }

  return ReceiverCapability{remoting_version, std::move(media_capabilities)};
}

ErrorOr<ReceiverStatus> ParseStatus(const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid,
                 "Empty JSON in status parsing");
  }

  double wifi_snr;
  std::vector<int32_t> wifi_speed;
  if (!json::ParseAndValidateDouble(value["wifiSnr"], &wifi_snr) ||
      !json::ParseAndValidateIntArray(value["wifiSpeed"], &wifi_speed)) {
    return Error::Code::kJsonParseError;
  }
  return ReceiverStatus{wifi_snr, std::move(wifi_speed)};
}

}  // namespace

// static
ErrorOr<ReceiverMessage> ReceiverMessage::Parse(
    const std::string& message_data) {
  Json::CharReaderBuilder builder;
  Json::CharReaderBuilder::strictMode(&builder.settings_);
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

  Json::Value root_node;
  std::string error_msg;
  const bool succeeded = reader->parse(
      message_data.data(), message_data.data() + message_data.length(),
      &root_node, &error_msg);
  if (!succeeded) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse receiver message");
  }

  ReceiverMessage message;
  std::string result;
  if (!root_node ||
      !json::ParseAndValidateInt(root_node["seqNum"],
                                 &(message.sequence_number)) ||
      !json::ParseAndValidateString(root_node["result"], &result)) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse result or sequence number");
  }

  message.type = GetMessageType(root_node);

  // For backwards compatibility with <= M85, RPC responses lack a result field.
  message.valid =
      (result == "ok" || message.type == ReceiverMessage::Type::kRpc);
  if (!message.valid) {
    ErrorOr<ReceiverError> error = ParseError(root_node["error"]);
    if (error.is_value()) {
      message.body = std::move(error.value());
    }
    return message;
  }

  switch (message.type) {
    case Type::kAnswer: {
      Answer answer;
      if (openscreen::cast::Answer::ParseAndValidate(root_node["answer"],
                                                     &answer)) {
        message.body = std::move(answer);
        message.valid = true;
      }
    } break;

    case Type::kStatusResponse: {
      ErrorOr<ReceiverStatus> status = ParseStatus(root_node["status"]);
      if (status.is_value()) {
        message.body = std::move(status.value());
        message.valid = true;
      }
    } break;

    case Type::kCapabilitiesResponse: {
      ErrorOr<ReceiverCapability> capability =
          ParseCapability(root_node["capabilities"]);
      if (capability.is_value()) {
        message.body = std::move(capability.value());
        message.valid = true;
      }
    } break;

    case Type::kRpc: {
      std::string rpc;
      if (json::ParseAndValidateString(root_node["rpc"], &rpc) &&
          base64::Decode(rpc, &rpc)) {
        message.body = std::move(rpc);
        message.valid = true;
      }
    } break;

    case Type::kUnknown:
    default:
      break;
  }

  return message;
}

}  // namespace cast
}  // namespace openscreen
