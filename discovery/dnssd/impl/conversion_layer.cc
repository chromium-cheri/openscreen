// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/public/instance_record.h"

namespace openscreen {
namespace discovery {
namespace {

inline void AddServiceInfoToLabels(const std::string& service,
                                   const std::string& domain,
                                   std::vector<absl::string_view>* labels) {
  std::vector<std::string> service_labels = absl::StrSplit(service, '.');
  labels->insert(labels->end(), service_labels.begin(), service_labels.end());

  std::vector<std::string> domain_labels = absl::StrSplit(domain, '.');
  labels->insert(labels->end(), domain_labels.begin(), domain_labels.end());
}

inline cast::mdns::DomainName GetPtrDomainName(const std::string& service,
                                               const std::string& domain) {
  std::vector<absl::string_view> labels;
  AddServiceInfoToLabels(service, domain, &labels);
  return cast::mdns::DomainName{labels};
}

inline cast::mdns::DomainName GetInstanceDomainName(const std::string& instance,
                                                    const std::string& service,
                                                    const std::string& domain) {
  std::vector<absl::string_view> labels;
  labels.emplace_back(instance);
  AddServiceInfoToLabels(service, domain, &labels);
  return cast::mdns::DomainName{labels};
}

inline cast::mdns::MdnsRecord CreatePtrRecord(
    const DnsSdInstanceRecord& record) {
  auto inner_domain = GetInstanceDomainName(
      record.instance_id(), record.service_id(), record.domain_id());
  cast::mdns::PtrRecordRdata data(inner_domain);

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  auto domain = GetPtrDomainName(record.service_id(), record.domain_id());
  return cast::mdns::MdnsRecord(domain, cast::mdns::DnsType::kPTR,
                                cast::mdns::DnsClass::kIN,
                                cast::mdns::RecordType::kShared, ttl, data);
}

inline cast::mdns::MdnsRecord CreateSrvRecord(
    const DnsSdInstanceRecord& record) {
  auto domain = GetInstanceDomainName(record.instance_id(), record.service_id(),
                                      record.domain_id());
  uint16_t port{0};
  if (record.address_v4().has_value()) {
    port = record.address_v4().value().port;
  } else if (record.address_v6().has_value()) {
    port = record.address_v6().value().port;
  } else {
    OSP_NOTREACHED();
  }

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  cast::mdns::SrvRecordRdata data(0, 0, port, domain);
  return cast::mdns::MdnsRecord(domain, cast::mdns::DnsType::kSRV,
                                cast::mdns::DnsClass::kIN,
                                cast::mdns::RecordType::kUnique, ttl, data);
}

inline absl::optional<cast::mdns::MdnsRecord> CreateARecord(
    const DnsSdInstanceRecord& record) {
  if (!record.address_v4().has_value()) {
    return absl::nullopt;
  }

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  cast::mdns::ARecordRdata data(record.address_v4().value().address);
  auto domain = GetInstanceDomainName(record.instance_id(), record.service_id(),
                                      record.domain_id());
  return cast::mdns::MdnsRecord(domain, cast::mdns::DnsType::kA,
                                cast::mdns::DnsClass::kIN,
                                cast::mdns::RecordType::kUnique, ttl, data);
}

inline absl::optional<cast::mdns::MdnsRecord> CreateAAAARecord(
    const DnsSdInstanceRecord& record) {
  if (!record.address_v6().has_value()) {
    return absl::nullopt;
  }

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(120);
  cast::mdns::AAAARecordRdata data(record.address_v6().value().address);
  auto domain = GetInstanceDomainName(record.instance_id(), record.service_id(),
                                      record.domain_id());
  return cast::mdns::MdnsRecord(domain, cast::mdns::DnsType::kAAAA,
                                cast::mdns::DnsClass::kIN,
                                cast::mdns::RecordType::kUnique, ttl, data);
}

inline cast::mdns::MdnsRecord CreateTxtRecord(
    const DnsSdInstanceRecord& record) {
  std::vector<std::vector<uint8_t>> txt = record.txt().GetData();
  std::vector<absl::string_view> txt_converted;
  for (const std::vector<uint8_t>& record : txt) {
    txt_converted.emplace_back(reinterpret_cast<const char*>(record.data()),
                               record.size());
  }
  cast::mdns::TxtRecordRdata data(txt_converted);

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds ttl(75 * 60);
  auto domain = GetInstanceDomainName(record.instance_id(), record.service_id(),
                                      record.domain_id());
  return cast::mdns::MdnsRecord(domain, cast::mdns::DnsType::kTXT,
                                cast::mdns::DnsClass::kIN,
                                cast::mdns::RecordType::kUnique, ttl, data);
}

}  // namespace

ErrorOr<DnsSdTxtRecord> CreateFromDnsTxt(
    const cast::mdns::TxtRecordRdata& txt_data) {
  DnsSdTxtRecord txt;
  if (txt_data.texts().size() == 1 && txt_data.texts()[0] == "") {
    return txt;
  }

  // Iterate backwards so that the first key of each type is the one that is
  // present at the end, as pet spec.
  for (auto it = txt_data.texts().rbegin(); it != txt_data.texts().rend();
       it++) {
    const std::string& text = *it;
    size_t index_of_eq = text.find_first_of('=');
    if (index_of_eq != std::string::npos) {
      if (index_of_eq == 0) {
        return Error::Code::kParameterInvalid;
      }
      std::string key = text.substr(0, index_of_eq);
      std::string value = text.substr(index_of_eq + 1);
      absl::Span<const uint8_t> data(
          reinterpret_cast<const uint8_t*>(value.c_str()), value.size());
      const auto set_result = txt.SetValue(key, data);
      if (!set_result.ok()) {
        return set_result;
      }
    } else {
      const auto set_result = txt.SetFlag(text, true);
      if (!set_result.ok()) {
        return set_result;
      }
    }
  }

  return txt;
}

ErrorOr<InstanceKey> GetInstanceKey(const cast::mdns::MdnsRecord& record) {
  const cast::mdns::DomainName& names =
      !IsPtrRecord(record)
          ? record.name()
          : absl::get<cast::mdns::PtrRecordRdata>(record.rdata()).ptr_domain();
  if (names.labels().size() < 4) {
    return Error::Code::kParameterInvalid;
  }

  auto it = names.labels().begin();
  InstanceKey result;
  result.instance_id = *it++;
  std::string service_name = *it++;
  std::string protocol = *it++;
  result.service_id = service_name.append(".").append(protocol);
  result.domain_id = absl::StrJoin(it, record.name().labels().end(), ".");
  if (!IsInstanceValid(result.instance_id) ||
      !IsServiceValid(result.service_id) || !IsDomainValid(result.domain_id)) {
    return Error::Code::kParameterInvalid;
  }
  return result;
}

ErrorOr<ServiceKey> GetServiceKey(const cast::mdns::MdnsRecord& record) {
  ErrorOr<InstanceKey> key_or_error = GetInstanceKey(record);
  if (key_or_error.is_error()) {
    return key_or_error.error();
  }
  return GetServiceKey(key_or_error.value());
}

ServiceKey GetServiceKey(const InstanceKey& key) {
  return {key.service_id, key.domain_id};
}

DnsQueryInfo GetInstanceQueryInfo(const InstanceKey& key) {
  auto domain =
      GetInstanceDomainName(key.instance_id, key.service_id, key.domain_id);
  return {domain, cast::mdns::DnsType::kANY, cast::mdns::DnsClass::kANY};
}

DnsQueryInfo GetPtrQueryInfo(const ServiceKey& key) {
  auto domain = GetPtrDomainName(key.service_id, key.domain_id);
  return {domain, cast::mdns::DnsType::kPTR, cast::mdns::DnsClass::kANY};
}

ServiceKey GetServiceKey(absl::string_view service, absl::string_view domain) {
  OSP_DCHECK(IsServiceValid(service));
  OSP_DCHECK(IsDomainValid(domain));
  return {service.data(), domain.data()};
}

bool IsPtrRecord(const cast::mdns::MdnsRecord& record) {
  return record.dns_type() == cast::mdns::DnsType::kPTR;
}

std::vector<cast::mdns::MdnsRecord> GetDnsRecords(
    const DnsSdInstanceRecord& record) {
  std::vector<cast::mdns::MdnsRecord> records{CreatePtrRecord(record),
                                              CreateSrvRecord(record),
                                              CreateTxtRecord(record)};

  auto v4 = CreateARecord(record);
  if (v4.has_value()) {
    records.push_back(std::move(v4.value()));
  }

  auto v6 = CreateAAAARecord(record);
  if (v6.has_value()) {
    records.push_back(std::move(v6.value()));
  }

  return records;
}

}  // namespace discovery
}  // namespace openscreen
