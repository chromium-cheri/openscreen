// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/service_info.h"

#include "cast/common/public/testing/discovery_utils.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {

TEST(ServiceInfoTests, ConvertValidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  discovery::DnsSdInstanceEndpoint record(instance, kCastV2ServiceId,
                                          kCastV2DomainId, txt, kEndpointV4,
                                          kEndpointV6, 0);
  ErrorOr<ServiceInfo> info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_endpoint);
  EXPECT_EQ(info.value().v4_endpoint, kEndpointV4);
  EXPECT_TRUE(info.value().v6_endpoint);
  EXPECT_EQ(info.value().v6_endpoint, kEndpointV6);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kEndpointV4, 0);
  ASSERT_FALSE(record.address_v6());
  info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_endpoint);
  EXPECT_EQ(info.value().v4_endpoint, kEndpointV4);
  EXPECT_FALSE(info.value().v6_endpoint);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceEndpoint(
      instance, kCastV2ServiceId, kCastV2DomainId, txt, kEndpointV6, 0);
  ASSERT_FALSE(record.address_v4());
  info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_FALSE(info.value().v4_endpoint);
  EXPECT_TRUE(info.value().v6_endpoint);
  EXPECT_EQ(info.value().v6_endpoint, kEndpointV6);
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
  discovery::DnsSdInstanceEndpoint record(instance, kCastV2ServiceId,
                                          kCastV2DomainId, txt, kEndpointV4,
                                          kEndpointV6, 0);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kVersionId);
  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt, kEndpointV4,
                                            kEndpointV6, 0);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kCapabilitiesId);
  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt, kEndpointV4,
                                            kEndpointV6, 0);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kStatusId);
  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt, kEndpointV4,
                                            kEndpointV6, 0);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kFriendlyNameId);
  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt, kEndpointV4,
                                            kEndpointV6, 0);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kModelNameId);
  record = discovery::DnsSdInstanceEndpoint(instance, kCastV2ServiceId,
                                            kCastV2DomainId, txt, kEndpointV4,
                                            kEndpointV6, 0);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());
}

TEST(ServiceInfoTests, ConvertValidToDnsSd) {
  ServiceInfo info;
  info.v4_endpoint = kEndpointV4;
  info.v6_endpoint = kEndpointV6;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  discovery::DnsSdInstanceRecord record = ServiceInfoToDnsSdRecord(info);
  CompareTxtString(record.txt(), kUniqueIdKey, kTestUniqueId);
  CompareTxtString(record.txt(), kCapabilitiesId, kCapabilitiesString);
  CompareTxtString(record.txt(), kModelNameId, kModelName);
  CompareTxtString(record.txt(), kFriendlyNameId, kFriendlyName);
  CompareTxtInt(record.txt(), kVersionId, kTestVersion);
  CompareTxtInt(record.txt(), kStatusId, kStatus);
}

}  // namespace cast
}  // namespace openscreen
