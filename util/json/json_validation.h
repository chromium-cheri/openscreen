// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_JSON_JSON_VALIDATION_H_
#define UTIL_JSON_JSON_VALIDATION_H_

#include <vector>

#include "json/value.h"
#include "platform/base/error.h"

namespace openscreen {
namespace json {

// Returns a list of validation errors. An empty list indicates success.
std::vector<Error> Validate(const Json::Value& document,
                            const Json::Value& schema_root);

}  // namespace json
}  // namespace openscreen

#endif  // UTIL_JSON_JSON_VALIDATION_H_
