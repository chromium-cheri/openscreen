// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_validation.h"

#include <numeric>
#include <string>

#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/stringprintf.h"
#include "valijson/adapters/jsoncpp_adapter.hpp"
#include "valijson/schema.hpp"
#include "valijson/schema_parser.hpp"
#include "valijson/utils/jsoncpp_utils.hpp"
#include "valijson/validator.hpp"

namespace openscreen {
namespace json {

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

}  // namespace json
}  // namespace openscreen
