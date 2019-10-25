// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include "absl/strings/str_join.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"

namespace openscreen {
namespace discovery {

ErrorOr<DnsSdTxtRecord> ConvertFromDnsTxt(
    const cast::mdns::TxtRecordRdata& txt_data) {
  DnsSdTxtRecord txt;
  if (txt_data.texts().size() == 1 && txt_data.texts()[0] == "") {
    return txt;
  }

  // Iterate backwards so that the first key of each type is the one that is
  // present at the end, as pet spec.
  for (int i = txt_data.texts().size() - 1; i >= 0; i--) {
    const std::string& text = txt_data.texts()[i];
    size_t index_of_eq = text.find_first_of('=');
    Error result = Error::None();
    if (index_of_eq != std::string::npos) {
      if (!index_of_eq) {
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

ErrorOr<DnsSdInstanceRecord> ConvertFromDnsData(const InstanceKey& srv_key,
                                                const DnsData& data) {
  if (!data.srv.has_value() || !data.txt.has_value() ||
      (!data.a.has_value() && !data.aaaa.has_value())) {
    return Error::Code::kItemNotFound;
  }

  if (!IsInstanceValid(srv_key.instance_id) ||
      !IsServiceValid(srv_key.service_id) ||
      !IsDomainValid(srv_key.domain_id)) {
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
  DnsSdTxtRecord txt = std::move(txt_or_error.value());

  if (v4.has_value() && v6.has_value()) {
    return DnsSdInstanceRecord(srv_key.instance_id, srv_key.service_id,
                               srv_key.domain_id, std::move(v4.value()),
                               std::move(v6.value()), std::move(txt));
  } else {
    IPEndpoint ep =
        v4.has_value() ? std::move(v4.value()) : std::move(v6.value());
    return DnsSdInstanceRecord(srv_key.instance_id, srv_key.service_id,
                               srv_key.domain_id, std::move(ep),
                               std::move(txt));
  }
}

InstanceKey GetInstanceKey(const cast::mdns::MdnsRecord& record) {
  auto it = record.name().labels().begin();
  InstanceKey result;
  result.instance_id = *it++;
  result.service_id = *it++ + "." + *it++;
  result.domain_id = absl::StrJoin(it, record.name().labels().end(), ".");
  return result;
}

ServiceKey GetServiceKey(const cast::mdns::MdnsRecord& record) {
  auto it = record.name().labels().begin();
  it++;  // The first label is the instance id, so skip it.
  ServiceKey result;
  result.service_id = *it++ + "." + *it++;
  result.domain_id = absl::StrJoin(it, record.name().labels().end(), ".");
  return result;
}

ServiceKey GetServiceKey(absl::string_view service, absl::string_view domain) {
  return {service.data(), domain.data()};
}

}  // namespace discovery
}  // namespace openscreen
