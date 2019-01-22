// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_MDNS_CONTROLLER_PUBLISHER_FACTORY_H_
#define API_PUBLIC_MDNS_CONTROLLER_PUBLISHER_FACTORY_H_

#include <memory>

#include "api/public/controller_publisher.h"

namespace openscreen {

class MdnsControllerPublisherFactory {
 public:
  static std::unique_ptr<ControllerPublisher> Create(
      const ControllerPublisher::Config& config,
      ControllerPublisher::Observer* observer);
};

}  // namespace openscreen

#endif  // API_PUBLIC_MDNS_CONTROLLER_PUBLISHER_FACTORY_H_
