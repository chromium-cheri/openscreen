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

MdnsRecord GetARecord(IPAddress address) {
  constexpr std::chrono::seconds ttl(120);
  std::vector<absl::string_view> labels{"label"};
  ARecordRdata data(address);
  DomainName domain(labels);
  MdnsRecord aRecord(domain, DnsType::kA, DnsClass::kIN, RecordType::kUnique,
                     ttl, data);
  return aRecord;
}

}  // namespace

using testing::Return;

class MockMdnsService : public MdnsService {
 public:
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback) override {
    OSP_UNIMPLEMENTED();
  }

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback) override {
    OSP_UNIMPLEMENTED();
  }

  MOCK_METHOD1(RegisterRecord, void(const MdnsRecord& record));
  MOCK_METHOD1(DeregisterRecord, void(const MdnsRecord& record));
};

class PublisherTesting : public PublisherImpl {
 public:
  PublisherTesting()
      : PublisherImpl(&mock_service_,
                      [this](const DnsSdInstanceRecord& record) {
                        return GetRecords(record);
                      }) {}

  MockMdnsService& mdns_service() { return mock_service_; }

  MOCK_METHOD1(GetRecords, std::vector<MdnsRecord>(const DnsSdInstanceRecord&));

 private:
  MockMdnsService mock_service_;
};

TEST(DnsSdPublisherImplTests, TestRegisterAndDeregister) {
  PublisherTesting publisher;
  IPAddress address = IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});
  IPAddress address2 = IPAddress(std::array<uint8_t, 4>{{192, 168, 255, 255}});
  DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                             {address, 80}, {});
  MdnsRecord aRecord = GetARecord(address);
  MdnsRecord aRecord2 = GetARecord(address2);
  std::vector<MdnsRecord> record_set{aRecord};
  std::vector<MdnsRecord> record_set2{aRecord2};

  EXPECT_CALL(publisher, GetRecords(record)).WillOnce(Return(record_set));
  EXPECT_CALL(publisher.mdns_service(), RegisterRecord(aRecord)).Times(1);
  publisher.Register(record);

  EXPECT_CALL(publisher, GetRecords(record)).Times(0);
  EXPECT_CALL(publisher.mdns_service(), DeregisterRecord(aRecord2)).Times(0);
  publisher.DeregisterAll("_other-service._udp", "domain");
  publisher.DeregisterAll("_service._udp", "other.domain");

  EXPECT_CALL(publisher, GetRecords(record)).WillOnce(Return(record_set2));
  EXPECT_CALL(publisher.mdns_service(), DeregisterRecord(aRecord2)).Times(1);
  publisher.DeregisterAll("_service._udp", "domain");
}

}  // namespace discovery
}  // namespace openscreen
