// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/sysctl.h>

#include "platform/test/paths_internal.h"
#include "util/std_util.h"

namespace openscreen {

std::string GetExePath() {
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
  std::string path(_POSIX_PATH_MAX, 0);
  size_t cb = path.size();
  int ret = sysctl(mib, 4, data(path), &cb, NULL, 0);
  if (ret < 0) {
    path.resize(0);
  } else {
    path.resize(cb);
  }
  return path;
}

}  // namespace openscreen
