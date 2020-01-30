// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/cast_app_discovery_service_impl.h"

#include <algorithm>
#include <chrono>

#include "cast/sender/public/cast_media_source.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

// The minimum time that must elapse before an app availability result can be
// force refreshed.
static constexpr std::chrono::minutes kRefreshThreshold =
    std::chrono::minutes(1);

}  // namespace

CastAppDiscoveryServiceImpl::CastAppDiscoveryServiceImpl(
    CastPlatformHandler* platform_handler,
    ClockNowFunctionPtr clock)
    : platform_handler_(platform_handler), clock_(clock) {
  OSP_DCHECK(platform_handler_);
  OSP_DCHECK(clock_);
}

CastAppDiscoveryServiceImpl::~CastAppDiscoveryServiceImpl() = default;

std::unique_ptr<CastAppDiscoveryService::Subscription>
CastAppDiscoveryServiceImpl::StartObservingDevices(
    const CastMediaSource& source,
    DeviceQueryCallback callback) {
  const std::string& source_id = source.source_id();

  // Return cached results immediately, if available.
  std::vector<std::string> cached_device_ids =
      availability_tracker_.GetAvailableDevices(source);
  if (!cached_device_ids.empty()) {
    callback(source, GetDevicesByIds(cached_device_ids));
  }

  auto& callbacks = device_queries_[source_id];
  if (callbacks.empty()) {
    // NOTE: Even though we retain availability results for an app unregistered
    // from the tracker, we will refresh the results when the app is
    // re-registered.
    std::vector<std::string> new_app_ids =
        availability_tracker_.RegisterSource(source);
    for (const auto& app_id : new_app_ids) {
      for (const auto& entry : devices_by_id_) {
        RequestAppAvailability(entry.first, app_id);
      }
    }
  }

  uint32_t query_id = GetNextDeviceQueryId();
  callbacks.push_back({query_id, std::move(callback)});
  return std::make_unique<Subscription>(this, query_id);
}

void CastAppDiscoveryServiceImpl::Refresh() {
  const auto app_ids = availability_tracker_.GetRegisteredApps();
  for (const auto& entry : devices_by_id_) {
    for (const auto& app_id : app_ids) {
      RequestAppAvailability(entry.first, app_id);
    }
  }
}

void CastAppDiscoveryServiceImpl::OnDeviceAddedOrUpdated(
    const ServiceInfo& device) {
  const std::string& device_id = device.unique_id;
  devices_by_id_[device_id] = device;

  // Any queries that currently contain this device should be updated.
  UpdateDeviceQueries(availability_tracker_.GetSupportedSources(device_id));

  for (const std::string& app_id : availability_tracker_.GetRegisteredApps()) {
    RequestAppAvailability(device_id, app_id);
  }
}

void CastAppDiscoveryServiceImpl::OnDeviceRemoved(const ServiceInfo& device) {
  const std::string& device_id = device.unique_id;
  devices_by_id_.erase(device_id);
  UpdateDeviceQueries(availability_tracker_.RemoveResultsForDevice(device_id));
}

void CastAppDiscoveryServiceImpl::RequestAppAvailability(
    const std::string& device_id,
    const std::string& app_id) {
  if (ShouldRefreshAppAvailability(device_id, app_id, clock_())) {
    platform_handler_->RequestAppAvailability(
        device_id, app_id,
        [this, device_id](const std::string& app_id,
                          AppAvailabilityResult availability) {
          UpdateAppAvailability(device_id, app_id, availability);
        });
  }
}

void CastAppDiscoveryServiceImpl::UpdateAppAvailability(
    const std::string& device_id,
    const std::string& app_id,
    AppAvailabilityResult availability) {
  if (devices_by_id_.find(device_id) == devices_by_id_.end()) {
    return;
  }

  OSP_DVLOG << "App " << app_id << " on device " << device_id << " is "
            << ToString(availability);

  UpdateDeviceQueries(availability_tracker_.UpdateAppAvailability(
      device_id, app_id, {availability, clock_()}));
}

void CastAppDiscoveryServiceImpl::UpdateDeviceQueries(
    const std::vector<CastMediaSource>& sources) {
  for (const auto& source : sources) {
    const std::string& source_id = source.source_id();
    auto it = device_queries_.find(source_id);
    if (it == device_queries_.end())
      continue;
    std::vector<std::string> device_ids =
        availability_tracker_.GetAvailableDevices(source);
    std::vector<ServiceInfo> devices = GetDevicesByIds(device_ids);
    for (const auto& callback : it->second) {
      callback.callback(source, devices);
    }
  }
}

std::vector<ServiceInfo> CastAppDiscoveryServiceImpl::GetDevicesByIds(
    const std::vector<std::string>& device_ids) const {
  std::vector<ServiceInfo> devices;
  for (const std::string& device_id : device_ids) {
    auto entry = devices_by_id_.find(device_id);
    if (entry != devices_by_id_.end()) {
      devices.push_back(entry->second);
    }
  }
  return devices;
}

bool CastAppDiscoveryServiceImpl::ShouldRefreshAppAvailability(
    const std::string& device_id,
    const std::string& app_id,
    Clock::time_point now) const {
  auto availability = availability_tracker_.GetAvailability(device_id, app_id);
  switch (availability.availability) {
    case AppAvailabilityResult::kAvailable:
      return false;
    case AppAvailabilityResult::kUnavailable:
      return (now - availability.time) > kRefreshThreshold;
    case AppAvailabilityResult::kUnknown:
      return true;
  }

  OSP_NOTREACHED();
  return false;
}

uint32_t CastAppDiscoveryServiceImpl::GetNextDeviceQueryId() {
  if (free_query_ids_.empty()) {
    return next_device_query_id_++;
  } else {
    uint32_t id = free_query_ids_.back();
    free_query_ids_.pop_back();
    return id;
  }
}

void CastAppDiscoveryServiceImpl::RemoveDeviceQueryCallback(uint32_t id) {
  for (auto entry = device_queries_.begin(); entry != device_queries_.end();
       ++entry) {
    const std::string& source_id = entry->first;
    auto& callbacks = entry->second;
    auto it =
        std::find_if(callbacks.begin(), callbacks.end(),
                     [id](const DeviceQueryCallbackEntry& callback_entry) {
                       return callback_entry.id == id;
                     });
    if (it != callbacks.end()) {
      callbacks.erase(it);
      if (callbacks.empty()) {
        availability_tracker_.UnregisterSource(source_id);
        device_queries_.erase(entry);
      }
      return;
    }
  }
}

}  // namespace cast
}  // namespace openscreen
