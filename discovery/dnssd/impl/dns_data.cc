// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/dns_data.h"

#include "absl/types/optional.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/conversion_layer.h"

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
  return Error::Code::kUnknownError;
}

}  // namespace

// static
ErrorOr<DnsData> DnsData::Create(const InstanceKey& instance_key) {
  if (!IsInstanceValid(instance_key.instance_id) ||
      !IsServiceValid(instance_key.service_id) ||
      !IsDomainValid(instance_key.domain_id)) {
    return Error::Code::kParameterInvalid;
  }

  return DnsData(instance_key);
}

DnsData::DnsData(const InstanceKey& instance_id) : instance_id_(instance_id) {}

ErrorOr<DnsSdInstanceRecord> DnsData::CreateRecord() {
  if (!srv_.has_value() || !txt_.has_value() ||
      (!a_.has_value() && !aaaa_.has_value())) {
    return Error::Code::kItemNotFound;
  }

  absl::optional<IPEndpoint> v4 = absl::nullopt;
  if (a_.has_value()) {
    v4 = {a_.value().ipv4_address(), srv_.value().port()};
  }

  absl::optional<IPEndpoint> v6 = absl::nullopt;
  if (aaaa_.has_value()) {
    v6 = {aaaa_.value().ipv6_address(), srv_.value().port()};
  }

  ErrorOr<DnsSdTxtRecord> txt_or_error = CreateFromDnsTxt(txt_.value());
  if (txt_or_error.is_error()) {
    return txt_or_error.error();
  }

  if (v4.has_value() && v6.has_value()) {
    return DnsSdInstanceRecord(instance_id_.instance_id,
                               instance_id_.service_id, instance_id_.domain_id,
                               std::move(v4.value()), std::move(v6.value()),
                               std::move(txt_or_error.value()));
  } else {
    IPEndpoint ep =
        v4.has_value() ? std::move(v4.value()) : std::move(v6.value());
    return DnsSdInstanceRecord(instance_id_.instance_id,
                               instance_id_.service_id, instance_id_.domain_id,
                               std::move(ep), std::move(txt_or_error.value()));
  }
}

Error DnsData::ApplyDataRecordChange(const cast::mdns::MdnsRecord& record,
                                     cast::mdns::RecordChangedEvent event) {
  switch (record.dns_type()) {
    case cast::mdns::DnsType::kSRV:
      return ProcessRecord(&srv_, record, event);
    case cast::mdns::DnsType::kTXT:
      return ProcessRecord(&txt_, record, event);
    case cast::mdns::DnsType::kA:
      return ProcessRecord(&a_, record, event);
    case cast::mdns::DnsType::kAAAA:
      return ProcessRecord(&aaaa_, record, event);
    default:
      return Error::Code::kOperationInvalid;
  }
}

}  // namespace discovery
}  // namespace openscreen
