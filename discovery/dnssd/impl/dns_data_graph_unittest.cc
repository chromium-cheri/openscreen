// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/dns_data_graph.h"

#include "discovery/mdns/testing/mdns_test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class DomainChangeImpl {
 public:
  MOCK_METHOD1(OnStartTracking, void(const DomainName&));
  MOCK_METHOD1(OnStopTracking, void(const DomainName&));
};

class DnsDataGraphTests : public testing::Test {
 public:
  DnsDataGraphTests() : graph_(network_interface_) {
    EXPECT_CALL(callbacks_, OnStartTracking(ptr_domain_));
    StartTracking(ptr_domain_);
    testing::Mock::VerifyAndClearExpectations(&callbacks_);
    EXPECT_EQ(graph_.tracked_domain_count(), size_t{1});
  }

  ~DnsDataGraphTests() {
    // The |graph_| object going out of scope causes a large number of deletion
    // callbacks that are unrelated to the ongoing test.
    graph_.on_node_deletion_ = [](const DomainName& domain) {};
  }

 protected:
  std::vector<MdnsRecord> GetRecords(const DomainName& domain) {
    auto it = graph_.nodes_.find(domain);
    if (it == graph_.nodes_.end()) {
      return {};
    }

    return it->second->records();
  }

  bool HasNode(const DomainName& domain) {
    return graph_.nodes_.find(domain) != graph_.nodes_.end();
  }

  bool ContainsRecordType(const DomainName& domain, DnsType type) {
    std::vector<MdnsRecord> records = GetRecords(domain);
    return std::find_if(records.begin(), records.end(),
                        [type](const MdnsRecord& record) {
                          return record.dns_type() == type;
                        }) != records.end();
  }

  void TriggerRecordCreation(MdnsRecord record,
                             Error::Code result_code = Error::Code::kNone) {
    size_t size = graph_.tracked_domain_count();
    Error result =
        ApplyDataRecordChange(std::move(record), RecordChangedEvent::kCreated);
    EXPECT_EQ(result.code(), result_code)
        << "Failed with error code " << result.code();
    size_t new_size = graph_.tracked_domain_count();
    EXPECT_EQ(size, new_size);
  }

  void TriggerRecordCreationWithCallback(MdnsRecord record,
                                         const DomainName& target_domain) {
    EXPECT_CALL(callbacks_, OnStartTracking(target_domain));
    size_t size = graph_.tracked_domain_count();
    Error result =
        ApplyDataRecordChange(std::move(record), RecordChangedEvent::kCreated);
    EXPECT_TRUE(result.ok()) << "Failed with error code " << result.code();
    size_t new_size = graph_.tracked_domain_count();
    EXPECT_EQ(size + 1, new_size);
  }

  void ExpectDomainEqual(const DnsSdInstance& instance,
                         const DomainName& name) {
    EXPECT_EQ(name.labels().size(), size_t{4});
    EXPECT_EQ(instance.instance_id(), name.labels()[0]);
    EXPECT_EQ(instance.service_id(), name.labels()[1] + "." + name.labels()[2]);
    EXPECT_EQ(instance.domain_id(), name.labels()[3]);
  }

  Error ApplyDataRecordChange(MdnsRecord record, RecordChangedEvent event) {
    return graph_.ApplyDataRecordChange(
        std::move(record), event,
        [this](const DomainName& domain) {
          callbacks_.OnStartTracking(domain);
        },
        [this](const DomainName& domain) {
          callbacks_.OnStopTracking(domain);
        });
  }

  void StartTracking(const DomainName& domain) {
    graph_.StartTracking(domain, [this](const DomainName& domain) {
      callbacks_.OnStartTracking(domain);
    });
  }

  void StopTracking(const DomainName& domain) {
    graph_.StopTracking(domain, [this](const DomainName& domain) {
      callbacks_.OnStopTracking(domain);
    });
  }

  StrictMock<DomainChangeImpl> callbacks_;
  NetworkInterfaceIndex network_interface_ = 1234;
  DnsDataGraph graph_;
  DomainName ptr_domain_{"_cast", "_tcp", "local"};
  DomainName primary_domain_{"test", "_cast", "_tcp", "local"};
  DomainName secondary_domain_{"test2", "_cast", "_tcp", "local"};
  DomainName tertiary_domain_{"test3", "_cast", "_tcp", "local"};
};

TEST_F(DnsDataGraphTests, CallbacksCalledForStartStopTracking) {
  EXPECT_CALL(callbacks_, OnStopTracking(ptr_domain_));
  StopTracking(ptr_domain_);

  EXPECT_EQ(graph_.tracked_domain_count(), size_t{0});
}

TEST_F(DnsDataGraphTests, ApplyChangeForUntrackedDomainError) {
  Error result = ApplyDataRecordChange(GetFakeSrvRecord(primary_domain_),
                                       RecordChangedEvent::kCreated);
  EXPECT_EQ(result.code(), Error::Code::kOperationCancelled);
  EXPECT_EQ(graph_.tracked_domain_count(), size_t{1});
}

TEST_F(DnsDataGraphTests, ChildrenStopTrackingWhenRootQueryStopped) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_, secondary_domain_);
  auto a = GetFakeARecord(secondary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreationWithCallback(srv, secondary_domain_);
  TriggerRecordCreation(a);

  EXPECT_CALL(callbacks_, OnStopTracking(ptr_domain_));
  EXPECT_CALL(callbacks_, OnStopTracking(primary_domain_));
  EXPECT_CALL(callbacks_, OnStopTracking(secondary_domain_));
  StopTracking(ptr_domain_);
  testing::Mock::VerifyAndClearExpectations(&callbacks_);

  EXPECT_EQ(graph_.tracked_domain_count(), size_t{0});
}

TEST_F(DnsDataGraphTests, ChildrenStopTrackingWhenParentDeleted) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_, secondary_domain_);
  auto a = GetFakeARecord(secondary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreationWithCallback(srv, secondary_domain_);
  TriggerRecordCreation(a);

  EXPECT_CALL(callbacks_, OnStopTracking(primary_domain_));
  EXPECT_CALL(callbacks_, OnStopTracking(secondary_domain_));
  auto result = ApplyDataRecordChange(ptr, RecordChangedEvent::kExpired);
  EXPECT_TRUE(result.ok()) << "Failed with error code " << result.code();
  testing::Mock::VerifyAndClearExpectations(&callbacks_);

  EXPECT_EQ(graph_.tracked_domain_count(), size_t{1});
}

TEST_F(DnsDataGraphTests, OnlyAffectedNodesChangedWhenParentDeleted) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_, secondary_domain_);
  auto a = GetFakeARecord(secondary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreationWithCallback(srv, secondary_domain_);
  TriggerRecordCreation(a);

  EXPECT_CALL(callbacks_, OnStopTracking(secondary_domain_));
  auto result = ApplyDataRecordChange(srv, RecordChangedEvent::kExpired);
  EXPECT_TRUE(result.ok()) << "Failed with error code " << result.code();
  testing::Mock::VerifyAndClearExpectations(&callbacks_);

  EXPECT_EQ(graph_.tracked_domain_count(), size_t{2});
}

TEST_F(DnsDataGraphTests, CreateFailsForExistingRecord) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreation(srv);

  auto result = ApplyDataRecordChange(srv, RecordChangedEvent::kCreated);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(graph_.tracked_domain_count(), size_t{2});
}

TEST_F(DnsDataGraphTests, UpdateFailsForNonExistingRecord) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);

  auto result = ApplyDataRecordChange(srv, RecordChangedEvent::kUpdated);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(graph_.tracked_domain_count(), size_t{2});
}

TEST_F(DnsDataGraphTests, DeleteFailsForNonExistingRecord) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);

  auto result = ApplyDataRecordChange(srv, RecordChangedEvent::kExpired);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(graph_.tracked_domain_count(), size_t{2});
}

TEST_F(DnsDataGraphTests, CreateEndpointsGeneratesCorrectRecords) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_, secondary_domain_);
  auto txt = GetFakeTxtRecord(primary_domain_);
  auto a = GetFakeARecord(secondary_domain_);
  auto aaaa = GetFakeAAAARecord(secondary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreation(txt);
  TriggerRecordCreationWithCallback(srv, secondary_domain_);

  ErrorOr<std::vector<DnsSdInstanceEndpoint>> endpoints =
      graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  EXPECT_EQ(endpoints.value().size(), size_t{0});

  TriggerRecordCreation(a);
  endpoints = graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{1});
  auto endpoint_a = std::move(endpoints.value()[0]);
  EXPECT_TRUE(endpoint_a.address_v4());
  EXPECT_FALSE(endpoint_a.address_v6());
  EXPECT_EQ(endpoint_a.address_v4(), kFakeARecordAddress);
  ExpectDomainEqual(endpoint_a, primary_domain_);
  EXPECT_EQ(endpoint_a.port(), kFakeSrvRecordPort);

  TriggerRecordCreation(aaaa);
  endpoints = graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{1});
  auto endpoint_a_aaaa = std::move(endpoints.value()[0]);
  ASSERT_TRUE(endpoint_a_aaaa.address_v4());
  ASSERT_TRUE(endpoint_a_aaaa.address_v6());
  EXPECT_EQ(endpoint_a_aaaa.address_v4(), kFakeARecordAddress);
  EXPECT_EQ(endpoint_a_aaaa.address_v6(), kFakeAAAARecordAddress);
  EXPECT_EQ(static_cast<DnsSdInstance>(endpoint_a),
            static_cast<DnsSdInstance>(endpoint_a_aaaa));

  endpoints = graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{1});
  auto endpoint_ptr = std::move(endpoints.value()[0]);
  EXPECT_EQ(endpoint_a_aaaa, endpoint_ptr);

  auto result = ApplyDataRecordChange(a, RecordChangedEvent::kExpired);
  EXPECT_TRUE(result.ok()) << "Failed with error code " << result.code();
  endpoints = graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{1});
  auto endpoint_aaaa = std::move(endpoints.value()[0]);
  EXPECT_FALSE(endpoint_aaaa.address_v4());
  ASSERT_TRUE(endpoint_aaaa.address_v6());
  EXPECT_EQ(endpoint_aaaa.address_v6(), kFakeAAAARecordAddress);
  EXPECT_EQ(static_cast<DnsSdInstance>(endpoint_a),
            static_cast<DnsSdInstance>(endpoint_aaaa));

  result = ApplyDataRecordChange(aaaa, RecordChangedEvent::kExpired);
  EXPECT_TRUE(result.ok()) << "Failed with error code " << result.code();
  endpoints = graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  EXPECT_EQ(endpoints.value().size(), size_t{0});
}

TEST_F(DnsDataGraphTests, CreateEndpointsHandlesSelfLoops) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_, primary_domain_);
  auto txt = GetFakeTxtRecord(primary_domain_);
  auto a = GetFakeARecord(primary_domain_);
  auto aaaa = GetFakeAAAARecord(primary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreation(srv);
  TriggerRecordCreation(txt);
  TriggerRecordCreation(a);
  TriggerRecordCreation(aaaa);

  auto endpoints = graph_.CreateEndpoints(GetDomainGroup(srv), primary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{1});
  DnsSdInstanceEndpoint endpoint = std::move(endpoints.value()[0]);

  EXPECT_EQ(endpoint.address_v4(), kFakeARecordAddress);
  EXPECT_EQ(endpoint.address_v6(), kFakeAAAARecordAddress);
  ExpectDomainEqual(endpoint, primary_domain_);
  EXPECT_EQ(endpoint.port(), kFakeSrvRecordPort);

  auto endpoints2 = graph_.CreateEndpoints(GetDomainGroup(ptr), ptr_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{1});
  DnsSdInstanceEndpoint endpoint2 = std::move(endpoints.value()[0]);
  EXPECT_EQ(endpoint, endpoint2);
}

TEST_F(DnsDataGraphTests, CreateEndpointsWithMultipleParents) {
  auto ptr = GetFakePtrRecord(primary_domain_);
  auto srv = GetFakeSrvRecord(primary_domain_, tertiary_domain_);
  auto txt = GetFakeTxtRecord(primary_domain_);
  auto ptr2 = GetFakePtrRecord(secondary_domain_);
  auto srv2 = GetFakeSrvRecord(secondary_domain_, tertiary_domain_);
  auto txt2 = GetFakeTxtRecord(secondary_domain_);
  auto a = GetFakeARecord(tertiary_domain_);
  auto aaaa = GetFakeAAAARecord(tertiary_domain_);

  TriggerRecordCreationWithCallback(ptr, primary_domain_);
  TriggerRecordCreationWithCallback(ptr2, secondary_domain_);
  TriggerRecordCreationWithCallback(srv, tertiary_domain_);
  TriggerRecordCreation(srv2);
  TriggerRecordCreation(txt);
  TriggerRecordCreation(txt2);
  TriggerRecordCreation(a);
  TriggerRecordCreation(aaaa);

  auto endpoints = graph_.CreateEndpoints(GetDomainGroup(a), tertiary_domain_);
  ASSERT_TRUE(endpoints.is_value());
  ASSERT_EQ(endpoints.value().size(), size_t{2});

  DnsSdInstanceEndpoint endpoint_a = std::move(endpoints.value()[0]);
  DnsSdInstanceEndpoint endpoint_b = std::move(endpoints.value()[1]);
  DnsSdInstanceEndpoint* endpoint_1;
  DnsSdInstanceEndpoint* endpoint_2;
  if (endpoint_a.instance_id() == "test") {
    endpoint_1 = &endpoint_a;
    endpoint_2 = &endpoint_b;
  } else {
    endpoint_2 = &endpoint_a;
    endpoint_1 = &endpoint_b;
  }

  EXPECT_EQ(endpoint_1->address_v4(), kFakeARecordAddress);
  EXPECT_EQ(endpoint_1->address_v6(), kFakeAAAARecordAddress);
  EXPECT_EQ(endpoint_1->port(), kFakeSrvRecordPort);
  ExpectDomainEqual(*endpoint_1, primary_domain_);

  EXPECT_EQ(endpoint_2->address_v4(), kFakeARecordAddress);
  EXPECT_EQ(endpoint_2->address_v6(), kFakeAAAARecordAddress);
  EXPECT_EQ(endpoint_2->port(), kFakeSrvRecordPort);
  ExpectDomainEqual(*endpoint_2, secondary_domain_);
}

}  // namespace discovery
}  // namespace openscreen
