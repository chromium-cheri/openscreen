// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/protocol/castv2/validation.h"

#include "cast/protocol/castv2/receiver_schema_data.h"
#include "cast/protocol/castv2/streaming_schema_data.h"
#include "util/json/json_serialization.h"
#include "util/json/json_validation.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

std::vector<Error> ValidateStreamingMessage(const Json::Value& message) {
  static ErrorOr<Json::Value> root = json::Parse(kStreamingSchema);
  // If the schema doesn't load it is a major developer error.
  OSP_CHECK(root.is_value());
  return json::Validate(message, root.value());
}

std::vector<Error> ValidateReceiverMessage(const Json::Value& message) {
  static ErrorOr<Json::Value> root = json::Parse(kReceiverSchema);
  // If the schema doesn't load it is a major developer error.
  OSP_CHECK(root.is_value());
  return json::Validate(message, root.value());
}

}  // namespace cast
}  // namespace openscreen
