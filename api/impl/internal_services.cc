// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/internal_services.h"

#include "base/make_unique.h"
#include "discovery/mdns/mdns_responder_adapter_impl.h"

namespace openscreen {
namespace {

constexpr char kServiceType[] = "_googlecast._tcp";

class MdnsResponderAdapterImplFactory final
    : public MdnsResponderAdapterFactory {
 public:
  MdnsResponderAdapterImplFactory() = default;
  ~MdnsResponderAdapterImplFactory() override = default;

  std::unique_ptr<mdns::MdnsResponderAdapter> Create() override {
    return MakeUnique<mdns::MdnsResponderAdapterImpl>();
  }
};

}  // namespace

// static
void InternalServices::RunEventLoopOnce() {
  auto* services = GetInstance();
  services->platform_->RunEventLoopOnce();
}

// static
std::unique_ptr<ScreenListener> InternalServices::CreateListener(
    const MdnsScreenListenerConfig& config,
    ScreenListenerObserver* observer) {
  auto* services = GetInstance();
  services->EnsureMdnsServiceCreated();
  auto listener =
      MakeUnique<ScreenListenerImpl>(observer, services->mdns_service_.get());
  // TODO: use only interfaces requested in config
  return listener;
}

// static
std::unique_ptr<ScreenPublisher> InternalServices::CreatePublisher(
    const MdnsScreenPublisherConfig& config,
    ScreenPublisherObserver* observer) {
  // TODO: use interfaces requested in config
  return nullptr;
}

// static
InternalServices* InternalServices::GetInstance() {
  static InternalServices services;
  return &services;
}

InternalServices::InternalServices() = default;
InternalServices::~InternalServices() = default;

void InternalServices::EnsureMdnsServiceCreated() {
  if (mdns_service_) {
    return;
  }
  auto mdns_responder_factory = MakeUnique<MdnsResponderAdapterImplFactory>();
  auto platform = MakeUnique<PlatformService>();
  platform_ = platform.get();
  mdns_service_ = MakeUnique<MdnsResponderService>(
      kServiceType, std::move(mdns_responder_factory), std::move(platform));
}

}  // namespace openscreen
