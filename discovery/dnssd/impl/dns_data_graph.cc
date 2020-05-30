// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/dns_data_graph.h"

#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/instance_key.h"

namespace openscreen {
namespace discovery {
namespace {

ErrorOr<DnsSdInstanceEndpoint> CreateEndpoint(
    const DomainName& domain,
    const absl::optional<ARecordRdata>& a,
    const absl::optional<AAAARecordRdata>& aaaa,
    const SrvRecordRdata& srv,
    const TxtRecordRdata& txt,
    NetworkInterfaceIndex network_interface) {
  // Create the user-visible TXT record representation.
  ErrorOr<DnsSdTxtRecord> txt_or_error = CreateFromDnsTxt(txt);
  if (txt_or_error.is_error()) {
    return txt_or_error.error();
  }

  InstanceKey instance_id(domain);
  if (a.has_value() && aaaa.has_value()) {
    return DnsSdInstanceEndpoint(
        instance_id.instance_id(), instance_id.service_id(),
        instance_id.domain_id(), std::move(txt_or_error.value()),
        {a.value().ipv4_address(), srv.port()},
        {aaaa.value().ipv6_address(), srv.port()}, network_interface);
  } else {
    IPEndpoint ep = a.has_value()
                        ? IPEndpoint{a.value().ipv4_address(), srv.port()}
                        : IPEndpoint{aaaa.value().ipv6_address(), srv.port()};
    return DnsSdInstanceEndpoint(
        instance_id.instance_id(), instance_id.service_id(),
        instance_id.domain_id(), std::move(txt_or_error.value()), std::move(ep),
        network_interface);
  }
}

}  // namespace

DnsDataGraph::Node::Node(DomainName name, DnsDataGraph* graph)
    : name_(std::move(name)), graph_(graph) {
  OSP_DCHECK(graph_);

  graph_->on_node_creation_(name_);
}

DnsDataGraph::Node::~Node() {
  // A node should only be deleted when it has no parents. The only case where
  // a deletion can occur when parents are still extant is during destruction of
  // the holding graph. In that case, the state of the graph no longer matters
  // and all nodes will be deleted, so no need to consider the child pointers.
  if (!graph_->is_dtor_running_) {
    OSP_DCHECK(parents_.empty());

    // Erase all childrens' parent pointers to this node.
    for (Node* child : children_) {
      RemoveChild(child);
    }

    OSP_DCHECK(graph_->on_node_deletion_);
    graph_->on_node_deletion_(name_);
  }
}

Error DnsDataGraph::Node::ApplyDataRecordChange(MdnsRecord record,
                                                RecordChangedEvent event) {
  DomainName child_name;
  std::vector<MdnsRecord>::iterator it;

  // PTR records are allowed to have duplicates. Other record types are not.
  if (record.dns_type() == DnsType::kPTR) {
    child_name = absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
    it = std::find(records_.begin(), records_.end(), record);
  } else {
    if (record.dns_type() == DnsType::kSRV) {
      child_name = absl::get<SrvRecordRdata>(record.rdata()).target();
    }
    it = FindRecord(record.dns_type());
  }

  // Apply the change.
  switch (event) {
    case RecordChangedEvent::kCreated:
      if (it != records_.end()) {
        return Error::Code::kItemAlreadyExists;
      }
      records_.push_back(std::move(record));
      break;

    case RecordChangedEvent::kUpdated:
      if (it == records_.end()) {
        return Error::Code::kItemNotFound;
      }
      *it = std::move(record);
      break;

    case RecordChangedEvent::kExpired:
      if (it == records_.end()) {
        return Error::Code::kItemNotFound;
      }
      records_.erase(it);
      break;
  }

  // Apply any required edge changes to the graph.
  if (!child_name.empty()) {
    if (event == RecordChangedEvent::kCreated) {
      auto pair = graph_->nodes_.emplace(child_name, std::unique_ptr<Node>());
      if (pair.second) {
        auto new_node = std::make_unique<Node>(std::move(child_name), graph_);
        pair.first->second.swap(new_node);
      }

      AddChild(pair.first->second.get());
    } else if (event == RecordChangedEvent::kExpired) {
      auto it = graph_->nodes_.find(child_name);
      OSP_DCHECK(it != graph_->nodes_.end());
      RemoveChild(it->second.get());
    }
  }

  return Error::None();
}

void DnsDataGraph::Node::AddChild(Node* child) {
  OSP_DCHECK(child);
  children_.push_back(child);
  child->parents_.push_back(this);
}

void DnsDataGraph::Node::RemoveChild(Node* child) {
  OSP_DCHECK(child);

  auto it = std::find(children_.begin(), children_.end(), child);
  OSP_DCHECK(it != children_.end());
  children_.erase(it);

  it = std::find(child->parents_.begin(), child->parents_.end(), this);
  OSP_DCHECK(it != child->parents_.end());
  child->parents_.erase(it);

  // If the node has been orphaned, remove it.
  if (child->parents_.empty() && child != this) {
    // NOTE: These must be done as separate steps to avoid invalidation of any
    // iterators.
    DomainName child_name = child->name();
    auto it = graph_->nodes_.find(child_name);
    OSP_DCHECK(it != graph_->nodes_.end());
    it->second.reset();

    graph_->nodes_.erase(child_name);
  }
}

std::vector<MdnsRecord>::iterator DnsDataGraph::Node::FindRecord(DnsType type) {
  return std::find_if(
      records_.begin(), records_.end(),
      [type](const MdnsRecord& record) { return record.dns_type() == type; });
}

DnsDataGraph::DnsDataGraph(NetworkInterfaceIndex network_interface)
    : network_interface_(network_interface) {}

DnsDataGraph::~DnsDataGraph() {
  is_dtor_running_ = true;
}

void DnsDataGraph::StartTracking(const DomainName& domain,
                                 DomainChangeCallback on_start_tracking) {
  OSP_DCHECK(!on_node_creation_);
  OSP_DCHECK(!on_node_deletion_);
  on_node_creation_ = std::move(on_start_tracking);

  auto pair = nodes_.emplace(domain, std::make_unique<Node>(domain, this));
  OSP_DCHECK(pair.second);

  on_node_creation_ = nullptr;
}

void DnsDataGraph::StopTracking(const DomainName& domain,
                                DomainChangeCallback on_stop_tracking) {
  OSP_DCHECK(!on_node_creation_);
  OSP_DCHECK(!on_node_deletion_);
  on_node_deletion_ = std::move(on_stop_tracking);

  auto it = nodes_.find(domain);
  OSP_CHECK(it != nodes_.end());
  OSP_DCHECK(it->second->parents().empty());
  it->second.reset();
  nodes_.erase(domain);

  on_node_deletion_ = nullptr;
}

Error DnsDataGraph::ApplyDataRecordChange(
    MdnsRecord record,
    RecordChangedEvent event,
    DomainChangeCallback on_start_tracking,
    DomainChangeCallback on_stop_tracking) {
  auto it = nodes_.find(record.name());
  if (it == nodes_.end()) {
    return Error::Code::kOperationCancelled;
  }

  // Update the callbacks so that any changes from the change application call
  // the correct callback.
  OSP_DCHECK(!on_node_creation_);
  OSP_DCHECK(!on_node_deletion_);
  on_node_creation_ = std::move(on_start_tracking);
  on_node_deletion_ = std::move(on_stop_tracking);

  auto result = it->second->ApplyDataRecordChange(std::move(record), event);

  on_node_creation_ = nullptr;
  on_node_deletion_ = nullptr;

  return result;
}

ErrorOr<std::vector<DnsSdInstanceEndpoint>> DnsDataGraph::CreateEndpoints(
    DomainGroup domain_group,
    const DomainName& name) {
  auto it = nodes_.find(name);
  if (it == nodes_.end()) {
    return std::vector<DnsSdInstanceEndpoint>{};
  }
  Node* target_node = it->second.get();

  // NOTE: One of these will contain one or less elements, so iterating over
  // them both will be fast.
  std::vector<Node*> srv_and_txt_record_nodes;
  std::vector<Node*> address_record_nodes;

  switch (domain_group) {
    case DomainGroup::kAddress: {
      absl::optional<ARecordRdata> a =
          target_node->GetRdata<ARecordRdata>(DnsType::kA);
      absl::optional<AAAARecordRdata> aaaa =
          target_node->GetRdata<AAAARecordRdata>(DnsType::kAAAA);
      if (!a.has_value() && !aaaa.has_value()) {
        return std::vector<DnsSdInstanceEndpoint>{};
      }

      address_record_nodes.push_back(target_node);
      srv_and_txt_record_nodes = target_node->parents();
      break;
    }

    case DomainGroup::kSrvAndTxt: {
      absl::optional<SrvRecordRdata> srv =
          target_node->GetRdata<SrvRecordRdata>(DnsType::kSRV);
      absl::optional<TxtRecordRdata> txt =
          target_node->GetRdata<TxtRecordRdata>(DnsType::kTXT);

      if (!srv.has_value() || !txt.has_value()) {
        return std::vector<DnsSdInstanceEndpoint>{};
      }

      srv_and_txt_record_nodes.push_back(target_node);
      address_record_nodes = target_node->children();
      break;
    }

    case DomainGroup::kPtr: {
      std::vector<DnsSdInstanceEndpoint> endpoints;
      for (Node* child : target_node->children()) {
        ErrorOr<std::vector<DnsSdInstanceEndpoint>> child_endpoints =
            CreateEndpoints(DomainGroup::kSrvAndTxt, child->name());
        if (child_endpoints.is_error()) {
          return child_endpoints.error();
        }
        endpoints.insert(endpoints.end(), child_endpoints.value().begin(),
                         child_endpoints.value().end());
      }
      return endpoints;
    }

    default:
      return Error::Code::kParameterInvalid;
  }

  // Iterate across all node pairs and create all possible DnsSdInstanceEndpoint
  // objects.
  std::vector<DnsSdInstanceEndpoint> endpoints;
  for (Node* srv_and_txt : srv_and_txt_record_nodes) {
    for (Node* address : address_record_nodes) {
      // Get all requisite data to create a DnsSdInstanceEndpoint.
      absl::optional<ARecordRdata> a =
          address->GetRdata<ARecordRdata>(DnsType::kA);
      absl::optional<AAAARecordRdata> aaaa =
          address->GetRdata<AAAARecordRdata>(DnsType::kAAAA);
      absl::optional<SrvRecordRdata> srv =
          srv_and_txt->GetRdata<SrvRecordRdata>(DnsType::kSRV);
      absl::optional<TxtRecordRdata> txt =
          srv_and_txt->GetRdata<TxtRecordRdata>(DnsType::kTXT);

      // Validate the data.
      if (!srv.has_value() || srv.value().target() != address->name()) {
        continue;
      } else if (!txt.has_value()) {
        continue;
      } else if (!a.has_value() && !aaaa.has_value()) {
        continue;
      }

      ErrorOr<DnsSdInstanceEndpoint> endpoint =
          CreateEndpoint(srv_and_txt->name(), a, aaaa, srv.value(), txt.value(),
                         network_interface_);
      if (endpoint.is_value()) {
        endpoints.push_back(std::move(endpoint.value()));
      } else {
        return endpoint.error();
      }
    }
  }

  return endpoints;
}

DnsDataGraph::DomainGroup GetDomainGroup(DnsType type) {
  switch (type) {
    case DnsType::kA:
    case DnsType::kAAAA:
      return DnsDataGraph::DomainGroup::kAddress;
    case DnsType::kSRV:
    case DnsType::kTXT:
      return DnsDataGraph::DomainGroup::kSrvAndTxt;
    case DnsType::kPTR:
      return DnsDataGraph::DomainGroup::kPtr;
    default:
      OSP_NOTREACHED();
      return DnsDataGraph::DomainGroup::kNone;
  }
}

bool DnsDataGraph::IsTracked(const DomainName& domain) const {
  return nodes_.find(domain) != nodes_.end();
}

DnsDataGraph::DomainGroup GetDomainGroup(const MdnsRecord record) {
  return GetDomainGroup(record.dns_type());
}

}  // namespace discovery
}  // namespace openscreen
