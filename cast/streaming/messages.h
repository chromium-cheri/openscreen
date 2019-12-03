// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGES_H_
#define CAST_STREAMING_MESSAGES_H_

#include <vector>

#include "platform/base/error.h"
#include "third_party/jsoncpp/src/include/json/value.h"

namespace cast {
namespace streaming {

template <typename T>
Json::Value PrimitiveVectorToJson(const std::vector<T>& vec) {
  Json::Value array(Json::ValueType::arrayValue);
  array.resize(vec.size());

  for (Json::Value::ArrayIndex i = 0; i < vec.size(); ++i) {
    array[i] = Json::Value(vec[i]);
  }

  return array;
}

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_MESSAGES_H_
