// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/mdns_service_publisher_factory.h"

#include "osp/impl/internal_services.h"
#include "platform/api/network_runner.h"

namespace openscreen {

// static
std::unique_ptr<ServicePublisher> MdnsServicePublisherFactory::Create(
    const ServicePublisher::Config& config,
    ServicePublisher::Observer* observer,
    platform::NetworkRunner* network_runner) {
  return InternalServices::CreatePublisher(config, observer, network_runner);
}

}  // namespace openscreen
