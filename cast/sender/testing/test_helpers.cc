// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/testing/test_helpers.h"

#include "cast/common/channel/message_util.h"
#include "cast/sender/channel/message_util.h"
#include "gtest/gtest.h"
#include "util/json/json_serialization.h"
#include "util/json/json_value.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

using ::cast::channel::CastMessage;

void VerifyAppAvailabilityRequest(const CastMessage& message,
                                  const std::string& app_id,
                                  int32_t* request_id,
                                  std::string* sender_id) {
  std::string app_id_out;
  VerifyAppAvailabilityRequest(message, &app_id_out, request_id, sender_id);
  EXPECT_EQ(app_id_out, app_id);
}

void VerifyAppAvailabilityRequest(const CastMessage& message,
                                  std::string* app_id,
                                  int32_t* request_id,
                                  std::string* sender_id) {
  EXPECT_EQ(message.namespace_(), kReceiverNamespace);
  EXPECT_EQ(message.destination_id(), kPlatformReceiverId);
  EXPECT_EQ(message.payload_type(),
            ::cast::channel::CastMessage_PayloadType_STRING);
  EXPECT_NE(message.source_id(), kPlatformSenderId);
  *sender_id = message.source_id();

  ErrorOr<Json::Value> maybe_value = json::Parse(message.payload_utf8());
  ASSERT_TRUE(maybe_value);
  Json::Value& value = maybe_value.value();

  absl::optional<absl::string_view> maybe_type =
      MaybeGetString(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyType));
  ASSERT_TRUE(maybe_type);
  EXPECT_EQ(maybe_type.value(),
            CastMessageTypeToString(CastMessageType::kGetAppAvailability));

  absl::optional<int> maybe_id =
      MaybeGetInt(value, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyRequestId));
  ASSERT_TRUE(maybe_id);
  *request_id = maybe_id.value();

  const Json::Value* maybe_app_ids =
      value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyAppId));
  ASSERT_TRUE(maybe_app_ids);
  ASSERT_TRUE(maybe_app_ids->isArray());
  ASSERT_EQ(maybe_app_ids->size(), 1u);
  Json::Value app_id_value = maybe_app_ids->get(0u, Json::Value(""));
  const char* begin = nullptr;
  const char* end = nullptr;
  app_id_value.getString(&begin, &end);
  ASSERT_GT(end, begin);
  *app_id = std::string(begin, end);
}

CastMessage CreateAppAvailabilityResponse(
    int32_t request_id,
    const std::string& sender_id,
    const std::string& app_id,
    AppAvailabilityResult availability_result) {
  CastMessage availability_response;
  Json::Value dict(Json::ValueType::objectValue);
  dict[kMessageKeyRequestId] = request_id;
  Json::Value availability(Json::ValueType::objectValue);
  OSP_CHECK(availability_result == AppAvailabilityResult::kAvailable ||
            availability_result == AppAvailabilityResult::kUnavailable);
  availability[app_id.c_str()] =
      availability_result == AppAvailabilityResult::kAvailable
          ? kMessageValueAppAvailable
          : kMessageValueAppUnavailable;
  dict[kMessageKeyAvailability] = std::move(availability);
  ErrorOr<std::string> serialized = json::Stringify(dict);
  OSP_CHECK(serialized);
  availability_response.set_source_id(kPlatformReceiverId);
  availability_response.set_destination_id(sender_id);
  availability_response.set_namespace_(kReceiverNamespace);
  availability_response.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  availability_response.set_payload_utf8(std::move(serialized.value()));
  availability_response.set_payload_type(
      ::cast::channel::CastMessage_PayloadType_STRING);

  return availability_response;
}

}  // namespace cast
}  // namespace openscreen
