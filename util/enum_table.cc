// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/enum_table.h"

#include <cstdlib>

namespace openscreen {

// static
const GenericEnumTableEntry* GenericEnumTableEntry::FindByString(
    absl::Span<const GenericEnumTableEntry> data,
    absl::string_view str) {
  for (const auto& datum : data) {
    if (datum.str_ == str) {
      return &datum;
    }
  }
  return nullptr;
}

// static
absl::optional<absl::string_view> GenericEnumTableEntry::FindByValue(
    absl::Span<const GenericEnumTableEntry> data,
    int value) {
  for (const auto& datum : data) {
    if (datum.value_ == value && datum.has_str())
      return datum.str();
  }
  return absl::nullopt;
}

}  // namespace openscreen
