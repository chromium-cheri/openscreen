// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/protocol/castv2/validation.h"

#include <string>

#include "cast/protocol/castv2/receiver_schema_data.h"
#include "cast/protocol/castv2/streaming_schema_data.h"
#include "cast/third_party/valijson/src/include/valijson/adapters/jsoncpp_adapter.hpp"
#include "cast/third_party/valijson/src/include/valijson/schema.hpp"
#include "cast/third_party/valijson/src/include/valijson/schema_parser.hpp"
#include "cast/third_party/valijson/src/include/valijson/utils/jsoncpp_utils.hpp"
#include "cast/third_party/valijson/src/include/valijson/validator.hpp"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

namespace {

std::vector<Error> MapErrors(const valijson::ValidationResults& results) {
  std::vector<Error> errors;
  for (const auto& result : results) {
    const std::string context = Join(result.context, ", ");
    errors.emplace_back(
        Error{Error::Code::kJsonParseError,
              StringPrintf("Node: %s, Message: %s", context.c_str(),
                           result.description.c_str())});

    OSP_VLOG << "JsonCpp validation error: "
             << errors.at(errors.size() - 1).message();
  }
  return errors;
}

}  // anonymous namespace

std::vector<Error> Validate(const Json::Value& document,
                            const Json::Value& schema_root) {
  valijson::adapters::JsonCppAdapter adapter(schema_root);
  valijson::Schema schema;
  valijson::SchemaParser parser;
  parser.populateSchema(adapter, schema);

  valijson::Validator validator;
  valijson::adapters::JsonCppAdapter document_adapter(document);
  valijson::ValidationResults results;
  if (validator.validate(schema, document_adapter, &results)) {
    return {};
  }
  return MapErrors(results);
}

std::vector<Error> ValidateStreamingMessage(const Json::Value& message) {
  static ErrorOr<Json::Value> root = json::Parse(kStreamingSchema);
  // If the schema doesn't load it is a major developer error.
  OSP_CHECK(root.is_value());
  return Validate(message, root.value());
}

std::vector<Error> ValidateReceiverMessage(const Json::Value& message) {
  static ErrorOr<Json::Value> root = json::Parse(kReceiverSchema);
  // If the schema doesn't load it is a major developer error.
  OSP_CHECK(root.is_value());
  return Validate(message, root.value());
}

}  // namespace cast
}  // namespace openscreen
