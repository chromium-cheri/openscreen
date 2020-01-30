// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/cast_platform_handler.h"

#include <random>

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/discovery/service_info.h"
#include "util/json/json_serialization.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

static constexpr std::chrono::seconds kRequestTimeout = std::chrono::seconds(5);

namespace {

std::string MakeRandomSenderId() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(1, 1000000);
  std::stringstream ss;
  ss << "sender-" << dist(gen);
  return ss.str();
}

}  // namespace

CastPlatformHandler::CastPlatformHandler(VirtualConnectionRouter* router,
                                         VirtualConnectionManager* manager,
                                         ClockNowFunctionPtr clock,
                                         TaskRunner* task_runner)
    : sender_id_(MakeRandomSenderId()),
      virtual_conn_router_(router),
      virtual_conn_manager_(manager),
      clock_(clock),
      task_runner_(task_runner) {
  OSP_DCHECK(virtual_conn_manager_);
  OSP_DCHECK(clock_);
  OSP_DCHECK(task_runner_);
  virtual_conn_router_->AddHandlerForLocalId(sender_id_, this);
}

CastPlatformHandler::~CastPlatformHandler() {
  virtual_conn_router_->RemoveHandlerForLocalId(sender_id_);
}

void CastPlatformHandler::RequestAppAvailability(
    const std::string& device_id,
    const std::string& app_id,
    AppAvailabilityCallback callback) {
  auto entry = socket_by_device_.find(device_id);
  if (entry == socket_by_device_.end()) {
    callback(app_id, AppAvailabilityResult::kUnknown);
    return;
  }
  uint32_t socket_id = entry->second;

  int32_t request_id = NextRequestId();
  ErrorOr<::cast::channel::CastMessage> message =
      CreateAppAvailabilityRequest(sender_id_, request_id, app_id);
  if (message.is_error()) {
    callback(app_id, AppAvailabilityResult::kUnknown);
    return;
  }

  PendingRequests& pending_requests = pending_requests_by_device_[device_id];
  auto timeout = std::make_unique<Alarm>(clock_, task_runner_);
  timeout->ScheduleFromNow(
      [this, request_id]() { CancelAppAvailabilityRequest(request_id); },
      kRequestTimeout);
  pending_requests.availability.push_back(AvailabilityRequest{
      request_id, app_id, std::move(timeout), std::move(callback)});

  VirtualConnection virtual_conn{sender_id_, kPlatformReceiverId, socket_id};
  if (!virtual_conn_manager_->GetConnectionData(virtual_conn)) {
    virtual_conn_manager_->AddConnection(virtual_conn,
                                         VirtualConnection::AssociatedData{});
  }

  virtual_conn_router_->SendMessage(std::move(virtual_conn),
                                    std::move(message.value()));
}

void CastPlatformHandler::OnDeviceAddedOrUpdated(const ServiceInfo& device,
                                                 uint32_t socket_id) {
  socket_by_device_[device.unique_id] = socket_id;
}

void CastPlatformHandler::OnDeviceRemoved(const ServiceInfo& device) {
  auto pending_requests = pending_requests_by_device_.find(device.unique_id);
  if (pending_requests != pending_requests_by_device_.end()) {
    for (const AvailabilityRequest& availability :
         pending_requests->second.availability) {
      availability.callback(availability.app_id,
                            AppAvailabilityResult::kUnknown);
    }
    pending_requests_by_device_.erase(pending_requests);
  }
  socket_by_device_.erase(device.unique_id);
}

void CastPlatformHandler::OnMessage(VirtualConnectionRouter* router,
                                    CastSocket* socket,
                                    ::cast::channel::CastMessage message) {
  if (message.payload_type() !=
          ::cast::channel::CastMessage_PayloadType_STRING ||
      message.namespace_() != kReceiverNamespace ||
      message.source_id() != kPlatformReceiverId) {
    return;
  }
  ErrorOr<Json::Value> dict_or_error = json::Parse(message.payload_utf8());
  if (dict_or_error.is_error()) {
    return;
  }

  Json::Value& dict = dict_or_error.value();
  absl::optional<int> request_id =
      MaybeGetInt(dict, JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyRequestId));
  if (request_id) {
    auto entry =
        std::find_if(socket_by_device_.begin(), socket_by_device_.end(),
                     [socket](const std::pair<std::string, uint32_t>& entry) {
                       return entry.second == socket->socket_id();
                     });
    if (entry != socket_by_device_.end()) {
      HandleResponse(entry->first, request_id.value(), dict);
    }
  }
}

void CastPlatformHandler::HandleResponse(const std::string& device_id,
                                         int32_t request_id,
                                         const Json::Value& message) {
  auto entry = pending_requests_by_device_.find(device_id);
  if (entry == pending_requests_by_device_.end()) {
    return;
  }
  PendingRequests& pending_requests = entry->second;
  auto it = std::find_if(pending_requests.availability.begin(),
                         pending_requests.availability.end(),
                         [request_id](const AvailabilityRequest& request) {
                           return request.request_id == request_id;
                         });
  if (it != pending_requests.availability.end()) {
    const Json::Value* maybe_availability =
        message.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kMessageKeyAvailability));
    if (maybe_availability && maybe_availability->isObject()) {
      absl::optional<absl::string_view> result =
          MaybeGetString(*maybe_availability, &it->app_id[0],
                         &it->app_id[0] + it->app_id.size());
      if (result) {
        AppAvailabilityResult availability_result =
            AppAvailabilityResult::kUnknown;
        if (result.value() == kMessageValueAppAvailable) {
          availability_result = AppAvailabilityResult::kAvailable;
        } else if (result.value() == kMessageValueAppUnavailable) {
          availability_result = AppAvailabilityResult::kUnavailable;
        }
        it->callback(it->app_id, availability_result);
      }
    }
    pending_requests.availability.erase(it);
  }
}

void CastPlatformHandler::CancelAppAvailabilityRequest(int32_t request_id) {
  for (auto& entry : pending_requests_by_device_) {
    PendingRequests& pending_requests = entry.second;
    auto it = std::find_if(pending_requests.availability.begin(),
                           pending_requests.availability.end(),
                           [request_id](const AvailabilityRequest& request) {
                             return request.request_id == request_id;
                           });
    if (it != pending_requests.availability.end()) {
      it->callback(it->app_id, AppAvailabilityResult::kUnknown);
      pending_requests.availability.erase(it);
    }
  }
}

int32_t CastPlatformHandler::NextRequestId() {
  return next_request_id++;
}

}  // namespace cast
}  // namespace openscreen
