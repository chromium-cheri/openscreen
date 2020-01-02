// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/discovery/service_info.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/escaping.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

std::string EncodeBase64(uint64_t value) {
  absl::string_view decoded(reinterpret_cast<char*>(&value), sizeof(value));
  std::string encoded;
  absl::Base64Escape(decoded, &encoded);
  return encoded;
}

bool IsError(Error error, Error* result) {
  if (error.ok()) {
    return false;
  } else {
    *result = error;
    return true;
  }
}

ErrorOr<uint64_t> DecodeBase64(std::string value) {
  std::string decoded;
  if (!absl::Base64Unescape(value, &decoded) ||
      decoded.size() > sizeof(uint64_t)) {
    return Error::Code::kParameterInvalid;
  }

  std::vector<uint8_t> bytes(decoded.begin(), decoded.end());
  if (bytes.size() < sizeof(uint64_t)) {
    bytes.insert(bytes.begin(), sizeof(uint64_t) - bytes.size(), 0x00);
  }

  return *reinterpret_cast<uint64_t*>(bytes.data());
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

}  // namespace

ErrorOr<discovery::DnsSdInstanceRecord> Convert(const ServiceInfo& service) {
  OSP_DCHECK(discovery::IsServiceValid(kCastV2ServiceId));
  OSP_DCHECK(discovery::IsDomainValid(kCastV2DomainId));

  const std::string& instance_id = service.unique_id;
  if (!discovery::IsInstanceValid(instance_id)) {
    return Error::Code::kParameterInvalid;
  }

  uint64_t capabilities = static_cast<uint64_t>(service.capabilities);
  std::string capabilities_str = EncodeBase64(capabilities);

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

  ErrorOr<uint64_t> capabilities_flags = DecodeBase64(capabilities_base64);
  if (capabilities_flags.is_error()) {
    return capabilities_flags.error();
  }
  record.capabilities =
      static_cast<ReceiverCapabilities>(capabilities_flags.value());

  return record;
}

}  // namespace cast
}  // namespace openscreen
