// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SPAN_UTIL_H_
#define UTIL_SPAN_UTIL_H_

#include <string>

#include "platform/base/span.h"

namespace openscreen {

// Returns a ByteView over the data in `str`.
ByteView ByteViewFromString(const std::string& str);

// Returns a std::string copy of the data in `bytes`.
std::string ByteViewToString(const ByteView& bytes);

}  // namespace openscreen

#endif  // UTIL_SPAN_UTIL_H_
