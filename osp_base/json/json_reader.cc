// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/json/json_reader.h"

#include "osp_base/error.h"

#include "json/value.h"

namespace openscreen {

  JsonReader::JsonReader() = default;

  ErrorOr<Json::Value> JsonReader::Read(absl::string_view document) {
    return Error::None();
  }
}