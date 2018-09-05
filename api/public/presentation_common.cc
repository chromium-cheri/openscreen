// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation_common.h"

namespace openscreen {

PresentationError::PresentationError() = default;
PresentationError::PresentationError(PresentationErrorCode error,
                                     const std::string& message)
    : error(error), message(message) {}
PresentationError::PresentationError(const PresentationError& other) = default;
PresentationError::~PresentationError() = default;

PresentationError& PresentationError::operator=(
    const PresentationError& other) = default;

}  // namespace openscreen
