// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {
namespace {

cast::mdns::MdnsRecord GetARecord(IPAddress address) {
  constexpr std::chrono::seconds ttl(120);
  std::vector<absl::string_view> labels{"label"};
  cast::mdns::ARecordRdata data(address);
  cast::mdns::DomainName domain(labels);
  cast::mdns::MdnsRecord aRecord(domain, cast::mdns::DnsType::kA,
                                 cast::mdns::DnsClass::kIN,
                                 cast::mdns::RecordType::kUnique, ttl, data);
  return aRecord;
}

}  // namespace

using testing::Return;

class MockMdnsService : public cast::mdns::MdnsService {
 public:
  void StartQuery(const cast::mdns::DomainName& name,
                  cast::mdns::DnsType dns_type,
                  cast::mdns::DnsClass dns_class,
                  cast::mdns::MdnsRecordChangedCallback* callback) override {
    OSP_UNIMPLEMENTED();
  }

  void StopQuery(const cast::mdns::DomainName& name,
                 cast::mdns::DnsType dns_type,
                 cast::mdns::DnsClass dns_class,
                 cast::mdns::MdnsRecordChangedCallback* callback) override {
    OSP_UNIMPLEMENTED();
  }

  MOCK_METHOD1(RegisterRecord, void(const cast::mdns::MdnsRecord& record));
  MOCK_METHOD1(DeregisterRecord, void(const cast::mdns::MdnsRecord& record));
};

class PublisherTesting : public PublisherImpl {
 public:
  PublisherTesting()
      : PublisherImpl(&mock_service_,
                      [this](const DnsSdInstanceRecord& record) {
                        return GetRecords(record);
                      }) {}

  MockMdnsService* mdns_service() { return &mock_service_; }

  MOCK_METHOD1(GetRecords,
               std::vector<cast::mdns::MdnsRecord>(const DnsSdInstanceRecord&));

 private:
  MockMdnsService mock_service_;
};

TEST(DnsSdPublisherImplTests, TestRegisterAndDeregister) {
  PublisherTesting publisher;
  IPAddress address = IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});
  IPAddress address2 = IPAddress(std::array<uint8_t, 4>{{192, 168, 255, 255}});
  DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                             {address, 80}, {});
  cast::mdns::MdnsRecord aRecord = GetARecord(address);
  cast::mdns::MdnsRecord aRecord2 = GetARecord(address2);
  std::vector<cast::mdns::MdnsRecord> record_set{aRecord};
  std::vector<cast::mdns::MdnsRecord> record_set2{aRecord2};

  EXPECT_CALL(publisher, GetRecords(record)).WillOnce(Return(record_set));
  EXPECT_CALL(*publisher.mdns_service(), RegisterRecord(aRecord)).Times(1);
  publisher.Register(record);

  EXPECT_CALL(publisher, GetRecords(record)).Times(0);
  EXPECT_CALL(*publisher.mdns_service(), DeregisterRecord(aRecord2)).Times(0);
  publisher.DeregisterAll("_other-service._udp", "domain");
  publisher.DeregisterAll("_service._udp", "other.domain");

  EXPECT_CALL(publisher, GetRecords(record)).WillOnce(Return(record_set2));
  EXPECT_CALL(*publisher.mdns_service(), DeregisterRecord(aRecord2)).Times(1);
  publisher.DeregisterAll("_service._udp", "domain");
}

}  // namespace discovery
}  // namespace openscreen
