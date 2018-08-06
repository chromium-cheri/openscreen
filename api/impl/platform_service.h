// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_PLATFORM_SERVICE_H_
#define API_IMPL_PLATFORM_SERVICE_H_

#include <memory>
#include <vector>

#include "base/make_unique.h"
#include "discovery/mdns/mdns_responder_adapter.h"
#include "platform/api/event_waiter.h"
#include "platform/api/network_interface.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class PlatformService {
 public:
  PlatformService();
  ~PlatformService();

  void RegisterInterfaces(mdns::MdnsResponderAdapter* mdns_responder);
  void DeregisterInterfaces(mdns::MdnsResponderAdapter* mdns_responder);
  void RunEventLoopOnce();

  template <typename T>
  void SetEventCallback(void (T::*cb)(platform::ReceivedData), T* obj) {
    callback_ = MakeUnique<CallbackModel<T>>(obj, cb);
  }

 private:
  class CallbackConcept {
   public:
    virtual void callback(platform::ReceivedData data) = 0;
  };

  template <typename T>
  class CallbackModel final : public CallbackConcept {
   public:
    CallbackModel(T* obj, void (T::*cb)(platform::ReceivedData))
        : obj_(obj), cb_(cb) {}

    void callback(platform::ReceivedData data) override {
      (obj_->*cb_)(std::move(data));
    }

   private:
    T* obj_;
    void (T::*cb_)(platform::ReceivedData);
  };

  // TODO: Watch network changes.
  std::unique_ptr<CallbackConcept> callback_;
  std::vector<platform::UdpSocketIPv4Ptr> v4_sockets_;
  std::vector<platform::UdpSocketIPv6Ptr> v6_sockets_;
  platform::EventWaiterPtr waiter_;
};

}  // namespace openscreen

#endif  // API_IMPL_PLATFORM_SERVICE_H_
