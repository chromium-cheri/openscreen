// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/public/cast_app_discovery_service.h"

namespace openscreen {
namespace cast {

CastAppDiscoveryService::Subscription::Subscription(
    CastAppDiscoveryService* discovery_service,
    uint32_t id)
    : discovery_service_(discovery_service), id_(id) {}

CastAppDiscoveryService::Subscription::~Subscription() {
  discovery_service_->RemoveDeviceQueryCallback(id_);
}

}  // namespace cast
}  // namespace openscreen
