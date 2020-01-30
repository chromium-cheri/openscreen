// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_PUBLIC_CAST_APP_DISCOVERY_SERVICE_H_
#define CAST_SENDER_PUBLIC_CAST_APP_DISCOVERY_SERVICE_H_

#include <memory>
#include <vector>

#include "cast/common/discovery/service_info.h"

namespace openscreen {
namespace cast {

class CastMediaSource;

// Interface for app discovery for Cast devices.
class CastAppDiscoveryService {
 public:
  using DeviceQueryFunc = void(const CastMediaSource& source,
                               const std::vector<ServiceInfo>& devices);
  using DeviceQueryCallback = std::function<DeviceQueryFunc>;

  class Subscription {
   public:
    Subscription(CastAppDiscoveryService* discovery_service, uint32_t id);
    Subscription(const Subscription&) = delete;
    ~Subscription();
    Subscription& operator=(const Subscription&) = delete;

   private:
    CastAppDiscoveryService* const discovery_service_;
    uint32_t id_;
  };

  virtual ~CastAppDiscoveryService() = default;

  // Adds a device query for |source|. Results will be continuously returned via
  // |callback| until the returned Subscription is destroyed by the caller.
  // If there are cached results available, |callback| will be invoked before
  // this method returns.
  virtual std::unique_ptr<Subscription> StartObservingDevices(
      const CastMediaSource& source,
      DeviceQueryCallback callback) = 0;

  // Refreshes the state of app discovery in the service. It is suitable to call
  // this method when the user initiates a user gesture.
  virtual void Refresh() = 0;

 private:
  friend class Subscription;

  virtual void RemoveDeviceQueryCallback(uint32_t id) = 0;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_SENDER_PUBLIC_CAST_APP_DISCOVERY_SERVICE_H_
