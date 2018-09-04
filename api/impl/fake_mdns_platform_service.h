// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_FAKE_MDNS_PLATFORM_SERVICE_H_
#define API_IMPL_FAKE_MDNS_PLATFORM_SERVICE_H_

#include <vector>

#include "api/impl/mdns_platform_service.h"

namespace openscreen {

// TODO: move to test/?
class FakeMdnsPlatformService final : public MdnsPlatformService {
 public:
  FakeMdnsPlatformService();
  ~FakeMdnsPlatformService() override;

  void set_interfaces(const BoundInterfaces& interfaces) {
    interfaces_ = interfaces;
  }

  // PlatformService overrides.
  BoundInterfaces RegisterInterfaces(
      const std::vector<int32_t>& interface_index_whitelist) override;
  void DeregisterInterfaces(
      const BoundInterfaces& registered_interfaces) override;

 private:
  BoundInterfaces registered_interfaces_;
  BoundInterfaces interfaces_;
};

}  // namespace openscreen

#endif  // API_IMPL_FAKE_MDNS_PLATFORM_SERVICE_H_
