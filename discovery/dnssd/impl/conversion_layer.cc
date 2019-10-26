// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include "absl/strings/str_join.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"

namespace openscreen {
namespace discovery {
namespace {

template <typename T>
inline Error CreateRecord(absl::optional<T>* stored,
                          const cast::mdns::MdnsRecord& record) {
  Error result =
      stored->has_value() ? Error::Code::kItemAlreadyExists : Error::None();
  *stored = absl::get<T>(record.rdata());
  return result;
}

template <typename T>
inline Error UpdateRecord(absl::optional<T>* stored,
                          const cast::mdns::MdnsRecord& record) {
  Error result =
      !stored->has_value() ? Error::Code::kItemNotFound : Error::None();
  *stored = absl::get<T>(record.rdata());
  return result;
}

template <typename T>
inline Error DeleteRecord(absl::optional<T>* stored) {
  Error result =
      !stored->has_value() ? Error::Code::kItemNotFound : Error::None();
  *stored = absl::nullopt;
  return result;
}

template <typename T>
inline Error ProcessRecord(absl::optional<T>* stored,
                           const cast::mdns::MdnsRecord& record,
                           cast::mdns::RecordChangedEvent event) {
  switch (event) {
    case cast::mdns::RecordChangedEvent::kCreated:
      return CreateRecord(stored, record);
    case cast::mdns::RecordChangedEvent::kUpdated:
      return UpdateRecord(stored, record);
    case cast::mdns::RecordChangedEvent::kDeleted:
      return DeleteRecord(stored);
  }
}

}  // namespace

ErrorOr<DnsSdTxtRecord> ConvertFromDnsTxt(
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

ErrorOr<DnsSdInstanceRecord> ConvertFromDnsData(const InstanceKey& instance_key,
                                                const DnsData& data) {
  if (!data.srv.has_value() || !data.txt.has_value() ||
      (!data.a.has_value() && !data.aaaa.has_value())) {
    return Error::Code::kItemNotFound;
  }

  if (!IsInstanceValid(instance_key.instance_id) ||
      !IsServiceValid(instance_key.service_id) ||
      !IsDomainValid(instance_key.domain_id)) {
    return Error::Code::kParameterInvalid;
  }

  absl::optional<IPEndpoint> v4 = absl::nullopt;
  if (data.a.has_value()) {
    v4 = {data.a.value().ipv4_address(), data.srv.value().port()};
  }

  absl::optional<IPEndpoint> v6 = absl::nullopt;
  if (data.aaaa.has_value()) {
    v6 = {data.aaaa.value().ipv6_address(), data.srv.value().port()};
  }

  ErrorOr<DnsSdTxtRecord> txt_or_error = ConvertFromDnsTxt(data.txt.value());
  if (txt_or_error.is_error()) {
    return txt_or_error.error();
  }

  if (v4.has_value() && v6.has_value()) {
    return DnsSdInstanceRecord(instance_key.instance_id,
                               instance_key.service_id, instance_key.domain_id,
                               std::move(v4.value()), std::move(v6.value()),
                               std::move(txt_or_error.value()));
  } else {
    IPEndpoint ep =
        v4.has_value() ? std::move(v4.value()) : std::move(v6.value());
    return DnsSdInstanceRecord(instance_key.instance_id,
                               instance_key.service_id, instance_key.domain_id,
                               std::move(ep), std::move(txt_or_error.value()));
  }
}

Error ApplyDataRecordChange(DnsData* data,
                            const cast::mdns::MdnsRecord& record,
                            cast::mdns::RecordChangedEvent event) {
  switch (record.dns_type()) {
    case cast::mdns::DnsType::kSRV:
      return ProcessRecord(&data->srv, record, event);
    case cast::mdns::DnsType::kTXT:
      return ProcessRecord(&data->txt, record, event);
    case cast::mdns::DnsType::kA:
      return ProcessRecord(&data->a, record, event);
    case cast::mdns::DnsType::kAAAA:
      return ProcessRecord(&data->aaaa, record, event);
    default:
      return Error::Code::kOperationInvalid;
  }
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
  result.service_id = *it++ + "." + *it++;
  result.domain_id = absl::StrJoin(it, record.name().labels().end(), ".");
  return result;
}

ErrorOr<ServiceKey> GetServiceKey(const cast::mdns::MdnsRecord& record) {
  ErrorOr<InstanceKey> key_or_error = GetInstanceKey(record);
  if (key_or_error.is_error()) {
    return key_or_error.error();
  }
  return ServiceKey{key_or_error.value().service_id,
                    key_or_error.value().domain_id};
}

ServiceKey GetServiceKey(absl::string_view service, absl::string_view domain) {
  return {service.data(), domain.data()};
}

}  // namespace discovery
}  // namespace openscreen
