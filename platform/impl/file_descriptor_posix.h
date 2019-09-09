// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_IMPL_FILE_DESCRIPTOR_POSIX_H_
#define PLATFORM_IMPL_FILE_DESCRIPTOR_POSIX_H_

#include "platform/api/file_descriptor.h"

namespace openscreen {
namespace platform {

struct FileDescriptor {
  explicit FileDescriptor(int descriptor);
  int fd;

  bool operator==(const FileDescriptor& other) const;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_FILE_DESCRIPTOR_POSIX_H_
