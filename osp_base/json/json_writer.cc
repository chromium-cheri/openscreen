// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/json/json_writer.h"

#include <memory>
#include <sstream>
#include <string>

#include "json/value.h"
#include "osp_base/error.h"
#include "platform/api/logging.h"

namespace openscreen {
JsonWriter::JsonWriter() = default;

ErrorOr<std::string> JsonWriter::Write(const Json::Value& value) {
  if (value.empty()) {
    return Error::Code::kJsonWriteError;
  }

  std::unique_ptr<Json::StreamWriter> const writer(factory_.newStreamWriter());
  std::stringstream stream;
  writer->write(value, &stream);
  stream << std::endl;

  if (!stream) {
    return Error::Code::kJsonWriteError;
  }

  return stream.str();
}
}  // namespace openscreen
