// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_common.h"

namespace openscreen {

StartError::StartError() = default;
StartError::StartError(ErrorCode error, const std::string& message)
    : error(error), message(message) {}
StartError::StartError(const StartError& other) = default;
StartError::~StartError() = default;

StartError& StartError::operator=(const StartError& other) = default;

}  // namespace openscreen
