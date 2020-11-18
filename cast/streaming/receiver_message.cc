// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_message.h"

#include <utility>

#include "absl/strings/ascii.h"
#include "cast/streaming/message_fields.h"
#include "json/reader.h"
#include "json/writer.h"
#include "platform/base/error.h"
#include "util/base64.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace {

ReceiverMessage::Type GetTypeFromString(absl::string_view s) {
  if (s == kMessageTypeAnswer)
    return ReceiverMessage::Type::kAnswer;
  if (s == "STATUS_RESPONSE")
    return ReceiverMessage::Type::kStatusResponse;
  if (s == "CAPABILITIES_RESPONSE")
    return ReceiverMessage::Type::kCapabilitiesResponse;
  if (s == "RPC")
    return ReceiverMessage::Type::kRpc;
  OSP_NOTREACHED();
  return ReceiverMessage::Type::kRpc;
}

std::string GetStringFromType(ReceiverMessage::Type type) {
  if (type == ReceiverMessage::Type::kAnswer)
    return kMessageTypeAnswer;
  if (type == ReceiverMessage::Type::kStatusResponse)
    return "STATUS_RESPONSE";
  if (type == ReceiverMessage::Type::kCapabilitiesResponse)
    return "CAPABILITIES_RESPONSE";
  if (type == ReceiverMessage::Type::kRpc)
    return "RPC";
  OSP_NOTREACHED();
  return {};
}

ReceiverMessage::Type GetMessageType(const Json::Value& root) {
  std::string type;
  if (!json::ParseAndValidateString(root[kMessageType], &type)) {
    return ReceiverMessage::Type::kUnknown;
  }

  absl::AsciiStrToUpper(&type);
  return GetTypeFromString(type);
}

std::string GetDetails(const Json::Value& value) {
  if (!value) {
    return {};
  }

  Json::StreamWriterBuilder builder;
  return Json::writeString(builder, value);
}

}  // namespace

ErrorOr<ReceiverError> ReceiverError::Parse(const Json::Value& value) {
  if (!value) {
    return Error(Error::Code::kParameterInvalid, "Empty JSON in error parsing");
  }

  int code;
  std::string description;
  if (!json::ParseAndValidateInt(value[kErrorCode], &code) ||
      !json::ParseAndValidateString(value[kErrorDescription], &description)) {
    return Error::Code::kJsonParseError;
  }

  // We are generally pretty forgiving about details: throwing an error
  // because the Receiver didn't properly fill out the detail of an error
  // message doesn't really make sense.
  return ReceiverError{code, description, GetDetails(value[kErrorDetails])};
}

ErrorOr<ReceiverCapability> ReceiverCapability::Parse(
    const Json::Value& value) {
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

ErrorOr<ReceiverWifiStatus> ReceiverWifiStatus::Parse(
    const Json::Value& value) {
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
  return ReceiverWifiStatus{wifi_snr, std::move(wifi_speed)};
}

Json::Value ReceiverWifiStatus::ToJson() const {
  Json::Value root;
  root["wifiSnr"] = wifi_snr;
  Json::Value speeds(Json::ValueType::arrayValue);
  for (const auto& speed : wifi_speed) {
    speeds.append(speed);
  }
  root["wifiSpeed"] = std::move(speeds);
  return root;
}

Json::Value ReceiverCapability::ToJson() const {
  Json::Value root;
  root["remoting"] = remoting_version;
  Json::Value capabilities(Json::ValueType::arrayValue);
  for (const auto& capability : media_capabilities) {
    capabilities.append(capability);
  }
  root["mediaCaps"] = std::move(capabilities);
  return root;
}

Json::Value ReceiverError::ToJson() const {
  Json::Value root;
  root[kErrorCode] = code;
  root[kErrorDescription] = description;
  root[kErrorDetails] = details;
  return root;
}

// static
ErrorOr<ReceiverMessage> ReceiverMessage::Parse(
    const std::string& message_data) {
  ErrorOr<Json::Value> root = json::Parse(message_data);
  if (root.is_error()) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse receiver message");
  }

  ReceiverMessage message;
  std::string result;
  if (!root ||
      !json::ParseAndValidateInt(root[kSequenceNumber],
                                 &(message.sequence_number)) ||
      !json::ParseAndValidateString(root[kResult], &result)) {
    return Error(Error::Code::kJsonParseError,
                 "Failed to parse result or sequence number");
  }

  message.type = GetMessageType(root.value());
  // For backwards compatibility with <= M85, RPC responses lack a result field.
  message.valid =
      (result == kResultOk || message.type == ReceiverMessage::Type::kRpc);
  if (!message.valid) {
    ErrorOr<ReceiverError> error =
        ReceiverError::Parse(root.value()[kErrorMessageBody]);
    if (error.is_value()) {
      message.body = std::move(error.value());
    }
    return message;
  }

  switch (message.type) {
    case Type::kAnswer: {
      Answer answer;
      if (openscreen::cast::Answer::ParseAndValidate(
              root.value()[kAnswerMessageBody], &answer)) {
        message.body = std::move(answer);
        message.valid = true;
      }
    } break;

    case Type::kStatusResponse: {
      ErrorOr<ReceiverWifiStatus> status =
          ReceiverWifiStatus::Parse(root.value()[kStatusMessageBody]);
      if (status.is_value()) {
        message.body = std::move(status.value());
        message.valid = true;
      }
    } break;

    case Type::kCapabilitiesResponse: {
      ErrorOr<ReceiverCapability> capability =
          ReceiverCapability::Parse(root.value()[kCapabilitiesMessageBody]);
      if (capability.is_value()) {
        message.body = std::move(capability.value());
        message.valid = true;
      }
    } break;

    case Type::kRpc: {
      std::string rpc;
      if (json::ParseAndValidateString(root.value()[kRpcMessageBody], &rpc) &&
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

Json::Value ReceiverMessage::ToJson() const {
  Json::Value root;
  root[kMessageType] = GetStringFromType(type);
  if (sequence_number >= 0) {
    root[kSequenceNumber] = sequence_number;
  }

  if (type == ReceiverMessage::Type::kAnswer) {
    if (valid) {
      root[kResult] = kResultOk;
      root[kAnswerMessageBody] = absl::get<Answer>(body).ToJson();
    } else {
      root[kResult] = kResultError;
      root[kErrorMessageBody] = absl::get<ReceiverError>(body).ToJson();
    }
  } else if (type == ReceiverMessage::Type::kStatusResponse) {
    root[kStatusMessageBody] = absl::get<ReceiverWifiStatus>(body).ToJson();
  } else if (type == ReceiverMessage::Type::kCapabilitiesResponse) {
    root[kCapabilitiesMessageBody] =
        absl::get<ReceiverCapability>(body).ToJson();
  } else if (type == ReceiverMessage::Type::kRpc) {
    root[kRpcMessageBody] = base64::Encode(absl::get<std::string>(body));
  } else {
    OSP_NOTREACHED() << "Unknown message type";
  }
  return root;
}

}  // namespace cast
}  // namespace openscreen
