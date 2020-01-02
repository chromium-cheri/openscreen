// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/discovery/service_info.h"

#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {
namespace {

// Constants used for testing.
static const IPAddress kAddressV4(192, 168, 0, 0);
static const IPAddress kAddressV6(1, 2, 3, 4, 5, 6, 7, 8);
static const IPEndpoint kEndpointV4{kAddressV4, 0};
static const IPEndpoint kEndpointV6{kAddressV6, 0};
static constexpr char kTestUniqueId[] = "1234";
static constexpr char kFriendlyName[] = "Friendly Name 123";
static constexpr char kModelName[] = "Eureka";
static constexpr uint8_t kTestVersion = 0;
static constexpr char kCapabilitiesString[] =
    "AwAAAAAAAAA=";  // base64 encoded '0x00000003'
static constexpr ReceiverCapabilities kCapabilitiesParsed =
    static_cast<ReceiverCapabilities>(0x03);
static constexpr uint8_t kStatus = 0x01;
static constexpr ReceiverStatus kStatusParsed = ReceiverStatus::kBusy;

discovery::DnsSdTxtRecord CreateValidTxt() {
  discovery::DnsSdTxtRecord txt;
  txt.SetValue(kUniqueIdKey, kTestUniqueId);
  txt.SetValue(kVersionId, kTestVersion);
  txt.SetValue(kCapabilitiesId, kCapabilitiesString);
  txt.SetValue(kStatusId, kStatus);
  txt.SetValue(kFriendlyNameId, kFriendlyName);
  txt.SetValue(kModelNameId, kModelName);
  return txt;
}

void CompareString(const discovery::DnsSdTxtRecord& txt,
                   const std::string& key,
                   const std::string& expected) {
  ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  ASSERT_FALSE(value.is_error()) << "expected: " << expected;
  const std::vector<uint8_t>& data = value.value().get();
  std::string parsed_value = std::string(data.begin(), data.end());
  EXPECT_EQ(parsed_value, expected) << "key: " << key;
}

void CompareInt(const discovery::DnsSdTxtRecord& txt,
                const std::string& key,
                uint8_t expected) {
  ErrorOr<discovery::DnsSdTxtRecord::ValueRef> value = txt.GetValue(key);
  ASSERT_FALSE(value.is_error())
      << "key: '" << key << "'' expected: '" << expected << "'";
  const std::vector<uint8_t>& data = value.value().get();
  std::string parsed_value = std::string(data.begin(), data.end());
  ASSERT_EQ(data.size(), size_t{1})
      << "key: '" << key << "' actual value: '" << parsed_value << "'";
  EXPECT_EQ(data[0], expected)
      << "key: '" << key << "' actual value: '" << parsed_value << "'";
}

}  // namespace

TEST(ServiceInfoTests, ConvertValidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  discovery::DnsSdInstanceRecord record(instance, kCastV2ServiceId,
                                        kCastV2DomainId, kEndpointV4,
                                        kEndpointV6, txt);
  ErrorOr<ServiceInfo> info = Convert(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_address);
  EXPECT_EQ(info.value().v4_address, kEndpointV4);
  EXPECT_TRUE(info.value().v6_address);
  EXPECT_EQ(info.value().v6_address, kEndpointV6);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4, txt);
  ASSERT_FALSE(record.address_v6());
  info = Convert(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_address);
  EXPECT_EQ(info.value().v4_address, kEndpointV4);
  EXPECT_FALSE(info.value().v6_address);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV6, txt);
  ASSERT_FALSE(record.address_v4());
  info = Convert(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_FALSE(info.value().v4_address);
  EXPECT_TRUE(info.value().v6_address);
  EXPECT_EQ(info.value().v6_address, kEndpointV6);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);
}

TEST(ServiceInfoTests, ConvertInvalidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  txt.ClearValue(kUniqueIdKey);
  discovery::DnsSdInstanceRecord record(instance, kCastV2ServiceId,
                                        kCastV2DomainId, kEndpointV4,
                                        kEndpointV6, txt);
  EXPECT_TRUE(Convert(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kVersionId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(Convert(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kCapabilitiesId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(Convert(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kStatusId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(Convert(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kFriendlyNameId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(Convert(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kModelNameId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(Convert(record).is_error());
}

TEST(ServiceInfoTests, ConvertValidToDnsSd) {
  ServiceInfo info;
  info.v4_address = kEndpointV4;
  info.v6_address = kEndpointV6;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  ErrorOr<discovery::DnsSdInstanceRecord> record = Convert(info);
  ASSERT_TRUE(record.is_value());
  EXPECT_EQ(record.value().instance_id(), kTestUniqueId);
  EXPECT_TRUE(record.value().address_v4());
  EXPECT_EQ(record.value().address_v4(), kEndpointV4);
  EXPECT_TRUE(record.value().address_v6());
  EXPECT_EQ(record.value().address_v6(), kEndpointV6);
  CompareString(record.value().txt(), kUniqueIdKey, kTestUniqueId);
  CompareString(record.value().txt(), kCapabilitiesId, kCapabilitiesString);
  CompareString(record.value().txt(), kModelNameId, kModelName);
  CompareString(record.value().txt(), kFriendlyNameId, kFriendlyName);
  CompareInt(record.value().txt(), kVersionId, kTestVersion);
  CompareInt(record.value().txt(), kStatusId, kStatus);

  info.v6_address = IPEndpoint{};
  record = Convert(info);
  ASSERT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().address_v4());
  EXPECT_EQ(record.value().address_v4(), kEndpointV4);
  EXPECT_FALSE(record.value().address_v6());
  CompareString(record.value().txt(), kUniqueIdKey, kTestUniqueId);
  CompareString(record.value().txt(), kCapabilitiesId, kCapabilitiesString);
  CompareString(record.value().txt(), kModelNameId, kModelName);
  CompareString(record.value().txt(), kFriendlyNameId, kFriendlyName);
  CompareInt(record.value().txt(), kVersionId, kTestVersion);
  CompareInt(record.value().txt(), kStatusId, kStatus);

  info.v6_address = kEndpointV6;
  info.v4_address = IPEndpoint{};
  record = Convert(info);
  ASSERT_TRUE(record.is_value());
  EXPECT_FALSE(record.value().address_v4());
  EXPECT_TRUE(record.value().address_v6());
  EXPECT_EQ(record.value().address_v6(), kEndpointV6);
  CompareString(record.value().txt(), kUniqueIdKey, kTestUniqueId);
  CompareString(record.value().txt(), kCapabilitiesId, kCapabilitiesString);
  CompareString(record.value().txt(), kModelNameId, kModelName);
  CompareString(record.value().txt(), kFriendlyNameId, kFriendlyName);
  CompareInt(record.value().txt(), kVersionId, kTestVersion);
  CompareInt(record.value().txt(), kStatusId, kStatus);
}

TEST(ServiceInfoTests, ConvertInvalidToDnsSd) {
  ServiceInfo info;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  ErrorOr<discovery::DnsSdInstanceRecord> record = Convert(info);
  EXPECT_TRUE(record.is_error());
}

TEST(ServiceInfoTests, IdentityChecks) {
  ServiceInfo info;
  info.v4_address = kEndpointV4;
  info.v6_address = kEndpointV6;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  ErrorOr<discovery::DnsSdInstanceRecord> converted_record = Convert(info);
  ASSERT_TRUE(converted_record.is_value());
  ErrorOr<ServiceInfo> identity_info = Convert(converted_record.value());
  ASSERT_TRUE(identity_info.is_value());
  EXPECT_EQ(identity_info.value(), info);

  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  discovery::DnsSdInstanceRecord record(kTestUniqueId, kCastV2ServiceId,
                                        kCastV2DomainId, kEndpointV4,
                                        kEndpointV6, txt);
  ErrorOr<ServiceInfo> converted_info = Convert(record);
  ASSERT_TRUE(converted_info.is_value());
  ErrorOr<discovery::DnsSdInstanceRecord> identity_record =
      Convert(converted_info.value());
  ASSERT_TRUE(identity_record.is_value());
  EXPECT_EQ(identity_record.value(), record);
}

}  // namespace cast
}  // namespace openscreen
