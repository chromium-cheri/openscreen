// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/std_util.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "util/osp_logging.h"

namespace openscreen {

std::string RemoveWhitespace(const std::string& s) {
  std::string stripped = s;
  stripped.erase(std::remove_if(stripped.begin(), stripped.end(), ::isspace),
                 stripped.end());
  return stripped;
}

}  // namespace openscreen
