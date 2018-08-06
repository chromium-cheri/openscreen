// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_INTERNAL_SERVICES_H_
#define API_IMPL_INTERNAL_SERVICES_H_

#include <memory>
#include <vector>

#include "api/impl/platform_service.h"
#include "api/impl/mdns_responder_service.h"
#include "api/impl/screen_listener_impl.h"
#include "api/impl/screen_publisher_impl.h"
#include "api/public/mdns_screen_listener_factory.h"
#include "api/public/mdns_screen_publisher_factory.h"
#include "base/macros.h"
#include "platform/api/event_waiter.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class InternalServices {
 public:
  static void RunEventLoopOnce();

  static std::unique_ptr<ScreenListener> CreateListener(
      const MdnsScreenListenerConfig& config,
      ScreenListenerObserver* observer);
  static std::unique_ptr<ScreenPublisher> CreatePublisher(
      const MdnsScreenPublisherConfig& config,
      ScreenPublisherObserver* observer);

 private:
  static InternalServices* GetInstance();

  InternalServices();
  ~InternalServices();

  void EnsureMdnsServiceCreated();

  // TODO: move necessary things to MdnsResponderService API and remove this.
  PlatformService* platform_;
  std::unique_ptr<MdnsResponderService> mdns_service_;

  DISALLOW_COPY_AND_ASSIGN(InternalServices);
};

}  // namespace openscreen

#endif  // API_IMPL_INTERNAL_SERVICES_H_
