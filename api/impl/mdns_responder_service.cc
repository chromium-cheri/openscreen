// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/mdns_responder_service.h"

#include <algorithm>
#include <utility>

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace {

std::string ScreenIdFromServiceInstanceName(
    const mdns::DomainName& service_instance) {
  std::string screen_id;
  screen_id.assign(
      reinterpret_cast<const char*>(service_instance.domain_name().data()),
      service_instance.domain_name().size());
  return screen_id;
}

}  // namespace

MdnsResponderService::MdnsResponderService(
    const std::string& service_name,
    const std::string& service_protocol,
    std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
    std::unique_ptr<MdnsPlatformService> platform)
    : service_type_{{service_name, service_protocol}},
      mdns_responder_factory_(std::move(mdns_responder_factory)),
      platform_(std::move(platform)) {}

MdnsResponderService::~MdnsResponderService() = default;

void MdnsResponderService::SetServiceConfig(
    const std::string& hostname,
    const std::string& instance,
    uint16_t port,
    const std::vector<platform::InterfaceIndex> whitelist,
    const std::map<std::string, std::string>& txt_data) {
  OSP_DCHECK(!hostname.empty());
  OSP_DCHECK(!instance.empty());
  OSP_DCHECK_NE(0, port);
  service_hostname_ = hostname;
  service_instance_name_ = instance;
  service_port_ = port;
  interface_index_whitelist_ = whitelist;
  service_txt_data_ = txt_data;
}

void MdnsResponderService::HandleNewEvents(
    const std::vector<platform::ReceivedData>& data) {
  if (!mdns_responder_)
    return;
  for (auto& packet : data) {
    mdns_responder_->OnDataReceived(packet.source, packet.original_destination,
                                    packet.bytes.data(), packet.length,
                                    packet.socket);
  }
  mdns_responder_->RunTasks();

  HandleMdnsEvents();
}

void MdnsResponderService::StartListener() {
  if (!mdns_responder_)
    mdns_responder_ = mdns_responder_factory_->Create();

  StartListening();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kRunning);
}

void MdnsResponderService::StartAndSuspendListener() {
  mdns_responder_ = mdns_responder_factory_->Create();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kSuspended);
}

void MdnsResponderService::StopListener() {
  StopListening();
  if (!publisher_ || publisher_->state() == ScreenPublisher::State::kStopped ||
      publisher_->state() == ScreenPublisher::State::kSuspended) {
    StopMdnsResponder();
    if (!publisher_ || publisher_->state() == ScreenPublisher::State::kStopped)
      mdns_responder_.reset();
  }
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kStopped);
}

void MdnsResponderService::SuspendListener() {
  StopMdnsResponder();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kSuspended);
}

void MdnsResponderService::ResumeListener() {
  StartListening();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kRunning);
}

void MdnsResponderService::SearchNow(ScreenListener::State from) {
  ScreenListenerImpl::Delegate::SetState(from);
}

void MdnsResponderService::StartPublisher() {
  if (!mdns_responder_)
    mdns_responder_ = mdns_responder_factory_->Create();

  StartService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kRunning);
}

void MdnsResponderService::StartAndSuspendPublisher() {
  mdns_responder_ = mdns_responder_factory_->Create();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kSuspended);
}

void MdnsResponderService::StopPublisher() {
  StopService();
  if (!listener_ || listener_->state() == ScreenListener::State::kStopped ||
      listener_->state() == ScreenListener::State::kSuspended) {
    StopMdnsResponder();
    if (!listener_ || listener_->state() == ScreenListener::State::kStopped)
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

void MdnsResponderService::HandleMdnsEvents() {
  // NOTE: In the common case, we will get a single combined packet for
  // PTR/SRV/TXT/A and then no other packets.  If we don't loop here, we would
  // start SRV/TXT queries based on the PTR response, but never check for events
  // again.  This should no longer be a problem when we have correct scheduling
  // of RunTasks.
  bool events_possible = false;
  PendingScreenInfoSet pending_screen_info_set;
  do {
    events_possible = false;
    for (auto& ptr_event : mdns_responder_->TakePtrResponses()) {
      events_possible = HandlePtrEvent(ptr_event, &pending_screen_info_set) ||
                        events_possible;
    }
    for (auto& srv_event : mdns_responder_->TakeSrvResponses()) {
      events_possible = HandleSrvEvent(srv_event, &pending_screen_info_set) ||
                        events_possible;
    }
    for (auto& txt_event : mdns_responder_->TakeTxtResponses()) {
      events_possible = HandleTxtEvent(txt_event, &pending_screen_info_set) ||
                        events_possible;
    }
    for (const auto& a_event : mdns_responder_->TakeAResponses()) {
      events_possible =
          HandleAEvent(a_event, &pending_screen_info_set) || events_possible;
    }
    for (const auto& aaaa_event : mdns_responder_->TakeAaaaResponses()) {
      events_possible = HandleAaaaEvent(aaaa_event, &pending_screen_info_set) ||
                        events_possible;
    }
    if (events_possible)
      mdns_responder_->RunTasks();
  } while (events_possible);

  if (!pending_screen_info_set.empty()) {
    for (const auto& instance_name : pending_screen_info_set) {
      auto service_entry = services_by_name_.find(instance_name);
      auto& service = service_entry->second;

      std::string screen_id = ScreenIdFromServiceInstanceName(instance_name);
      auto screen_info_entry = screen_info_.find(screen_id);
      HostInfo* host = GetHostInfo(service->ptr_socket, service->domain_name);
      if (!host || !service->has_ptr_record || !service->has_srv() ||
          service->txt_info.empty() ||
          !(host->v4_address || host->v6_address)) {
        if (screen_info_entry != screen_info_.end()) {
          const auto& screen_info = screen_info_entry->second;
          listener_->OnScreenRemoved(screen_info);
          screen_info_.erase(screen_info_entry);
        }
        if (!service->has_ptr_record && !service->has_srv())
          services_by_name_.erase(service_entry);
        continue;
      }

      std::string friendly_name;
      for (const auto& line : service->txt_info) {
        // TODO(btolsch): Placeholder until TXT data is spec'd.
        if (line.size() > 3 &&
            std::equal(std::begin("fn="), std::begin("fn=") + 3,
                       line.begin())) {
          friendly_name = line.substr(3);
          break;
        }
      }

      if (screen_info_entry == screen_info_.end()) {
        ScreenInfo screen_info{std::move(screen_id),
                               std::move(friendly_name),
                               GetInterfaceIndexFromSocket(service->ptr_socket),
                               {host->v4_address, service->port},
                               {host->v6_address, service->port}};
        listener_->OnScreenAdded(screen_info);
        screen_info_.emplace(screen_info.screen_id, std::move(screen_info));
      } else {
        auto& screen_info = screen_info_entry->second;
        if (screen_info.Update(std::move(friendly_name),
                               GetInterfaceIndexFromSocket(service->ptr_socket),
                               {host->v4_address, service->port},
                               {host->v6_address, service->port})) {
          listener_->OnScreenChanged(screen_info);
        }
      }
    }
  }
}

void MdnsResponderService::StartListening() {
  if (bound_interfaces_.empty()) {
    mdns_responder_->Init();
    bound_interfaces_ = platform_->RegisterInterfaces({});
    for (auto& interface : bound_interfaces_) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
  }
  mdns::DomainName service_type;
  OSP_CHECK(mdns::DomainName::FromLabels(service_type_.begin(),
                                         service_type_.end(), &service_type));
  for (const auto& interface : bound_interfaces_)
    mdns_responder_->StartPtrQuery(interface.socket, service_type);
}

void MdnsResponderService::StopListening() {
  mdns::DomainName service_type;
  OSP_CHECK(mdns::DomainName::FromLabels(service_type_.begin(),
                                         service_type_.end(), &service_type));
  for (const auto& kv : network_to_domain_to_host_) {
    platform::UdpSocketPtr socket = kv.first;
    const auto& domain_to_host = kv.second;

    for (const auto& kv2 : domain_to_host) {
      const auto& hostname = kv2.first;

      mdns_responder_->StopAQuery(socket, hostname);
      mdns_responder_->StopAaaaQuery(socket, hostname);
    }
  }
  network_to_domain_to_host_.clear();
  for (const auto& service : services_by_name_) {
    platform::UdpSocketPtr socket = service.second->ptr_socket;
    mdns_responder_->StopSrvQuery(socket, service.first);
    mdns_responder_->StopTxtQuery(socket, service.first);
  }
  services_by_name_.clear();
  for (const auto& interface : bound_interfaces_)
    mdns_responder_->StopPtrQuery(interface.socket, service_type);
  RemoveAllScreens();
}

void MdnsResponderService::StartService() {
  if (!bound_interfaces_.empty()) {
    std::vector<MdnsPlatformService::BoundInterface> deregistered_interfaces;
    for (auto it = bound_interfaces_.begin(); it != bound_interfaces_.end();) {
      if (std::find(interface_index_whitelist_.begin(),
                    interface_index_whitelist_.end(),
                    it->interface_info.index) ==
          interface_index_whitelist_.end()) {
        mdns_responder_->DeregisterInterface(it->socket);
        deregistered_interfaces.push_back(*it);
        it = bound_interfaces_.erase(it);
      } else {
        ++it;
      }
    }
    platform_->DeregisterInterfaces(deregistered_interfaces);
  } else {
    mdns_responder_->Init();
    bound_interfaces_ =
        platform_->RegisterInterfaces(interface_index_whitelist_);
    for (auto& interface : bound_interfaces_) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
  }
  mdns_responder_->SetHostLabel(service_hostname_);
  mdns::DomainName domain_name;
  OSP_CHECK(mdns::DomainName::FromLabels(&service_hostname_,
                                         &service_hostname_ + 1, &domain_name))
      << "bad hostname configured: " << service_hostname_;
  OSP_CHECK(domain_name.Append(mdns::DomainName::GetLocalDomain()));
  mdns_responder_->RegisterService(service_instance_name_, service_type_[0],
                                   service_type_[1], domain_name, service_port_,
                                   service_txt_data_);
}

void MdnsResponderService::StopService() {
  mdns_responder_->DeregisterService(service_instance_name_, service_type_[0],
                                     service_type_[1]);
}

void MdnsResponderService::StopMdnsResponder() {
  mdns_responder_->Close();
  platform_->DeregisterInterfaces(bound_interfaces_);
  bound_interfaces_.clear();
  network_to_domain_to_host_.clear();
  services_by_name_.clear();
  RemoveAllScreens();
}

void MdnsResponderService::UpdatePendingScreenInfoSet(
    PendingScreenInfoSet* pending_screen_info_set,
    const mdns::DomainName& domain_name) {
  for (auto& entry : services_by_name_) {
    const auto& instance_name = entry.first;
    const auto& instance = entry.second;
    if (instance->domain_name == domain_name) {
      pending_screen_info_set->emplace(instance_name);
    }
  }
}

void MdnsResponderService::RemoveAllScreens() {
  bool had_screens = !screen_info_.empty();
  screen_info_.clear();
  if (had_screens)
    listener_->OnAllScreensRemoved();
}

bool MdnsResponderService::HandlePtrEvent(
    const mdns::PtrEvent& ptr_event,
    PendingScreenInfoSet* pending_screen_info_set) {
  bool events_possible = false;
  const auto& instance_name = ptr_event.service_instance;
  platform::UdpSocketPtr socket = ptr_event.header.socket;
  auto entry = services_by_name_.find(ptr_event.service_instance);
  switch (ptr_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAddedNoCache:
      break;
    case mdns::QueryEventHeader::Type::kAdded: {
      if (entry != services_by_name_.end())
        break;
      mdns_responder_->StartSrvQuery(socket, instance_name);
      mdns_responder_->StartTxtQuery(socket, instance_name);
      events_possible = true;

      auto new_instance = MakeUnique<ServiceInstance>();
      new_instance->ptr_socket = socket;
      new_instance->has_ptr_record = true;
      pending_screen_info_set->emplace(instance_name);
      services_by_name_.emplace(std::move(instance_name),
                                std::move(new_instance));
    } break;
    case mdns::QueryEventHeader::Type::kRemoved:
      if (entry == services_by_name_.end())
        break;
      if (entry->second->ptr_socket != socket)
        break;
      entry->second->has_ptr_record = false;
      if (!entry->second->has_srv()) {
        mdns_responder_->StopSrvQuery(socket, instance_name);
        mdns_responder_->StopTxtQuery(socket, instance_name);
      }
      pending_screen_info_set->emplace(std::move(instance_name));
      break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleSrvEvent(
    const mdns::SrvEvent& srv_event,
    PendingScreenInfoSet* pending_screen_info_set) {
  bool events_possible = false;
  auto& domain_name = srv_event.domain_name;
  const auto& instance_name = srv_event.service_instance;
  platform::UdpSocketPtr socket = srv_event.header.socket;
  auto entry = services_by_name_.find(srv_event.service_instance);
  if (entry == services_by_name_.end())
    return events_possible;
  switch (srv_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAddedNoCache:
      break;
    case mdns::QueryEventHeader::Type::kAdded: {
      auto& domain_to_host =
          network_to_domain_to_host_[entry->second->ptr_socket];
      auto host_entry = domain_to_host.find(domain_name);
      if (host_entry == domain_to_host.end()) {
        mdns_responder_->StartAQuery(socket, domain_name);
        mdns_responder_->StartAaaaQuery(socket, domain_name);
        events_possible = true;
        auto result = domain_to_host.emplace(domain_name, HostInfo{});
        host_entry = result.first;
      }
      auto& dependent_services = host_entry->second.services;
      if (std::find_if(dependent_services.begin(), dependent_services.end(),
                       [entry](ServiceInstance* instance) {
                         return instance == entry->second.get();
                       }) == dependent_services.end()) {
        dependent_services.push_back(entry->second.get());
      }
      entry->second->domain_name = std::move(domain_name);
      entry->second->port = srv_event.port;
      pending_screen_info_set->emplace(std::move(instance_name));
    } break;
    case mdns::QueryEventHeader::Type::kRemoved: {
      auto domain_to_host_entry = network_to_domain_to_host_.find(socket);
      if (domain_to_host_entry == network_to_domain_to_host_.end())
        break;
      auto& domain_to_host = domain_to_host_entry->second;
      auto host_entry = domain_to_host.find(domain_name);
      if (host_entry != domain_to_host.end()) {
        auto& dependent_services = host_entry->second.services;
        dependent_services.erase(
            std::remove_if(dependent_services.begin(), dependent_services.end(),
                           [entry](ServiceInstance* instance) {
                             return instance == entry->second.get();
                           }),
            dependent_services.end());
        if (dependent_services.empty()) {
          mdns_responder_->StopAQuery(socket, host_entry->first);
          mdns_responder_->StopAaaaQuery(socket, host_entry->first);
          domain_to_host.erase(host_entry);
          if (domain_to_host.empty())
            network_to_domain_to_host_.erase(domain_to_host_entry);
        }
      }
      entry->second->domain_name = mdns::DomainName();
      entry->second->port = 0;
      if (!entry->second->has_ptr_record) {
        mdns_responder_->StopSrvQuery(socket, instance_name);
        mdns_responder_->StopTxtQuery(socket, instance_name);
      }
      pending_screen_info_set->emplace(std::move(instance_name));
    } break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleTxtEvent(
    const mdns::TxtEvent& txt_event,
    PendingScreenInfoSet* pending_screen_info_set) {
  bool events_possible = false;
  const auto& instance_name = txt_event.service_instance;
  auto entry = services_by_name_.find(instance_name);
  if (entry == services_by_name_.end())
    return events_possible;
  switch (txt_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAddedNoCache:
      break;
    case mdns::QueryEventHeader::Type::kAdded:
      pending_screen_info_set->emplace(instance_name);
      if (entry == services_by_name_.end()) {
        auto result = services_by_name_.emplace(std::move(instance_name),
                                                MakeUnique<ServiceInstance>());
        entry = result.first;
      }
      entry->second->txt_info = std::move(txt_event.txt_info);
      break;
    case mdns::QueryEventHeader::Type::kRemoved:
      entry->second->txt_info.clear();
      pending_screen_info_set->emplace(std::move(instance_name));
      break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleAddressEvent(
    platform::UdpSocketPtr socket,
    mdns::QueryEventHeader::Type response_type,
    const mdns::DomainName& domain_name,
    bool a_event,
    const IPAddress& address,
    PendingScreenInfoSet* pending_screen_info_set) {
  bool events_possible = false;
  switch (response_type) {
    case mdns::QueryEventHeader::Type::kAddedNoCache:
      break;
    case mdns::QueryEventHeader::Type::kAdded: {
      HostInfo* host = AddOrGetHostInfo(socket, domain_name);
      if (a_event)
        host->v4_address = address;
      else
        host->v6_address = address;
      UpdatePendingScreenInfoSet(pending_screen_info_set, domain_name);
    } break;
    case mdns::QueryEventHeader::Type::kRemoved: {
      HostInfo* host = GetHostInfo(socket, domain_name);

      if (a_event)
        host->v4_address = IPAddress();
      else
        host->v6_address = IPAddress();

      if (host->v4_address || host->v6_address)
        UpdatePendingScreenInfoSet(pending_screen_info_set, domain_name);
    } break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleAEvent(
    const mdns::AEvent& a_event,
    PendingScreenInfoSet* pending_screen_info_set) {
  return HandleAddressEvent(a_event.header.socket, a_event.header.response_type,
                            a_event.domain_name, true, a_event.address,
                            pending_screen_info_set);
}

bool MdnsResponderService::HandleAaaaEvent(
    const mdns::AaaaEvent& aaaa_event,
    PendingScreenInfoSet* pending_screen_info_set) {
  return HandleAddressEvent(aaaa_event.header.socket,
                            aaaa_event.header.response_type,
                            aaaa_event.domain_name, false, aaaa_event.address,
                            pending_screen_info_set);
}

MdnsResponderService::HostInfo* MdnsResponderService::AddOrGetHostInfo(
    platform::UdpSocketPtr socket,
    const mdns::DomainName& domain_name) {
  return &network_to_domain_to_host_[socket][domain_name];
}

MdnsResponderService::HostInfo* MdnsResponderService::GetHostInfo(
    platform::UdpSocketPtr socket,
    const mdns::DomainName& domain_name) {
  auto kv = network_to_domain_to_host_.find(socket);
  if (kv == network_to_domain_to_host_.end())
    return nullptr;

  auto& domain_to_host = kv->second;
  auto kv2 = domain_to_host.find(domain_name);
  if (kv2 == domain_to_host.end())
    return nullptr;
  return &kv2->second;
}

platform::InterfaceIndex MdnsResponderService::GetInterfaceIndexFromSocket(
    platform::UdpSocketPtr socket) const {
  auto it = std::find_if(
      bound_interfaces_.begin(), bound_interfaces_.end(),
      [socket](const MdnsPlatformService::BoundInterface& interface) {
        return interface.socket == socket;
      });
  if (it == bound_interfaces_.end())
    return platform::kInvalidInterfaceIndex;
  return it->interface_info.index;
}

}  // namespace openscreen
