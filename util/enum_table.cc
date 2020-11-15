// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/enum_table.h"

#include <cstdlib>

namespace openscreen {

#ifdef ARCH_CPU_64_BITS
// This assertion is pretty paranoid.  It will probably only ever be triggered
// if someone who doesn't understand how EnumTable works tries to add extra
// members to GenericEnumTableEntry.
static_assert(sizeof(GenericEnumTableEntry) == 16,
              "Instances of GenericEnumTableEntry are too big.");
#endif

// static
const GenericEnumTableEntry* GenericEnumTableEntry::FindByString(
    absl::Span<const GenericEnumTableEntry> data,
    absl::string_view str) {
  for (const auto& datum : data) {
    if (datum.length_ == str.length() &&
        std::memcmp(datum.chars_, str.data(), str.length()) == 0) {
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
