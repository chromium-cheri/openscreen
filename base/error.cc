// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/error.h"

namespace openscreen {

std::string ErrorCodeToString(GlobalErrorCode code) {
  switch (code) {
    ERROR_STRING_CASE(GlobalErrorCode, None);
    ERROR_STRING_CASE(GlobalErrorCode, CborParsing);
    default:
      return "Unknown";
  }
}

}  // namespace openscreen
