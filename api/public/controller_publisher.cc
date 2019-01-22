// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/controller_publisher.h"

namespace openscreen {

ControllerPublisherError::ControllerPublisherError() = default;
ControllerPublisherError::ControllerPublisherError(Code error,
                                                   const std::string& message)
    : error(error), message(message) {}
ControllerPublisherError::ControllerPublisherError(
    const ControllerPublisherError& other) = default;
ControllerPublisherError::~ControllerPublisherError() = default;

ControllerPublisherError& ControllerPublisherError::operator=(
    const ControllerPublisherError& other) = default;

ControllerPublisher::Metrics::Metrics() = default;
ControllerPublisher::Metrics::~Metrics() = default;

ControllerPublisher::Config::Config() = default;
ControllerPublisher::Config::~Config() = default;

ControllerPublisher::ControllerPublisher(Observer* observer)
    : observer_(observer) {}
ControllerPublisher::~ControllerPublisher() = default;

}  // namespace openscreen
