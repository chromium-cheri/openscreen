// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "platform/test/paths.h"
#include "util/logging.h"
#include "util/std_util.h"

namespace openscreen {
namespace {

std::string ReadTestDataPath() {
  std::string path(1000, 0);
  constexpr int kMaxAttempts = 3;
  bool success = false;
  for (int attempts = 0; attempts < kMaxAttempts; ++attempts) {
    int ret = readlink("/proc/self/exe", data(path), path.size());
    if (ret == static_cast<int>(path.size())) {
      path.resize(path.size() * 2);
    } else if (ret > 0) {
      path.resize(ret);
      success = true;
      break;
    }
  }
  OSP_DCHECK(success);

  // NOTE: This assumes that the executable is two directories above the source
  // root (e.g. out/Debug/unittests).  This is the standard layout GN expects
  // but is also assumed by Chromium infra.
  int slashes_found = 0;
  int i = path.size() - 1;
  for (; i > 0; --i) {
    slashes_found += path[i] == '/';
    if (slashes_found == 3) {
      break;
    }
  }
  OSP_DCHECK_EQ(slashes_found, 3);

  return path.substr(0, i + 1) + OPENSCREEN_TEST_DATA_DIR;
}

}  // namespace

const std::string& GetTestDataPath() {
  static std::string data_path = ReadTestDataPath();
  return data_path;
}

}  // namespace openscreen
