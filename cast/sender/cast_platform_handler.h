// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CAST_PLATFORM_HANDLER_H_
#define CAST_SENDER_CAST_PLATFORM_HANDLER_H_

#include <functional>
#include <map>
#include <string>

#include "cast/common/channel/cast_message_handler.h"
#include "cast/sender/channel/message_util.h"
#include "util/alarm.h"
#include "util/json/json_value.h"

namespace openscreen {
namespace cast {

struct ServiceInfo;
class VirtualConnectionManager;
class VirtualConnectionRouter;

// This class handles Cast messages that generally relate to the "platform", in
// other words not a specific app currently running (e.g. app availability,
// receiver status).  These messages follow a request/response format, so each
// request requires a corresponding response callback.  These requests will also
// timeout if there is no response after a certain amount of time (currently 5
// seconds).  The timeout callbacks will be called on the thread managed by
// |task_runner|.
// TODO(btolsch): Only heartbeat and auth _actually_ use sender-0, so should
// they go here or split and rename this?
class CastPlatformHandler final : public CastMessageHandler {
 public:
  using AppAvailabilityCallback =
      std::function<void(const std::string&, AppAvailabilityResult)>;

  CastPlatformHandler(VirtualConnectionRouter* router,
                      VirtualConnectionManager* manager,
                      ClockNowFunctionPtr clock,
                      TaskRunner* task_runner);
  ~CastPlatformHandler() override;

  // TODO(btolsch): Subscription object for cancelling these requests.
  void RequestAppAvailability(const std::string& device_id,
                              const std::string& app_id,
                              AppAvailabilityCallback callback);

  void OnDeviceAddedOrUpdated(const ServiceInfo& device, uint32_t socket_id);
  void OnDeviceRemoved(const ServiceInfo& device);

  // CastMessageHandler overrides.
  void OnMessage(VirtualConnectionRouter* router,
                 CastSocket* socket,
                 ::cast::channel::CastMessage message) override;

 private:
  struct AvailabilityRequest {
    int32_t request_id;
    std::string app_id;
    std::unique_ptr<Alarm> timeout;
    AppAvailabilityCallback callback;
  };

  struct PendingRequests {
    std::vector<AvailabilityRequest> availability;
  };

  void HandleResponse(const std::string& device_id,
                      int32_t request_id,
                      const Json::Value& message);

  void CancelAppAvailabilityRequest(int32_t request_id);

  int32_t NextRequestId();

  const std::string sender_id_;
  int32_t next_request_id = 0;
  VirtualConnectionRouter* const virtual_conn_router_;
  VirtualConnectionManager* const virtual_conn_manager_;
  std::map<std::string, uint32_t> socket_by_device_;
  std::map<std::string, PendingRequests> pending_requests_by_device_;

  const ClockNowFunctionPtr clock_;
  TaskRunner* const task_runner_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_SENDER_CAST_PLATFORM_HANDLER_H_
