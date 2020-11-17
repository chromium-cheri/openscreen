// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_ENUM_NAME_TABLE_H_
#define UTIL_ENUM_NAME_TABLE_H_

#include <array>
#include <utility>

#include "absl/strings/string_view.h"
#include "platform/base/error.h"
#include "util/osp_logging.h"

namespace openscreen {

constexpr char kUnknownEnumError[] = "Enum value not in array";

template <typename Enum, size_t Size>
using EnumNameTable = std::array<std::pair<const char*, Enum>, Size>;

template <typename Enum, size_t Size>
ErrorOr<const char*> GetEnumName(const EnumNameTable<Enum, Size>& map,
                                 Enum enum_) {
  for (auto pair : map) {
    if (pair.second == enum_) {
      return pair.first;
    }
  }
  return Error(Error::Code::kParameterInvalid, kUnknownEnumError);
}

template <typename Enum, size_t Size>
ErrorOr<Enum> GetEnum(const EnumNameTable<Enum, Size>& map,
                      absl::string_view name) {
  for (auto pair : map) {
    if (pair.first == name) {
      return pair.second;
    }
  }
  return Error(Error::Code::kParameterInvalid, kUnknownEnumError);
}

}  // namespace openscreen
#endif  // UTIL_ENUM_NAME_TABLE_H_
