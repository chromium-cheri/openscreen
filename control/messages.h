// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTROL_MESSAGES_H_
#define CONTROL_MESSAGES_H_

#include <cstdint>
#include <string>
#include <vector>

namespace openscreen {

enum class UrlAvailability {
  kCompatible,
  kNotCompatible,
  kNotValid,
  kErrorTimeout,
  kErrorTransient,
  kErrorPermanent,
  kErrorUnknown,
};

struct PresentationUrlAvailabilityRequest {
  uint64_t request_id;
  std::vector<std::string> urls;
};

ssize_t EncodePresentationUrlAvailabilityRequest(
    uint64_t request_id,
    const std::vector<std::string>& urls,
    uint8_t* buffer,
    ssize_t length);
ssize_t DecodePresentationUrlAvailabilityRequest(
    uint8_t* buffer,
    ssize_t length,
    PresentationUrlAvailabilityRequest* request);

}  // namespace openscreen

#endif  // CONTROL_MESSAGES_H_
