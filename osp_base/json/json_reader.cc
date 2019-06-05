// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/json/json_reader.h"

#include <memory>
#include <string>

#include "json/value.h"
#include "osp_base/error.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace {
  // A reasonable maximum stack depth, may need to adjust as needs change.
  const int kMaxStackDepth = 64;
}

JsonReader::JsonReader() {
  builder_["stackLimit"] = kMaxStackDepth;
}

ErrorOr<Json::Value> JsonReader::Read(absl::string_view document) {
  if (document.empty()) {
    OSP_LOG_WARN << "JSON parse error: empty document";
    return Error::Code::kJsonParseError;
  }

  Json::Value root_node;
  std::string error_msg;
  std::unique_ptr<Json::CharReader> reader(builder_.newCharReader());
  const bool succeeded =
      reader->parse(document.begin(), document.end(), &root_node, &error_msg);
  if (!succeeded) {
    OSP_LOG_WARN << "JSON parse error: " << error_msg;
    return Error::Code::kJsonParseError;
  }

  return root_node;
}
}  // namespace openscreen
