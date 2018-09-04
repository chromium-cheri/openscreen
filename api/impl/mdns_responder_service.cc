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
      reinterpret_cast<const char*>(service_instance.domain_name().data()),
      service_instance.domain_name().size());
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
    std::unique_ptr<MdnsPlatformService> platform)
    : mdns_responder_factory_(std::move(mdns_responder_factory)),
      platform_(std::move(platform)) {
  const auto labels = SplitByDot(service_type);
  CHECK_EQ(2u, labels.size())
      << "bad service-type configured: " << service_type;
  service_type_[0] = std::move(labels[0]);
  service_type_[1] = std::move(labels[1]);
}

MdnsResponderService::~MdnsResponderService() = default;

void MdnsResponderService::SetServiceConfig(
    const std::string& hostname,
    const std::string& instance,
    uint16_t port,
    const std::vector<int32_t> interface_index_whitelist,
    const std::vector<std::string>& txt_lines) {
  DCHECK(!hostname.empty());
  DCHECK(!instance.empty());
  DCHECK_NE(0, port);
  hostname_ = hostname;
  instance_ = instance;
  port_ = port;
  interface_index_whitelist_ = interface_index_whitelist;
  txt_lines_ = txt_lines;
}

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
  mdns_responder_->RunTasks();

  HandleMdnsEvents();
}

void MdnsResponderService::StartListener() {
  if (!mdns_responder_) {
    mdns_responder_ = mdns_responder_factory_->Create();
  }
  StartListening();
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kRunning);
}

void MdnsResponderService::StartAndSuspendListener() {
  mdns_responder_ = mdns_responder_factory_->Create();
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kSuspended);
}

void MdnsResponderService::StopListener() {
  StopListening();
  if (publisher_->state() == ScreenPublisher::State::kStopped) {
    StopMdnsResponder();
    mdns_responder_.reset();
  }
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kStopped);
}

void MdnsResponderService::SuspendListener() {
  // TODO(btolsch): What state should suspend actually produce for both
  // listening and publishing?
  StopMdnsResponder();
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kSuspended);
}

void MdnsResponderService::ResumeListener() {
  StartListening();
  ScreenListenerImpl::Delegate::SetState(ScreenListenerState::kRunning);
}

void MdnsResponderService::SearchNow(ScreenListenerState from) {
  ScreenListenerImpl::Delegate::SetState(from);
}

void MdnsResponderService::StartPublisher() {
  if (!mdns_responder_) {
    mdns_responder_ = mdns_responder_factory_->Create();
  }
  StartService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kRunning);
}

void MdnsResponderService::StartAndSuspendPublisher() {
  mdns_responder_ = mdns_responder_factory_->Create();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kSuspended);
}

void MdnsResponderService::StopPublisher() {
  StopService();
  // TODO(btolsch): We could also only call StopMdnsResponder for
  // ScreenListenerState::kSuspended.  If these ever become asynchronous
  // (unlikely), we'd also have to consider kStarting/kStopping/etc.  Similar
  // comments apply to StopListener.
  if (listener_->state() == ScreenListenerState::kStopped) {
    StopMdnsResponder();
    mdns_responder_.reset();
  }
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kStopped);
}

void MdnsResponderService::SuspendPublisher() {
  StopService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kSuspended);
}

void MdnsResponderService::ResumePublisher() {
  StartService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kRunning);
}

void MdnsResponderService::UpdateFriendlyName(
    const std::string& friendly_name) {
  // TODO: You keep using that word.  I do not think it means what you think it
  // means.
  mdns_responder_->SetHostLabel(friendly_name);
}

void MdnsResponderService::HandleMdnsEvents() {
  // NOTE: In the worst case, we might get a single combined packet for
  // PTR/SRV/TXT and then no other packets.  If we don't loop here, we would
  // start SRV/TXT queries based on the PTR response, but never check for events
  // again.  This should no longer be a problem when we have correct scheduling
  // of RunTasks.
  bool events_possible = true;
  while (events_possible) {
    events_possible = false;
    for (auto& ptr_event : mdns_responder_->TakePtrResponses()) {
      auto entry = services_.find(ptr_event.service_instance);
      switch (ptr_event.header.response_type) {
        case mdns::QueryEventHeader::Type::kAdded:
        case mdns::QueryEventHeader::Type::kAddedNoCache: {
          mdns_responder_->StartSrvQuery(ptr_event.service_instance);
          mdns_responder_->StartTxtQuery(ptr_event.service_instance);
          events_possible = true;
          int32_t interface_index = 0;
          if (ptr_event.header.receiving_socket_type ==
              mdns::QueryEventHeader::SocketType::kIPv4) {
            auto v4_socket = ptr_event.header.v4_socket;
            auto it = std::find_if(
                bound_interfaces_.v4_interfaces.begin(),
                bound_interfaces_.v4_interfaces.end(),
                [v4_socket](
                    const MdnsPlatformService::BoundInterfaceIPv4& interface) {
                  return v4_socket == interface.socket;
                });
            CHECK(it != bound_interfaces_.v4_interfaces.end());
            interface_index = it->interface_info.index;
          } else {
            // TODO(btolsch): ipv6
            UNIMPLEMENTED() << "ipv6";
          }
          if (entry == services_.end()) {
            ServiceInstance new_instance;
            new_instance.ptr_interface_index = interface_index;
            services_.emplace(std::move(ptr_event.service_instance),
                              new_instance);
          } else {
            entry->second.ptr_interface_index = interface_index;
          }
          MaybePushScreenInfo(ptr_event.service_instance, entry->second);
        } break;
        case mdns::QueryEventHeader::Type::kRemoved:
          if (entry != services_.end()) {
            entry->second.ptr_interface_index = 0;
            RemoveScreenInfo(ptr_event.service_instance);
          }
          break;
      }
    }
    for (auto& srv_event : mdns_responder_->TakeSrvResponses()) {
      auto entry = services_.find(srv_event.service_instance);
      switch (srv_event.header.response_type) {
        case mdns::QueryEventHeader::Type::kAdded:
        case mdns::QueryEventHeader::Type::kAddedNoCache:
          mdns_responder_->StartAQuery(srv_event.domain_name);
          events_possible = true;
          if (entry == services_.end()) {
            auto result = services_.emplace(
                std::move(srv_event.service_instance), ServiceInstance{});
            entry = result.first;
          }
          entry->second.domain_name = std::move(srv_event.domain_name);
          entry->second.port = srv_event.port;
          MaybePushScreenInfo(srv_event.service_instance, entry->second);
          break;
        case mdns::QueryEventHeader::Type::kRemoved:
          entry->second.domain_name = mdns::DomainName();
          entry->second.port = 0;
          RemoveScreenInfo(srv_event.service_instance);
          break;
      }
    }
    for (auto& txt_event : mdns_responder_->TakeTxtResponses()) {
      auto entry = services_.find(txt_event.service_instance);
      switch (txt_event.header.response_type) {
        case mdns::QueryEventHeader::Type::kAdded:
        case mdns::QueryEventHeader::Type::kAddedNoCache:
          if (entry == services_.end()) {
            auto result = services_.emplace(
                std::move(txt_event.service_instance), ServiceInstance{});
            entry = result.first;
          }
          entry->second.txt_info = std::move(txt_event.txt_info);
          MaybePushScreenInfo(txt_event.service_instance, entry->second);
          break;
        case mdns::QueryEventHeader::Type::kRemoved:
          entry->second.txt_info.clear();
          RemoveScreenInfo(txt_event.service_instance);
          break;
      }
    }
    for (const auto& a_event : mdns_responder_->TakeAResponses()) {
      switch (a_event.header.response_type) {
        case mdns::QueryEventHeader::Type::kAdded:
        case mdns::QueryEventHeader::Type::kAddedNoCache: {
          auto& addresses = addresses_[a_event.domain_name];
          addresses.v4_address = a_event.address;
          MaybePushScreenInfo(a_event.domain_name, addresses);
        } break;
        case mdns::QueryEventHeader::Type::kRemoved: {
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
        case mdns::QueryEventHeader::Type::kAdded:
        case mdns::QueryEventHeader::Type::kAddedNoCache: {
          auto& addresses = addresses_[aaaa_event.domain_name];
          addresses.v6_address = aaaa_event.address;
          MaybePushScreenInfo(aaaa_event.domain_name, addresses);
        } break;
        case mdns::QueryEventHeader::Type::kRemoved: {
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
    if (events_possible) {
      mdns_responder_->RunTasks();
    }
  }
}

void MdnsResponderService::StartListening() {
  if (bound_interfaces_.v4_interfaces.empty() &&
      bound_interfaces_.v6_interfaces.empty()) {
    mdns_responder_->Init();
    bound_interfaces_ = platform_->RegisterInterfaces({});
    for (auto& interface : bound_interfaces_.v4_interfaces) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
    for (auto& interface : bound_interfaces_.v6_interfaces) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
  }
  mdns::DomainName service_type;
  CHECK(mdns::DomainName::FromLabels(service_type_.begin(), service_type_.end(),
                                     &service_type));
  mdns_responder_->StartPtrQuery(service_type);
}

void MdnsResponderService::StopListening() {
  mdns::DomainName service_type;
  CHECK(mdns::DomainName::FromLabels(service_type_.begin(), service_type_.end(),
                                     &service_type));
  // TODO(btolsch): Stop other queries based on |services_| and |addresses_|.
  mdns_responder_->StopPtrQuery(service_type);
}

void MdnsResponderService::StartService() {
  if (!bound_interfaces_.v4_interfaces.empty() ||
      !bound_interfaces_.v6_interfaces.empty()) {
    MdnsPlatformService::BoundInterfaces deregistered_interfaces;
    for (auto it = bound_interfaces_.v4_interfaces.begin();
         it != bound_interfaces_.v4_interfaces.end();) {
      if (std::find(interface_index_whitelist_.begin(),
                    interface_index_whitelist_.end(),
                    it->interface_info.index) ==
          interface_index_whitelist_.end()) {
        mdns_responder_->DeregisterInterface(it->socket);
        deregistered_interfaces.v4_interfaces.push_back(*it);
        it = bound_interfaces_.v4_interfaces.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = bound_interfaces_.v6_interfaces.begin();
         it != bound_interfaces_.v6_interfaces.end();) {
      if (std::find(interface_index_whitelist_.begin(),
                    interface_index_whitelist_.end(),
                    it->interface_info.index) ==
          interface_index_whitelist_.end()) {
        mdns_responder_->DeregisterInterface(it->socket);
        deregistered_interfaces.v6_interfaces.push_back(*it);
        it = bound_interfaces_.v6_interfaces.erase(it);
      } else {
        ++it;
      }
    }
    platform_->DeregisterInterfaces(deregistered_interfaces);
  } else {
    mdns_responder_->Init();
    bound_interfaces_ =
        platform_->RegisterInterfaces(interface_index_whitelist_);
    for (auto& interface : bound_interfaces_.v4_interfaces) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
    for (auto& interface : bound_interfaces_.v6_interfaces) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
  }
  mdns_responder_->SetHostLabel(hostname_);
  mdns::DomainName domain_name;
  CHECK(mdns::DomainName::FromLabels(&hostname_, &hostname_ + 1, &domain_name))
      << "bad hostname configured: " << hostname_;
  CHECK(domain_name.Append(mdns::DomainName::kLocalDomain));
  mdns_responder_->RegisterService(instance_, service_type_[0],
                                   service_type_[1], domain_name, port_,
                                   txt_lines_);
}

void MdnsResponderService::StopService() {
  mdns_responder_->DeregisterService(instance_, service_type_[0],
                                     service_type_[1]);
}

void MdnsResponderService::StopMdnsResponder() {
  mdns_responder_->Close();
  platform_->DeregisterInterfaces(bound_interfaces_);
  bound_interfaces_.v4_interfaces.clear();
  bound_interfaces_.v6_interfaces.clear();
}

void MdnsResponderService::PushScreenInfo(
    const mdns::DomainName& service_instance,
    const ServiceInstance& instance_info,
    const ServiceAddresses& addresses) {
  std::string screen_id;
  screen_id.assign(
      reinterpret_cast<const char*>(service_instance.domain_name().data()),
      service_instance.domain_name().size());

  std::string friendly_name;
  for (const auto& line : instance_info.txt_info) {
    // TODO(btolsch): Placeholder until TXT data is spec'd.
    if (line.size() > 3 &&
        std::equal(std::begin("fn="), std::begin("fn=") + 3, line.begin())) {
      friendly_name = line.substr(3);
      break;
    }
  }

  auto entry = screen_info_.find(screen_id);
  if (entry == screen_info_.end()) {
    ScreenInfo screen_info;
    screen_info.screen_id = std::move(screen_id);
    screen_info.friendly_name = std::move(friendly_name);
    screen_info.network_interface_index = instance_info.ptr_interface_index;
    if (addresses.v4_address) {
      screen_info.ipv4_endpoint = {addresses.v4_address, instance_info.port};
    }
    if (addresses.v6_address) {
      screen_info.ipv6_endpoint = {addresses.v6_address, instance_info.port};
    }
    listener_->OnScreenAdded(screen_info);
    screen_info_.emplace(screen_info.screen_id, std::move(screen_info));
  } else {
    auto& screen_info = entry->second;
    bool changed = false;
    if (screen_info.friendly_name != friendly_name) {
      screen_info.friendly_name = std::move(friendly_name);
      changed = true;
    }
    if (screen_info.network_interface_index !=
        instance_info.ptr_interface_index) {
      screen_info.network_interface_index = instance_info.ptr_interface_index;
      changed = true;
    }
    if (screen_info.ipv4_endpoint.address != addresses.v4_address ||
        screen_info.ipv4_endpoint.port != instance_info.port) {
      screen_info.ipv4_endpoint = {addresses.v4_address, instance_info.port};
      changed = true;
    }
    if (screen_info.ipv6_endpoint.address != addresses.v6_address ||
        screen_info.ipv6_endpoint.port != instance_info.port) {
      screen_info.ipv6_endpoint = {addresses.v6_address, instance_info.port};
      changed = true;
    }
    if (changed) {
      listener_->OnScreenChanged(screen_info);
    }
  }
}

void MdnsResponderService::MaybePushScreenInfo(
    const mdns::DomainName& service_instance,
    const ServiceInstance& instance_info) {
  if (!instance_info.ptr_interface_index || instance_info.txt_info.empty() ||
      instance_info.domain_name.IsEmpty() || instance_info.port == 0) {
    return;
  }
  auto entry = addresses_.find(instance_info.domain_name);
  if (entry == addresses_.end()) {
    return;
  }
  PushScreenInfo(service_instance, instance_info, entry->second);
}

void MdnsResponderService::MaybePushScreenInfo(
    const mdns::DomainName& domain_name,
    const ServiceAddresses& address_info) {
  for (auto& entry : services_) {
    if (entry.second.domain_name == domain_name) {
      PushScreenInfo(entry.first, entry.second, address_info);
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

}  // namespace openscreen
