// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/protocol/castv2/validation.h"

#include <numeric>
#include <string>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json/value.h"
#include "platform/base/error.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

namespace {

constexpr char kEmptyJson[] = "{}";

// Schema format string, that allows for specifying definitions,
// properties, and required fields.
constexpr char kSchemaFormat[] = R"({
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "https://something/app_schema.json",
  "definitions": {
    %s
  },
  "type": "object",
  "properties": {
    %s
  },
  "required": [%s]
})";

// Fields used for an appId containing schema
constexpr char kAppIdDefinition[] = R"("app_id": {
    "type": "string",
    "enum": ["0F5096E8", "85CDB22F"]
  })";
constexpr char kAppIdName[] = "\"appId\"";
constexpr char kAppIdProperty[] =
    R"(  "appId": {"$ref": "#/definitions/app_id"})";

// Teest documents containing an appId.
constexpr char kValidAppIdDocument[] = R"({ "appId": "0F5096E8" })";
constexpr char kInvalidAppIdDocument[] = R"({ "appId": "FooBar" })";

std::string BuildSchema(const char* definitions,
                        const char* properties,
                        const char* required) {
  return StringPrintf(kSchemaFormat, definitions, properties, required);
}

bool TestValidate(absl::string_view document, absl::string_view schema) {
  OSP_DVLOG << "Validating document: \"" << document << "\" against schema: \""
            << schema << "\"";
  ErrorOr<Json::Value> document_root = json::Parse(document);
  EXPECT_TRUE(document_root.is_value());
  ErrorOr<Json::Value> schema_root = json::Parse(schema);
  EXPECT_TRUE(schema_root.is_value());

  std::vector<Error> errors =
      Validate(document_root.value(), schema_root.value());
  return errors.empty();
}

const std::string& GetEmptySchema() {
  static const std::string kEmptySchema = BuildSchema("", "", "");
  return kEmptySchema;
}

const std::string& GetAppSchema() {
  static const std::string kAppIdSchema =
      BuildSchema(kAppIdDefinition, kAppIdProperty, kAppIdName);
  return kAppIdSchema;
}

}  // namespace

TEST(JsonValidationTest, EmptyPassesEmpty) {
  EXPECT_TRUE(TestValidate(kEmptyJson, kEmptyJson));
}

TEST(JsonValidationTest, EmptyPassesBasicSchema) {
  EXPECT_TRUE(TestValidate(kEmptyJson, GetEmptySchema()));
}

TEST(JsonValidationTest, EmptyFailsAppIdSchema) {
  EXPECT_FALSE(TestValidate(kEmptyJson, GetAppSchema()));
}

TEST(JsonValidationTest, InvalidAppIdFailsAppIdSchema) {
  EXPECT_FALSE(TestValidate(kInvalidAppIdDocument, GetAppSchema()));
}

TEST(JsonValidationTest, ValidAppIdPassesAppIdSchema) {
  EXPECT_TRUE(TestValidate(kValidAppIdDocument, GetAppSchema()));
}

TEST(JsonValidationTest, InvalidAppIdPassesEmptySchema) {
  EXPECT_TRUE(TestValidate(kInvalidAppIdDocument, GetEmptySchema()));
}

TEST(JsonValidationTest, ValidAppIdPassesEmptySchema) {
  EXPECT_TRUE(TestValidate(kValidAppIdDocument, GetEmptySchema()));
}

}  // namespace cast
}  // namespace openscreen
