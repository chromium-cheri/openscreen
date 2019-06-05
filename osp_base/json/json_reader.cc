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

JsonReader::JsonReader() {
  Json::CharReaderBuilder builder;

  reader_ = std::unique_ptr<Json::CharReader>(builder.newCharReader());
}

ErrorOr<Json::Value> JsonReader::Read(absl::string_view document) {
  Json::Value root_node;
  std::string error_msg;

  if (document.empty()) {
    OSP_LOG_WARN << "JSON parse error: empty document";
    return Error::Code::kJsonParseError;
  }

  const bool succeeded =
      reader_->parse(document.begin(), document.end(), &root_node, &error_msg);

  if (!succeeded) {
    OSP_LOG_WARN << "JSON parse error: " << error_msg;
    return Error::Code::kJsonParseError;
  }

  return root_node;
}
}  // namespace openscreen
