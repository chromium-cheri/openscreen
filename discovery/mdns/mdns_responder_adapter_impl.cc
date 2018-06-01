// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder_adapter_impl.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace mdns {
namespace {

const DomainName kLocalDomain{{5, 'l', 'o', 'c', 'a', 'l', 0}};

void AssignMdnsPort(mDNSIPPort* mdns_port, uint16_t port) {
  mdns_port->b[0] = (port >> 8) & 0xff;
  mdns_port->b[1] = port & 0xff;
}

uint16_t GetNetworkOrderPort(const mDNSOpaque16& port) {
  return port.b[0] << 8 | port.b[1];
}

bool EndsWithLocalDomain(const DomainName& domain) {
  if (domain.domain_name().size() < kLocalDomain.domain_name().size()) {
    return false;
  }
  return std::equal(
      kLocalDomain.domain_name().begin(), kLocalDomain.domain_name().end(),
      domain.domain_name().end() - kLocalDomain.domain_name().size());
}

std::string MakeTxtData(const std::vector<std::string>& lines) {
  std::string txt;
  txt.reserve(256);
  for (const auto& line : lines) {
    txt.push_back(line.size());
    txt += line;
  }
  return txt;
}

MdnsResponderError MapMdnsError(int err) {
  switch (err) {
    case mStatus_NoError:
      return MdnsResponderError::kNoError;
    case mStatus_UnsupportedErr:
      return MdnsResponderError::kUnsupportedError;
    case mStatus_UnknownErr:
      return MdnsResponderError::kUnknownError;
    default:
      break;
  }
  DLOG_WARN << "unmapped mDNSResponder error: " << err;
  return MdnsResponderError::kUnknownError;
}

std::vector<std::string> ParseTxtResponse(const uint8_t (&data)[256],
                                          uint16_t length) {
  DCHECK(length <= 256);
  if (length == 0) {
    return {};
  }
  std::vector<std::string> lines;
  int total_pos = 0;
  while (total_pos < length) {
    uint8_t line_length = data[total_pos];
    lines.emplace_back(&data[total_pos + 1],
                       &data[total_pos + line_length + 1]);
    total_pos += line_length + 1;
  }
  return lines;
}

void MdnsStatusCallback(mDNS* mdns, mStatus result) {
  LOG_INFO << "status good? " << (result == mStatus_NoError);
}

}  // namespace

MdnsResponderAdapterImpl::MdnsResponderAdapterImpl() = default;
MdnsResponderAdapterImpl::~MdnsResponderAdapterImpl() = default;

bool MdnsResponderAdapterImpl::Init() {
  VLOG(2) << __func__;
  const auto err =
      mDNS_Init(&mdns_, &platform_storage_, rr_cache_, kRrCacheSize,
                mDNS_Init_DontAdvertiseLocalAddresses, &MdnsStatusCallback,
                mDNS_Init_NoInitCallbackContext);
  LOG_INFO << "init good? " << (err == mStatus_NoError);
  return err == mStatus_NoError;
}

void MdnsResponderAdapterImpl::Close() {
  mDNS_StartExit(&mdns_);
  // Let all services send goodbyes.
  while (!service_records_.empty()) {
    Execute();
  }
  mDNS_FinalExit(&mdns_);
}

void MdnsResponderAdapterImpl::SetHostLabel(const std::string& host_label) {
  MakeDomainLabelFromLiteralString(&mdns_.hostlabel, host_label.c_str());
  mDNS_SetFQDN(&mdns_);
}

bool MdnsResponderAdapterImpl::RegisterInterface(
    const platform::InterfaceInfo& interface_info,
    const platform::IPv4Subnet& interface_address,
    platform::UdpSocketIPv4Ptr socket,
    bool advertise) {
  const auto info_entry = v4_responder_interface_info_.find(socket);
  if (info_entry != v4_responder_interface_info_.end()) {
    if (!advertise || !!info_entry->second.Advertise == advertise) {
      return true;
    }
    DeregisterInterface(socket);
  }
  auto& info = v4_responder_interface_info_[socket];
  std::memset(&info, 0, sizeof(NetworkInterfaceInfo));
  info.InterfaceID = reinterpret_cast<decltype(info.InterfaceID)>(socket);
  info.Advertise = advertise;
  info.ip.type = mDNSAddrType_IPv4;
  std::memcpy(info.ip.ip.v4.b, interface_address.address.bytes.data(),
              interface_address.address.bytes.size());
  info.mask.type = mDNSAddrType_IPv4;
  // XXX: little-endian-specific
  info.mask.ip.v4.NotAnInteger =
      0xffffffffu >> (32 - interface_address.prefix_length);
  std::memcpy(info.MAC.b, interface_info.hardware_address,
              sizeof(interface_info.hardware_address));
  info.McastTxRx = 1;
  platform_storage_.v4_sockets.push_back(socket);
  return mDNS_RegisterInterface(&mdns_, &info, mDNSfalse) == mStatus_NoError;
}

bool MdnsResponderAdapterImpl::RegisterInterface(
    const platform::InterfaceInfo& interface_info,
    const platform::IPv6Subnet& interface_address,
    platform::UdpSocketIPv6Ptr socket,
    bool advertise) {
  UNIMPLEMENTED();
  return false;
}

bool MdnsResponderAdapterImpl::DeregisterInterface(
    platform::UdpSocketIPv4Ptr socket) {
  const auto info_entry = v4_responder_interface_info_.find(socket);
  if (info_entry == v4_responder_interface_info_.end()) {
    return false;
  }
  const auto it = std::find(platform_storage_.v4_sockets.begin(),
                            platform_storage_.v4_sockets.end(), socket);
  DCHECK(it != platform_storage_.v4_sockets.end());
  platform_storage_.v4_sockets.erase(it);
  mDNS_DeregisterInterface(&mdns_, &info_entry->second, mDNSfalse);
  v4_responder_interface_info_.erase(info_entry);
  return true;
}

bool MdnsResponderAdapterImpl::DeregisterInterface(
    platform::UdpSocketIPv6Ptr socket) {
  // get off my lawn
  UNIMPLEMENTED();
  return false;
}

void MdnsResponderAdapterImpl::OnDataReceived(
    const IPv4Endpoint& source,
    const IPv4Endpoint& original_destination,
    const uint8_t* data,
    size_t length,
    platform::UdpSocketIPv4Ptr receiving_socket) {
  VLOG(2) << __func__;
  mDNSAddr src;
  src.type = mDNSAddrType_IPv4;
  std::memcpy(src.ip.v4.b, source.address.bytes.data(), sizeof(src.ip.v4.b));
  mDNSIPPort srcport;
  AssignMdnsPort(&srcport, source.port);

  mDNSAddr dst;
  dst.type = mDNSAddrType_IPv4;
  std::memcpy(dst.ip.v4.b, original_destination.address.bytes.data(),
              sizeof(dst.ip.v4.b));
  mDNSIPPort dstport;
  AssignMdnsPort(&dstport, original_destination.port);

  mDNSCoreReceive(&mdns_, const_cast<uint8_t*>(data), data + length, &src,
                  srcport, &dst, dstport,
                  reinterpret_cast<mDNSInterfaceID>(receiving_socket));
}
void MdnsResponderAdapterImpl::OnDataReceived(
    const IPv6Endpoint& source,
    const IPv6Endpoint& original_destination,
    const uint8_t* data,
    size_t length,
    platform::UdpSocketIPv6Ptr receiving_socket) {
  VLOG(2) << __func__;
  mDNSAddr src;
  src.type = mDNSAddrType_IPv6;
  std::memcpy(src.ip.v6.b, source.address.bytes.data(), sizeof(src.ip.v6.b));
  mDNSIPPort srcport;
  AssignMdnsPort(&srcport, source.port);

  mDNSAddr dst;
  dst.type = mDNSAddrType_IPv6;
  std::memcpy(dst.ip.v6.b, original_destination.address.bytes.data(),
              sizeof(dst.ip.v6.b));
  mDNSIPPort dstport;
  AssignMdnsPort(&dstport, original_destination.port);

  mDNSCoreReceive(&mdns_, const_cast<uint8_t*>(data), data + length, &src,
                  srcport, &dst, dstport,
                  reinterpret_cast<mDNSInterfaceID>(receiving_socket));
}

int MdnsResponderAdapterImpl::Execute() {
  VLOG(2) << __func__;
  const auto t = mDNS_Execute(&mdns_);
  const auto now = mDNSPlatformRawTime();
  const auto next = t - now;
  VLOG(2) << "next execute: " << t << ", " << now << ", " << next;
  return next;
}

std::vector<AResponseEvent> MdnsResponderAdapterImpl::TakeAResponses() {
  return std::move(a_responses_);
}

std::vector<AaaaResponseEvent> MdnsResponderAdapterImpl::TakeAaaaResponses() {
  return std::move(aaaa_responses_);
}

std::vector<PtrResponseEvent> MdnsResponderAdapterImpl::TakePtrResponses() {
  return std::move(ptr_responses_);
}

std::vector<SrvResponseEvent> MdnsResponderAdapterImpl::TakeSrvResponses() {
  return std::move(srv_responses_);
}

std::vector<TxtResponseEvent> MdnsResponderAdapterImpl::TakeTxtResponses() {
  return std::move(txt_responses_);
}

MdnsResponderError MdnsResponderAdapterImpl::StartAQuery(
    const DomainName& domain_name) {
  VLOG(1) << __func__;
  DCHECK(EndsWithLocalDomain(domain_name));
  if (a_questions_.find(domain_name) != a_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  auto& question = a_questions_[domain_name];
  std::copy(domain_name.domain_name().begin(), domain_name.domain_name().end(),
            question.qname.c);

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  question.qtype = kDNSType_A;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::AQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StartAaaaQuery(
    const DomainName& domain_name) {
  VLOG(1) << __func__;
  DCHECK(EndsWithLocalDomain(domain_name));
  if (aaaa_questions_.find(domain_name) != aaaa_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  auto& question = aaaa_questions_[domain_name];
  std::copy(domain_name.domain_name().begin(), domain_name.domain_name().end(),
            question.qname.c);

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  question.qtype = kDNSType_AAAA;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::AaaaQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StartPtrQuery(
    const DomainName& service_type) {
  VLOG(1) << __func__;
  if (ptr_questions_.find(service_type) != ptr_questions_.end()) {
    return MdnsResponderError::kNoError;
  }

  auto& question = ptr_questions_[service_type];

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  if (EndsWithLocalDomain(service_type)) {
    std::copy(service_type.domain_name().begin(),
              service_type.domain_name().end(), question.qname.c);
  } else {
    if (!DomainName::Append(service_type, kLocalDomain, question.qname.c)) {
      return MdnsResponderError::kDomainOverflowError;
    }
  }
  question.qtype = kDNSType_PTR;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::PtrQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StartSrvQuery(
    const DomainName& service_instance) {
  VLOG(1) << __func__;
  DCHECK(EndsWithLocalDomain(service_instance));
  if (srv_questions_.find(service_instance) != srv_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  auto& question = srv_questions_[service_instance];

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  std::copy(service_instance.domain_name().begin(),
            service_instance.domain_name().end(), question.qname.c);
  question.qtype = kDNSType_SRV;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::SrvQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StartTxtQuery(
    const DomainName& service_instance) {
  VLOG(1) << __func__;
  DCHECK(EndsWithLocalDomain(service_instance));
  if (txt_questions_.find(service_instance) != txt_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  auto& question = txt_questions_[service_instance];

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  std::copy(service_instance.domain_name().begin(),
            service_instance.domain_name().end(), question.qname.c);
  question.qtype = kDNSType_TXT;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::TxtQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StopAQuery(
    const DomainName& domain_name) {
  VLOG(1) << __func__;
  auto entry = a_questions_.find(domain_name);
  if (entry == a_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  a_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StopAaaaQuery(
    const DomainName& domain_name) {
  VLOG(1) << __func__;
  auto entry = aaaa_questions_.find(domain_name);
  if (entry == aaaa_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  aaaa_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StopPtrQuery(
    const DomainName& service_type) {
  VLOG(1) << __func__;
  auto entry = ptr_questions_.find(service_type);
  if (entry == ptr_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  ptr_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StopSrvQuery(
    const DomainName& service_instance) {
  VLOG(1) << __func__;
  auto entry = srv_questions_.find(service_instance);
  if (entry == srv_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  srv_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::StopTxtQuery(
    const DomainName& service_instance) {
  VLOG(1) << __func__;
  auto entry = txt_questions_.find(service_instance);
  if (entry == txt_questions_.end()) {
    return MdnsResponderError::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  txt_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderError MdnsResponderAdapterImpl::RegisterService(
    const std::string& service_name,
    const DomainName& service_type,
    const DomainName& target_host,
    uint16_t target_port,
    const std::vector<std::string>& lines) {
  DCHECK(!EndsWithLocalDomain(service_type));
  service_records_.push_back(MakeUnique<ServiceRecordSet>());
  auto* service_record = service_records_.back().get();
  domainlabel name;
  domainname type;
  domainname domain;
  domainname host;
  mDNSIPPort port;

  MakeDomainLabelFromLiteralString(&name, service_name.c_str());
  std::copy(service_type.domain_name().begin(),
            service_type.domain_name().end(), type.c);
  std::copy(kLocalDomain.domain_name().begin(),
            kLocalDomain.domain_name().end(), domain.c);
  std::copy(target_host.domain_name().begin(), target_host.domain_name().end(),
            host.c);
  AssignMdnsPort(&port, target_port);
  auto txt = MakeTxtData(lines);
  if (txt.size() > 256) {
    // Not handling oversized TXT records.
    return MdnsResponderError::kUnsupportedError;
  }

  auto result = mDNS_RegisterService(
      &mdns_, service_record, &name, &type, &domain, &host, port,
      reinterpret_cast<const uint8_t*>(txt.data()), txt.size(), nullptr, 0,
      mDNSInterface_Any, &MdnsResponderAdapterImpl::ServiceCallback, this, 0);
  return MapMdnsError(result);
}

// static
void MdnsResponderAdapterImpl::AQueryCallback(mDNS* m,
                                              DNSQuestion* question,
                                              const ResourceRecord* answer,
                                              QC_result added) {
  VLOG(1) << __func__;
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_A);
  DomainName domain(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  IPv4Address address(answer->rdata->u.ipv4.b);

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  const auto event_type =
      added == QC_add
          ? ResponseType::kAdd
          : added == QC_rmv ? ResponseType::kRemove : ResponseType::kAddNoCache;
  adapter->a_responses_.emplace_back(
      QueryResponseEventHeader{
          event_type,
          reinterpret_cast<platform::UdpSocketIPv4Ptr>(answer->InterfaceID)},
      std::move(domain), address);
}

// static
void MdnsResponderAdapterImpl::AaaaQueryCallback(mDNS* m,
                                                 DNSQuestion* question,
                                                 const ResourceRecord* answer,
                                                 QC_result added) {
  VLOG(1) << __func__;
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_A);
  DomainName domain(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  IPv6Address address(answer->rdata->u.ipv6.b);

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  const auto event_type =
      added == QC_add
          ? ResponseType::kAdd
          : added == QC_rmv ? ResponseType::kRemove : ResponseType::kAddNoCache;
  adapter->aaaa_responses_.emplace_back(
      QueryResponseEventHeader{
          event_type,
          reinterpret_cast<platform::UdpSocketIPv4Ptr>(answer->InterfaceID)},
      std::move(domain), address);
}

// static
void MdnsResponderAdapterImpl::PtrQueryCallback(mDNS* m,
                                                DNSQuestion* question,
                                                const ResourceRecord* answer,
                                                QC_result added) {
  VLOG(1) << __func__;
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_PTR);
  DomainName result(std::vector<uint8_t>(
      answer->rdata->u.name.c,
      answer->rdata->u.name.c + DomainNameLength(&answer->rdata->u.name)));

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  const auto event_type =
      added == QC_add
          ? ResponseType::kAdd
          : added == QC_rmv ? ResponseType::kRemove : ResponseType::kAddNoCache;
  adapter->ptr_responses_.emplace_back(
      QueryResponseEventHeader{
          event_type,
          reinterpret_cast<platform::UdpSocketIPv4Ptr>(answer->InterfaceID)},
      std::move(result));
}

// static
void MdnsResponderAdapterImpl::SrvQueryCallback(mDNS* m,
                                                DNSQuestion* question,
                                                const ResourceRecord* answer,
                                                QC_result added) {
  VLOG(1) << __func__;
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_SRV);
  DomainName service(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  DomainName result(
      std::vector<uint8_t>(answer->rdata->u.srv.target.c,
                           answer->rdata->u.srv.target.c +
                               DomainNameLength(&answer->rdata->u.srv.target)));

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  const auto event_type =
      added == QC_add
          ? ResponseType::kAdd
          : added == QC_rmv ? ResponseType::kRemove : ResponseType::kAddNoCache;
  adapter->srv_responses_.emplace_back(
      QueryResponseEventHeader{
          event_type,
          reinterpret_cast<platform::UdpSocketIPv4Ptr>(answer->InterfaceID)},
      std::move(service), std::move(result),
      GetNetworkOrderPort(answer->rdata->u.srv.port));
}

// static
void MdnsResponderAdapterImpl::TxtQueryCallback(mDNS* m,
                                                DNSQuestion* question,
                                                const ResourceRecord* answer,
                                                QC_result added) {
  VLOG(1) << __func__;
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_TXT);
  DomainName service(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  auto lines = ParseTxtResponse(answer->rdata->u.txt.c, answer->rdlength);

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  const auto event_type =
      added == QC_add
          ? ResponseType::kAdd
          : added == QC_rmv ? ResponseType::kRemove : ResponseType::kAddNoCache;
  adapter->txt_responses_.emplace_back(
      QueryResponseEventHeader{
          event_type,
          reinterpret_cast<platform::UdpSocketIPv4Ptr>(answer->InterfaceID)},
      std::move(service), std::move(lines));
}

// static
void MdnsResponderAdapterImpl::ServiceCallback(mDNS* m,
                                               ServiceRecordSet* service_record,
                                               mStatus result) {
  VLOG(1) << __func__;
  if (result == mStatus_MemFree) {
    DLOG_INFO << "free service record";
    auto& service_records = reinterpret_cast<MdnsResponderAdapterImpl*>(
                                service_record->ServiceContext)
                                ->service_records_;
    service_records.erase(
        std::remove_if(
            service_records.begin(), service_records.end(),
            [service_record](const std::unique_ptr<ServiceRecordSet>& sr) {
              return sr.get() == service_record;
            }),
        service_records.end());
  }
}

bool MdnsResponderAdapterImpl::DomainNameComparator::operator()(
    const DomainName& a,
    const DomainName& b) const {
  return a.domain_name() < b.domain_name();
}

}  // namespace mdns
}  // namespace openscreen
