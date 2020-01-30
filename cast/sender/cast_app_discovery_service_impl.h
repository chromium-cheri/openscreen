// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CAST_APP_DISCOVERY_SERVICE_IMPL_H_
#define CAST_SENDER_CAST_APP_DISCOVERY_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cast/common/discovery/service_info.h"
#include "cast/sender/cast_app_availability_tracker.h"
#include "cast/sender/cast_platform_handler.h"
#include "cast/sender/public/cast_app_discovery_service.h"
#include "platform/api/time.h"

namespace openscreen {
namespace cast {

// Keeps track of device queries, receives device updates, and issues app
// availability requests based on these signals.
class CastAppDiscoveryServiceImpl : public CastAppDiscoveryService {
 public:
  CastAppDiscoveryServiceImpl(CastPlatformHandler* platform_handler,
                              ClockNowFunctionPtr clock);
  ~CastAppDiscoveryServiceImpl() override;

  // CastAppDiscoveryService implementation.
  std::unique_ptr<Subscription> StartObservingDevices(
      const CastMediaSource& source,
      DeviceQueryCallback callback) override;

  // Reissues app availability requests for currently registered (device,
  // app_id) pairs whose status is kUnavailable or kUnknown.
  void Refresh() override;

  void OnDeviceAddedOrUpdated(const ServiceInfo& device);
  void OnDeviceRemoved(const ServiceInfo& device);

 private:
  struct DeviceQueryCallbackEntry {
    uint32_t id;
    DeviceQueryCallback callback;
  };

  // Issues an app availability request for |app_id| to the device given by
  // |device_id| via |socket|.
  void RequestAppAvailability(const std::string& device_id,
                              const std::string& app_id);

  // Updates the availability result for |device_id| and |app_id| with |result|,
  // and notifies callbacks with updated device query results.
  void UpdateAppAvailability(const std::string& device_id,
                             const std::string& app_id,
                             AppAvailabilityResult result);

  // Updates the device query results for |sources|.
  void UpdateDeviceQueries(const std::vector<CastMediaSource>& sources);

  std::vector<ServiceInfo> GetDevicesByIds(
      const std::vector<std::string>& device_ids) const;

  // Returns true if an app availability request should be issued for
  // |device_id| and |app_id|. |now| is used for checking whether previously
  // cached results should be refreshed.
  bool ShouldRefreshAppAvailability(const std::string& device_id,
                                    const std::string& app_id,
                                    Clock::time_point now) const;

  uint32_t GetNextDeviceQueryId();

  void RemoveDeviceQueryCallback(uint32_t id) override;

  std::map<std::string, ServiceInfo> devices_by_id_;

  // Registered device queries and their associated callbacks.
  std::map<std::string, std::vector<DeviceQueryCallbackEntry>> device_queries_;

  // Callback ID tracking.
  uint32_t next_device_query_id_;
  std::vector<uint32_t> free_query_ids_;

  CastPlatformHandler* const platform_handler_;

  CastAppAvailabilityTracker availability_tracker_;

  const ClockNowFunctionPtr clock_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_SENDER_CAST_APP_DISCOVERY_SERVICE_IMPL_H_
