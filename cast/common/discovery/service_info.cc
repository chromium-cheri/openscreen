// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/discovery/service_info.h"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr uint8_t kMinimumAsciiInteger = '0';

// NOTE: Eureka uses base::StringToUint64 which takes in a string and reads it
// left to right, converts it to a number sequence, and uses the sequence to
// calculate the resulting integer. This process assumes that the input is in
// base 10. For example, ['1', '2', '3'] converts to 123.
std::string EncodeIntegerString(uint64_t value) {
  std::vector<uint8_t> data;
  while (value > 0) {
    data.insert(data.begin(), value % 10 + kMinimumAsciiInteger);
    value = value / 10;
  }
  return std::string(data.begin(), data.end());
}

ErrorOr<uint64_t> DecodeIntegerString(const std::string& value) {
  uint64_t current = 0;
  uint64_t last = 0;
  auto it = value.begin();
  for (; *it == '0'; it++) {
  }
  for (; it != value.end(); it++) {
    if (!std::isdigit(*it)) {
      return Error::Code::kParameterInvalid;
    }

    uint8_t index_value = *it - kMinimumAsciiInteger;
    current = current * 10 + index_value;
    if (current <= last) {
      return Error::Code::kParameterOutOfRange;
    }

    last = current;
  }

  return current;
}

bool TryParseString(const discovery::DnsSdTxtRecord& txt,
                    const std::string& key,
                    Error* error,
                    std::string* result) {
  ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  if (value.is_error()) {
    *error = value.error();
    return false;
  }

  const std::vector<uint8_t>& txt_value = value.value().get();
  *result = std::string(txt_value.begin(), txt_value.end());
  return true;
}

bool TryParseInt(const discovery::DnsSdTxtRecord& txt,
                 const std::string& key,
                 Error* error,
                 uint8_t* result) {
  ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  if (value.is_error()) {
    *error = value.error();
    return false;
  }

  const std::vector<uint8_t>& txt_value = value.value().get();
  if (txt_value.size() != 1) {
    *error = Error::Code::kParameterInvalid;
    return false;
  }

  *result = txt_value[0];
  return true;
}

bool IsError(Error error, Error* result) {
  if (error.ok()) {
    return false;
  } else {
    *result = error;
    return true;
  }
}

}  // namespace

ErrorOr<discovery::DnsSdInstanceRecord> Convert(const ServiceInfo& service) {
  OSP_DCHECK(discovery::IsServiceValid(kCastV2ServiceId));
  OSP_DCHECK(discovery::IsDomainValid(kCastV2DomainId));

  const std::string& instance_id = service.unique_id;
  if (!discovery::IsInstanceValid(instance_id)) {
    return Error::Code::kParameterInvalid;
  }

  uint64_t capabilities = static_cast<uint64_t>(service.capabilities);
  std::string capabilities_str = EncodeIntegerString(capabilities);

  discovery::DnsSdTxtRecord txt;
  Error error;
  if (IsError(txt.SetValue(kUniqueIdKey, service.unique_id), &error) ||
      IsError(txt.SetValue(kVersionId, service.protocol_version), &error) ||
      IsError(txt.SetValue(kCapabilitiesId, capabilities_str), &error) ||
      IsError(txt.SetValue(kStatusId, static_cast<uint8_t>(service.status)),
              &error) ||
      IsError(txt.SetValue(kFriendlyNameId, service.friendly_name), &error) ||
      IsError(txt.SetValue(kModelNameId, service.model_name), &error)) {
    return error;
  }

  if (!service.v4_address && !service.v6_address) {
    return Error::Code::kParameterInvalid;
  } else if (service.v4_address && service.v6_address) {
    return discovery::DnsSdInstanceRecord(instance_id, kCastV2ServiceId,
                                          kCastV2DomainId, service.v4_address,
                                          service.v6_address, std::move(txt));
  } else {
    const IPEndpoint& endpoint =
        service.v4_address ? service.v4_address : service.v6_address;
    return discovery::DnsSdInstanceRecord(instance_id, kCastV2ServiceId,
                                          kCastV2DomainId, endpoint,
                                          std::move(txt));
  }
}

ErrorOr<ServiceInfo> Convert(const discovery::DnsSdInstanceRecord& instance) {
  if (instance.service_id() != kCastV2ServiceId) {
    return Error::Code::kParameterInvalid;
  }

  ServiceInfo record;
  record.v4_address = instance.address_v4();
  record.v6_address = instance.address_v6();

  const auto& txt = instance.txt();
  std::string capabilities_base64;
  std::string unique_id;
  uint8_t status;
  Error error;
  if (!TryParseInt(txt, kVersionId, &error, &record.protocol_version) ||
      !TryParseInt(txt, kStatusId, &error, &status) ||
      !TryParseString(txt, kFriendlyNameId, &error, &record.friendly_name) ||
      !TryParseString(txt, kModelNameId, &error, &record.model_name) ||
      !TryParseString(txt, kCapabilitiesId, &error, &capabilities_base64) ||
      !TryParseString(txt, kUniqueIdKey, &error, &record.unique_id)) {
    return error;
  }

  record.status = static_cast<ReceiverStatus>(status);

  ErrorOr<uint64_t> capabilities_flags =
      DecodeIntegerString(capabilities_base64);
  if (capabilities_flags.is_error()) {
    return capabilities_flags.error();
  }
  record.capabilities =
      static_cast<ReceiverCapabilities>(capabilities_flags.value());

  return record;
}

}  // namespace cast
}  // namespace openscreen
