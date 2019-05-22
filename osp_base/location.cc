// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/location.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "osp_base/macros.h"
#include "platform/api/logging.h"

namespace openscreen {

Location::Location() = default;
Location::Location(const Location& other) = default;

Location::Location(const char* file_name, const void* program_counter)
    : file_name_(file_name), program_counter_(program_counter) {}

Location::Location(const char* function_name,
                   const char* file_name,
                   int line_number,
                   const void* program_counter)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number),
      program_counter_(program_counter) {
  OSP_DCHECK(program_counter);
}

std::string Location::ToString() const {
  if (has_source_info()) {
    return absl::StrCat(function_name_, "@", file_name_, ":", line_number_);
  }

  return absl::Substitute("pc:$0", program_counter_);
}

#if defined(COMPILER_GCC)
#define RETURN_ADDRESS() \
  __builtin_extract_return_addr(__builtin_return_address(0))
#else
#define RETURN_ADDRESS() nullptr
#endif

// static
OSP_NOINLINE Location Location::CreateFromHere(const char* file_name) {
  return Location(file_name, RETURN_ADDRESS());
}

// static
OSP_NOINLINE Location Location::CreateFromHere(const char* function_name,
                                               const char* file_name,
                                               int line_number) {
  return Location(function_name, file_name, line_number, RETURN_ADDRESS());
}

OSP_NOINLINE const void* GetProgramCounter() {
  return RETURN_ADDRESS();
}

}  // namespace openscreen