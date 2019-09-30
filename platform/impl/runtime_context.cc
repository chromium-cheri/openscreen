// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/runtime_context.h"

#include <memory>

namespace openscreen {
namespace platform {

// static
std::unique_ptr<RuntimeContext> RuntimeContext::Create() {
  return std::unique_ptr<RuntimeContext>(nullptr);
}

}  // namespace platform
}  // namespace openscreen
