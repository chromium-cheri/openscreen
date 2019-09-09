// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/impl/file_descriptor_posix.h"

namespace openscreen {
namespace platform {

FileDescriptor::FileDescriptor(int descriptor) : fd(descriptor) {}

bool FileDescriptor::operator==(const FileDescriptor& other) const {
  return fd == other.fd;
}

}  // namespace platform
}  // namespace openscreen
