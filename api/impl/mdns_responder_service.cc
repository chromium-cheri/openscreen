// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/mdns_responder_service.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace {

std::string ScreenIdFromServiceInstance(
    const mdns::DomainName& service_instance) {
  std::string screen_id;
  screen_id.assign(
      reinterpret_cast<const char*>(service_instance.domain_name.data()),
      service_instance.domain_name.size());
  return screen_id;
}

std::vector<std::string> SplitByDot(const std::string& domain_part) {
  std::vector<std::string> result;
  auto copy_it = domain_part.begin();
  for (auto it = domain_part.begin(); it != domain_part.end(); ++it) {
    if (*it == '.') {
      result.emplace_back(copy_it, it);
      copy_it = it + 1;
    }
  }
  if (copy_it != domain_part.end()) {
    result.emplace_back(copy_it, domain_part.end());
  }
  return result;
}

}  // namespace

MdnsResponderService::MdnsResponderService(
    const std::string& service_type,
    std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
    std::unique_ptr<PlatformService> platform)
    : service_type_(service_type),
      mdns_responder_factory_(std::move(mdns_responder_factory)),
      platform_(std::move(platform)) {
  platform_->SetEventCallback(&MdnsResponderService::HandleNewEvents, this);
}

MdnsResponderService::~MdnsResponderService() = default;

void MdnsResponderService::HandleNewEvents(platform::ReceivedData data) {
  if (!mdns_responder_)
    return;
  for (auto& packet : data.v4_data) {
    mdns_responder_->OnDataReceived(packet.source, packet.original_destination,
                                    packet.bytes.data(), packet.length,
                                    packet.socket);
  }
  for (auto& packet : data.v6_data) {
    mdns_responder_->OnDataReceived(packet.source, packet.original_destination,
                                    packet.bytes.data(), packet.length,
                                    packet.socket);
  }
  mdns_responder_->Execute();

  HandleMdnsEvents();
}

void MdnsResponderService::StartListener() {
  mdns_responder_ = mdns_responder_factory_->Create();
  mdns_responder_->Init();
  platform_->RegisterInterfaces(mdns_responder_.get());
  mdns::DomainName service_type;
  CHECK(mdns::DomainName::FromLabels(SplitByDot(service_type_), &service_type));
  mdns_responder_->StartPtrQuery(service_type);
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kRunning);
}

void MdnsResponderService::StartAndSuspendListener() {
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kSuspended);
}

void MdnsResponderService::StopListener() {
  platform_->DeregisterInterfaces(mdns_responder_.get());
  mdns_responder_->Close();
  mdns_responder_.reset();
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kStopped);
}

void MdnsResponderService::SuspendListener() {
  platform_->DeregisterInterfaces(mdns_responder_.get());
  mdns_responder_->Close();
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kSuspended);
}

void MdnsResponderService::ResumeListener() {
  mdns_responder_->Init();
  platform_->RegisterInterfaces(mdns_responder_.get());
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kRunning);
}

void MdnsResponderService::SearchNow(ScreenListenerState from) {}

// TODO
void MdnsResponderService::StartPublisher() {}
void MdnsResponderService::StartAndSuspendPublisher() {}
void MdnsResponderService::StopPublisher() {}
void MdnsResponderService::SuspendPublisher() {}
void MdnsResponderService::ResumePublisher() {}
void MdnsResponderService::UpdateFriendlyName(
    const std::string& friendly_name) {
  mdns_responder_->SetHostLabel(friendly_name);
}

void MdnsResponderService::HandleMdnsEvents() {
  for (auto& ptr_event : mdns_responder_->TakePtrResponses()) {
    auto entry = services_.find(ptr_event.service_instance);
    switch (ptr_event.header.response_type) {
      case mdns::ResponseType::kAdd:
      case mdns::ResponseType::kAddNoCache:
        mdns_responder_->StartSrvQuery(ptr_event.service_instance);
        mdns_responder_->StartTxtQuery(ptr_event.service_instance);
        if (entry == services_.end()) {
          services_.emplace(std::move(ptr_event.service_instance),
                            ServiceInstance{});
        } else {
          entry->second.has_ptr = true;
        }
        MaybePushScreenInfo(ptr_event.service_instance, entry->second);
        break;
      case mdns::ResponseType::kRemove:
        if (entry != services_.end()) {
          entry->second.has_ptr = false;
          RemoveScreenInfo(ptr_event.service_instance);
        }
        break;
    }
  }
  for (auto& srv_event : mdns_responder_->TakeSrvResponses()) {
    auto entry = services_.find(srv_event.service_instance);
    switch (srv_event.header.response_type) {
      case mdns::ResponseType::kAdd:
      case mdns::ResponseType::kAddNoCache:
        mdns_responder_->StartAQuery(srv_event.domain_name);
        if (entry == services_.end()) {
          auto result = services_.emplace(std::move(srv_event.service_instance),
                                          ServiceInstance{});
          entry = result.first;
        }
        entry->second.domain_name = std::move(srv_event.domain_name);
        entry->second.port = srv_event.port;
        MaybePushScreenInfo(srv_event.service_instance, entry->second);
        break;
      case mdns::ResponseType::kRemove:
        entry->second.domain_name = mdns::DomainName();
        entry->second.port = 0;
        RemoveScreenInfo(srv_event.service_instance);
        break;
    }
  }
  for (auto& txt_event : mdns_responder_->TakeTxtResponses()) {
    auto entry = services_.find(txt_event.service_instance);
    switch (txt_event.header.response_type) {
      case mdns::ResponseType::kAdd:
      case mdns::ResponseType::kAddNoCache:
        if (entry == services_.end()) {
          auto result = services_.emplace(std::move(txt_event.service_instance),
                                          ServiceInstance{});
          entry = result.first;
        }
        entry->second.txt_info = std::move(txt_event.txt_info);
        MaybePushScreenInfo(txt_event.service_instance, entry->second);
        break;
      case mdns::ResponseType::kRemove:
        entry->second.txt_info.clear();
        RemoveScreenInfo(txt_event.service_instance);
        break;
    }
  }
  for (const auto& a_event : mdns_responder_->TakeAResponses()) {
    switch (a_event.header.response_type) {
      case mdns::ResponseType::kAdd:
      case mdns::ResponseType::kAddNoCache: {
        auto& addresses = addresses_[a_event.domain_name];
        addresses.v4_address = a_event.address;
        MaybePushScreenInfo(a_event.domain_name, addresses);
      } break;
      case mdns::ResponseType::kRemove: {
        auto entry = addresses_.find(a_event.domain_name);
        if (entry != addresses_.end()) {
          entry->second.v4_address = IPv4Address();
          if (!entry->second.v6_address) {
            addresses_.erase(entry);
            RemoveScreenInfoByDomain(a_event.domain_name);
          }
        }
      } break;
    }
  }
  for (const auto& aaaa_event : mdns_responder_->TakeAaaaResponses()) {
    switch (aaaa_event.header.response_type) {
      case mdns::ResponseType::kAdd:
      case mdns::ResponseType::kAddNoCache: {
        auto& addresses = addresses_[aaaa_event.domain_name];
        addresses.v6_address = aaaa_event.address;
        MaybePushScreenInfo(aaaa_event.domain_name, addresses);
      } break;
      case mdns::ResponseType::kRemove: {
        auto entry = addresses_.find(aaaa_event.domain_name);
        if (entry != addresses_.end()) {
          entry->second.v6_address = IPv6Address();
          if (!entry->second.v4_address) {
            addresses_.erase(entry);
            RemoveScreenInfoByDomain(aaaa_event.domain_name);
          }
        }
      } break;
    }
  }
}

void MdnsResponderService::PushScreenInfo(
    const mdns::DomainName& service_instance,
    const ServiceInstance& instance_info,
    const ServiceAddresses& addresses) {
  std::string screen_id;
  screen_id.assign(
      reinterpret_cast<const char*>(service_instance.domain_name.data()),
      service_instance.domain_name.size());

  std::string friendly_name;
  for (const auto& line : instance_info.txt_info) {
    if (std::equal(std::begin("fn="), std::begin("fn=") + 3, line.begin())) {
      friendly_name = line.substr(3);
    }
  }

  auto entry = screen_info_.find(screen_id);
  if (entry == screen_info_.end()) {
    ScreenInfo screen_info;
    screen_info.screen_id = std::move(screen_id);
    screen_info.friendly_name = std::move(friendly_name);
    listener_->OnScreenAdded(screen_info);
    screen_info_.emplace(screen_info.screen_id, std::move(screen_info));
  } else {
    auto& screen_info = entry->second;
    if (screen_info.friendly_name != friendly_name) {
      screen_info.friendly_name = std::move(friendly_name);
      listener_->OnScreenChanged(screen_info);
    }
  }
}

void MdnsResponderService::MaybePushScreenInfo(
    const mdns::DomainName& service_instance,
    const ServiceInstance& instance_info) {
  if (!instance_info.has_ptr || instance_info.txt_info.empty() ||
      instance_info.domain_name.domain_name[0] == 0 || instance_info.port == 0)
    return;
  auto entry = addresses_.find(instance_info.domain_name);
  if (entry == addresses_.end())
    return;
}

void MdnsResponderService::MaybePushScreenInfo(
    const mdns::DomainName& domain_name,
    const ServiceAddresses& address_info) {
  for (auto& entry : services_) {
    if (entry.second.domain_name == domain_name) {
      ScreenInfo screen_info;
      screen_info.screen_id.assign(
          reinterpret_cast<const char*>(entry.first.domain_name.data()),
          entry.first.domain_name.size());
      for (const auto& line : entry.second.txt_info) {
        if (std::equal(std::begin("fn="), std::begin("fn=") + 3,
                       line.begin())) {
          screen_info.friendly_name = line.substr(3);
        }
      }
      listener_->OnScreenAdded(screen_info);
    }
  }
}

void MdnsResponderService::RemoveScreenInfo(
    const mdns::DomainName& service_instance) {
  const auto screen_id = ScreenIdFromServiceInstance(service_instance);
  auto entry = screen_info_.find(screen_id);
  if (entry == screen_info_.end())
    return;
  listener_->OnScreenRemoved(entry->second);
  screen_info_.erase(entry);
}

void MdnsResponderService::RemoveScreenInfoByDomain(
    const mdns::DomainName& domain_name) {
  for (const auto& entry : services_) {
    if (entry.second.domain_name == domain_name) {
      RemoveScreenInfo(entry.first);
    }
  }
}

bool MdnsResponderService::DomainNameComparator::operator()(
    const mdns::DomainName& a,
    const mdns::DomainName& b) const {
  return a.domain_name < b.domain_name;
}

}  // namespace openscreen
