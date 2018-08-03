// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/screen_publisher.h"

namespace openscreen {

ScreenPublisherErrorInfo::ScreenPublisherErrorInfo() = default;
ScreenPublisherErrorInfo::ScreenPublisherErrorInfo(ScreenPublisherError error,
                                                   const std::string& message)
    : error(error), message(message) {}
ScreenPublisherErrorInfo::ScreenPublisherErrorInfo(
    const ScreenPublisherErrorInfo& other) = default;
ScreenPublisherErrorInfo::~ScreenPublisherErrorInfo() = default;

ScreenPublisherErrorInfo& ScreenPublisherErrorInfo::operator=(
    const ScreenPublisherErrorInfo& other) = default;

ScreenPublisherMetrics::ScreenPublisherMetrics() = default;
ScreenPublisherMetrics::~ScreenPublisherMetrics() = default;

ScreenPublisherCommonConfig::ScreenPublisherCommonConfig() = default;
ScreenPublisherCommonConfig::~ScreenPublisherCommonConfig() = default;

ScreenPublisher::ScreenPublisher(ScreenPublisherObserver* observer)
    : observer_(observer) {}
ScreenPublisher::~ScreenPublisher() = default;

}  // namespace openscreen
