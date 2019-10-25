// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {
namespace {

static const IPAddress v4_address =
    IPAddress(std::array<uint8_t, 4>{192, 168, 0, 0});
static const IPAddress v6_address = IPAddress(std::array<uint8_t, 16>{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
static const std::string instance_name = "instance";
static const std::string service_name = "_srv-name._udp";
static const std::string domain_name = "local";
static const InstanceKey key = {instance_name, service_name, domain_name};
static constexpr uint16_t port_num = uint16_t{80};

DnsData CreateFullyPopulatedData() {
  cast::mdns::DomainName target{instance_name, "_srv-name", "_udp",
                                domain_name};
  cast::mdns::SrvRecordRdata srv(0, 0, port_num, target);
  cast::mdns::TxtRecordRdata txt{"name=value", "boolValue"};
  cast::mdns::ARecordRdata a(v4_address);
  cast::mdns::AAAARecordRdata aaaa(v6_address);
  return DnsData{srv, txt, a, aaaa};
}

cast::mdns::MdnsRecord CreateFullyPopulatedRecord() {
  cast::mdns::DomainName target{instance_name, "_srv-name", "_udp",
                                domain_name};
  auto type = cast::mdns::DnsType::kSRV;
  auto clazz = cast::mdns::DnsClass::kIN;
  auto record_type = cast::mdns::RecordType::kShared;
  auto ttl = std::chrono::seconds(0);
  cast::mdns::SrvRecordRdata srv(0, 0, port_num, target);
  return cast::mdns::MdnsRecord(target, type, clazz, record_type, ttl, srv);
}

}  // namespace

// TXT Conversions.
TEST(DnsSdConversionLayerTest, TestConvertTxtEmpty) {
  cast::mdns::TxtRecordRdata txt{};
  ErrorOr<DnsSdTxtRecord> record = ConvertFromDnsTxt(txt);
  EXPECT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestConvertTxtOnlyEmptyString) {
  cast::mdns::TxtRecordRdata txt;
  ErrorOr<DnsSdTxtRecord> record = ConvertFromDnsTxt(txt);
  EXPECT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestConvertTxtValidKeyValue) {
  cast::mdns::TxtRecordRdata txt{"name=value"};
  ErrorOr<DnsSdTxtRecord> record = ConvertFromDnsTxt(txt);
  ASSERT_TRUE(record.is_value());
  ASSERT_TRUE(record.value().GetValue("name").is_value());
  EXPECT_STREQ(reinterpret_cast<const char*>(
                   record.value().GetValue("name").value().data()),
               "value");
}

TEST(DnsSdConversionLayerTest, TestConvertTxtInvalidKeyValue) {
  cast::mdns::TxtRecordRdata txt{"=value"};
  ErrorOr<DnsSdTxtRecord> record = ConvertFromDnsTxt(txt);
  EXPECT_TRUE(record.is_error());
}

TEST(DnsSdConversionLayerTest, TestConvertTxtValidBool) {
  cast::mdns::TxtRecordRdata txt{"name"};
  ErrorOr<DnsSdTxtRecord> record = ConvertFromDnsTxt(txt);
  ASSERT_TRUE(record.is_value());
  ASSERT_TRUE(record.value().GetFlag("name").is_value());
  EXPECT_TRUE(record.value().GetFlag("name").value());
}

// DnsData Conversions.
TEST(DnsSdConversionLayerTest, TestConvertDnsDataCorrectly) {
  DnsData data = CreateFullyPopulatedData();
  ErrorOr<DnsSdInstanceRecord> result = ConvertFromDnsData(key, data);
  ASSERT_TRUE(result.is_value());

  DnsSdInstanceRecord record = result.value();
  ASSERT_TRUE(record.address_v4().has_value());
  ASSERT_TRUE(record.address_v6().has_value());
  EXPECT_EQ(record.instance_id(), instance_name);
  EXPECT_EQ(record.service_id(), service_name);
  EXPECT_EQ(record.domain_id(), domain_name);
  EXPECT_EQ(record.address_v4().value().port, port_num);
  EXPECT_EQ(record.address_v4().value().address, v4_address);
  EXPECT_EQ(record.address_v6().value().port, port_num);
  EXPECT_EQ(record.address_v6().value().address, v6_address);
  EXPECT_FALSE(record.txt().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestConvertDnsDataMissingData) {
  InstanceKey instance{instance_name, service_name, domain_name};
  DnsData data = CreateFullyPopulatedData();
  EXPECT_TRUE(ConvertFromDnsData(key, data).is_value());

  data = CreateFullyPopulatedData();
  data.srv = absl::nullopt;
  EXPECT_FALSE(ConvertFromDnsData(key, data).is_value());

  data = CreateFullyPopulatedData();
  data.txt = absl::nullopt;
  EXPECT_FALSE(ConvertFromDnsData(key, data).is_value());

  data = CreateFullyPopulatedData();
  data.a = absl::nullopt;
  EXPECT_TRUE(ConvertFromDnsData(key, data).is_value());

  data = CreateFullyPopulatedData();
  data.aaaa = absl::nullopt;
  EXPECT_TRUE(ConvertFromDnsData(key, data).is_value());

  data = CreateFullyPopulatedData();
  data.a = absl::nullopt;
  data.aaaa = absl::nullopt;
  EXPECT_FALSE(ConvertFromDnsData(key, data).is_value());
}

TEST(DnsSdConversionLayerTest, TestConvertDnsDataOneAddress) {
  // Address v4.
  DnsData data = CreateFullyPopulatedData();
  data.aaaa = absl::nullopt;
  ErrorOr<DnsSdInstanceRecord> result = ConvertFromDnsData(key, data);
  ASSERT_TRUE(result.is_value());

  DnsSdInstanceRecord record = result.value();
  EXPECT_FALSE(record.address_v6().has_value());
  ASSERT_TRUE(record.address_v4().has_value());
  EXPECT_EQ(record.address_v4().value().port, port_num);
  EXPECT_EQ(record.address_v4().value().address, v4_address);

  // Address v6.
  data = CreateFullyPopulatedData();
  data.a = absl::nullopt;
  result = ConvertFromDnsData(key, data);
  ASSERT_TRUE(result.is_value());

  record = result.value();
  EXPECT_FALSE(record.address_v4().has_value());
  ASSERT_TRUE(record.address_v6().has_value());
  EXPECT_EQ(record.address_v6().value().port, port_num);
  EXPECT_EQ(record.address_v6().value().address, v6_address);
}

TEST(DnsSdConversionLayerTest, TestConvertDnsDataBadTxt) {
  DnsData data = CreateFullyPopulatedData();
  data.txt = cast::mdns::TxtRecordRdata{"=bad_text"};
  ErrorOr<DnsSdInstanceRecord> result = ConvertFromDnsData(key, data);
  EXPECT_TRUE(result.is_error());
}

// Get*Key functions.
TEST(DnsSdConversionLayerTest, GetSrvKeyFromRecordTest) {
  cast::mdns::MdnsRecord record = CreateFullyPopulatedRecord();
  InstanceKey key = GetInstanceKey(record);
  EXPECT_EQ(key.instance_id, instance_name);
  EXPECT_EQ(key.service_id, service_name);
  EXPECT_EQ(key.domain_id, domain_name);
}

TEST(DnsSdConversionLayerTest, GetPtrKeyFromRecordTest) {
  cast::mdns::MdnsRecord record = CreateFullyPopulatedRecord();
  ServiceKey key = GetServiceKey(record);
  EXPECT_EQ(key.service_id, service_name);
  EXPECT_EQ(key.domain_id, domain_name);
}

TEST(DnsSdConversionLayerTest, GetPtrKeyFromStringsTest) {
  ServiceKey key = GetServiceKey(service_name, domain_name);
  EXPECT_EQ(key.service_id, service_name);
  EXPECT_EQ(key.domain_id, domain_name);
}

}  // namespace discovery
}  // namespace openscreen
