// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe_manager.h"

#include "discovery/mdns/mdns_probe.h"
#include "discovery/mdns/mdns_querier.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_sender.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace openscreen {
namespace discovery {

class MockDomainConfirmedProvider : public MdnsDomainConfirmedProvider {
 public:
  MOCK_METHOD2(OnDomainFound, void(const DomainName&, const DomainName&));
};

class MockMdnsSender : public MdnsSender {
 public:
  MockMdnsSender(UdpSocket* socket) : MdnsSender(socket) {}

  MOCK_METHOD1(SendMulticast, Error(const MdnsMessage& message));
  MOCK_METHOD2(SendUnicast,
               Error(const MdnsMessage& message, const IPEndpoint& endpoint));
};

class MockMdnsProbe : public MdnsProbe {
 public:
  MockMdnsProbe(DomainName target_name, IPEndpoint endpoint)
      : MdnsProbe(std::move(target_name), std::move(endpoint)) {}

  MOCK_METHOD1(Postpone, void(std::chrono::seconds));
};

class TestMdnsProbeManager : public MdnsProbeManagerImpl {
 public:
  using MdnsProbeManagerImpl::MdnsProbeManagerImpl;

  using MdnsProbeManagerImpl::CreateAddressRecord;
  using MdnsProbeManagerImpl::OnProbeFailure;
  using MdnsProbeManagerImpl::OnProbeSuccess;

  std::unique_ptr<MdnsProbe> CreateProbe(DomainName name,
                                         IPEndpoint endpoint) override {
    return std::make_unique<StrictMock<MockMdnsProbe>>(std::move(name),
                                                       std::move(endpoint));
  }

  StrictMock<MockMdnsProbe>* GetOngoingProbe(const DomainName& target) {
    const auto it =
        std::find_if(ongoing_probes_.begin(), ongoing_probes_.end(),
                     [&target](const OngoingProbe& ongoing) {
                       return ongoing.probe->target_name() == target;
                     });
    if (it != ongoing_probes_.end()) {
      return static_cast<StrictMock<MockMdnsProbe>*>(it->probe.get());
    }
    return nullptr;
  }

  StrictMock<MockMdnsProbe>* GetCompletedProbe(const DomainName& target) {
    const auto it =
        std::find_if(completed_probes_.begin(), completed_probes_.end(),
                     [&target](const std::unique_ptr<MdnsProbe>& completed) {
                       return completed->target_name() == target;
                     });
    if (it != completed_probes_.end()) {
      return static_cast<StrictMock<MockMdnsProbe>*>(it->get());
    }
    return nullptr;
  }

  StrictMock<MockMdnsProbe>* GetProbe(const DomainName& target) {
    StrictMock<MockMdnsProbe>* ongoing = GetOngoingProbe(target);
    StrictMock<MockMdnsProbe>* completed = GetCompletedProbe(target);
    EXPECT_TRUE(ongoing == nullptr || completed == nullptr);

    return ongoing ? ongoing : completed;
  }

  bool HasOngoingProbe(const DomainName& target) {
    return GetOngoingProbe(target) != nullptr;
  }

  bool HasCompletedProbe(const DomainName& target) {
    return GetCompletedProbe(target) != nullptr;
  }

  bool HasProbe(const DomainName& target) {
    return GetProbe(target) != nullptr;
  }

  void MarkProbeComplete(DomainName domain, IPEndpoint endpoint) {
    std::unique_ptr<MdnsProbe> probe =
        CreateProbe(std::move(domain), std::move(endpoint));
    completed_probes_.push_back(std::move(probe));
  }

  size_t GetOngoingProbeCount() { return ongoing_probes_.size(); }

  size_t GetCompletedProbeCount() { return completed_probes_.size(); }
};

class MdnsProbeManagerTests : public testing::Test {
 public:
  MdnsProbeManagerTests()
      : socket_(FakeUdpSocket::CreateDefault()),
        clock_(Clock::now()),
        task_runner_(&clock_),
        sender_(socket_.get()),
        receiver_(socket_.get()),
        querier_(&sender_, &receiver_, &task_runner_, FakeClock::now, &random_),
        manager_(&sender_, &querier_, &random_, &task_runner_) {}

 protected:
  MdnsMessage CreateProbeQueryMessage(DomainName domain,
                                      const IPEndpoint& endpoint) {
    MdnsMessage message(CreateMessageId(), MessageType::Query);
    MdnsQuestion question(domain, DnsType::kANY, DnsClass::kANY,
                          ResponseType::kUnicast);
    MdnsRecord record =
        manager_.CreateAddressRecord(std::move(domain), endpoint);
    message.AddQuestion(std::move(question));
    message.AddAuthorityRecord(std::move(record));
    return message;
  }

  std::unique_ptr<FakeUdpSocket> socket_;
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  StrictMock<MockMdnsSender> sender_;
  MdnsReceiver receiver_;
  MdnsRandom random_;
  MdnsQuerier querier_;
  TestMdnsProbeManager manager_;
  MockDomainConfirmedProvider callback_;

  const DomainName name_{"test", "_googlecast", "_tcp", "local"};
  const DomainName name_retry_{"test1", "_googlecast", "_tcp", "local"};
  const DomainName name2_{"test2", "_googlecast", "_tcp", "local"};

  const IPAddress address_v4_{192, 168, 0, 0};
  const IPEndpoint endpoint_v4_{address_v4_, 80};

  const IPAddress address_v4_earlier_{190, 160, 0, 0};
  const IPEndpoint endpoint_v4_earlier_{address_v4_earlier_, 60};

  const IPAddress address_v6_{0x0102, 0x0304, 0x0506, 0x0708,
                              0x090a, 0x0b0c, 0x0d0e, 0x0f10};
  const IPEndpoint endpoint_v6_{address_v6_, 3030};
};

TEST_F(MdnsProbeManagerTests, StartProbeBeginsProbeWhenNoneExistsOnly) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));

  EXPECT_FALSE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_CALL(callback_, OnDomainFound(name_, name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  manager_.OnProbeSuccess(ongoing_probe);
  testing::Mock::VerifyAndClearExpectations(ongoing_probe);

  EXPECT_FALSE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  ASSERT_TRUE(manager_.HasCompletedProbe(name_));
  StrictMock<MockMdnsProbe>* completed_probe =
      manager_.GetCompletedProbe(name_);
  EXPECT_EQ(ongoing_probe, completed_probe);
}

TEST_F(MdnsProbeManagerTests, StopProbeChangesOngoingProbesOnly) {
  EXPECT_FALSE(manager_.StopProbe(name_).ok());
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));

  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));

  EXPECT_TRUE(manager_.StopProbe(name_).ok());
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));

  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_CALL(callback_, OnDomainFound(name_, name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  manager_.OnProbeSuccess(ongoing_probe);
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  ASSERT_TRUE(manager_.HasCompletedProbe(name_));
  testing::Mock::VerifyAndClearExpectations(ongoing_probe);

  EXPECT_FALSE(manager_.StopProbe(name_).ok());
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  EXPECT_TRUE(manager_.HasCompletedProbe(name_));
}

TEST_F(MdnsProbeManagerTests, RespondToProbeQuerySendsNothingOnNoOwnedDomain) {
  const MdnsMessage query = CreateProbeQueryMessage(name_, endpoint_v6_);
  manager_.RespondToProbeQuery(query, endpoint_v4_);
}

TEST_F(MdnsProbeManagerTests, RespondToProbeQueryWorksForCompletedProbes) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_CALL(callback_, OnDomainFound(name_, name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  manager_.OnProbeSuccess(ongoing_probe);
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  ASSERT_TRUE(manager_.HasCompletedProbe(name_));
  testing::Mock::VerifyAndClearExpectations(ongoing_probe);

  const MdnsMessage query = CreateProbeQueryMessage(name_, endpoint_v6_);
  EXPECT_CALL(sender_, SendUnicast(_, endpoint_v6_))
      .WillOnce([this](const MdnsMessage& message,
                       const IPEndpoint& endpoint) -> Error {
        EXPECT_EQ(message.answers().size(), size_t{1});
        EXPECT_EQ(message.answers()[0].dns_type(), DnsType::kA);
        EXPECT_EQ(message.answers()[0].name(), this->name_);
        return Error::None();
      });
  manager_.RespondToProbeQuery(query, endpoint_v6_);
}

TEST_F(MdnsProbeManagerTests, TiebreakProbeQueryWorksForAllReceivedRecords) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);

  MdnsMessage query = CreateProbeQueryMessage(name_, endpoint_v4_earlier_);
  manager_.RespondToProbeQuery(query, endpoint_v4_earlier_);

  query = CreateProbeQueryMessage(name_, endpoint_v6_);
  EXPECT_CALL(*ongoing_probe, Postpone(_)).Times(1);
  manager_.RespondToProbeQuery(query, endpoint_v6_);
}

TEST_F(MdnsProbeManagerTests, IsDomainClaimedOnlyTrueAfterProbeSucceeds) {
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_FALSE(manager_.IsDomainClaimed(name_));
  EXPECT_FALSE(manager_.IsDomainClaimed(name2_));

  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_FALSE(manager_.IsDomainClaimed(name_));
  EXPECT_FALSE(manager_.IsDomainClaimed(name2_));

  EXPECT_FALSE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_CALL(callback_, OnDomainFound(name_, name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  manager_.OnProbeSuccess(ongoing_probe);
  EXPECT_TRUE(manager_.IsDomainClaimed(name_));
  EXPECT_FALSE(manager_.IsDomainClaimed(name2_));
  testing::Mock::VerifyAndClearExpectations(ongoing_probe);
}

TEST_F(MdnsProbeManagerTests, ProbeSuccessAfterProbeRemovalNoOp) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));

  EXPECT_FALSE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  EXPECT_TRUE(manager_.StopProbe(name_).ok());
  manager_.OnProbeSuccess(ongoing_probe);
}

TEST_F(MdnsProbeManagerTests, ProbeFailureAfterProbeRemovalNoOp) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));

  EXPECT_FALSE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  EXPECT_TRUE(manager_.StopProbe(name_).ok());
  manager_.OnProbeFailure(ongoing_probe);
}

TEST_F(MdnsProbeManagerTests, ProbeFailureCallsCallbackWhenAlreadyClaimed) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_retry_, endpoint_v4_).ok());
  ASSERT_TRUE(manager_.HasOngoingProbe(name_retry_));
  EXPECT_CALL(callback_, OnDomainFound(name_retry_, name_retry_));
  StrictMock<MockMdnsProbe>* ongoing_probe =
      manager_.GetOngoingProbe(name_retry_);
  manager_.OnProbeSuccess(ongoing_probe);
  ASSERT_TRUE(manager_.HasCompletedProbe(name_retry_));
  testing::Mock::VerifyAndClearExpectations(ongoing_probe);

  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  ongoing_probe = manager_.GetOngoingProbe(name_);
  EXPECT_CALL(callback_, OnDomainFound(name_, name_retry_));
  manager_.OnProbeFailure(ongoing_probe);
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_TRUE(manager_.HasCompletedProbe(name_retry_));
}

TEST_F(MdnsProbeManagerTests, ProbeFailureCreatesNewProbeIfNameUnclaimed) {
  EXPECT_TRUE(manager_.StartProbe(&callback_, name_, endpoint_v4_).ok());
  EXPECT_TRUE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  StrictMock<MockMdnsProbe>* ongoing_probe = manager_.GetOngoingProbe(name_);
  manager_.OnProbeFailure(ongoing_probe);
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  ASSERT_TRUE(manager_.HasOngoingProbe(name_retry_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_retry_));
  ongoing_probe = manager_.GetOngoingProbe(name_retry_);
  EXPECT_EQ(ongoing_probe->target_name(), name_retry_);

  EXPECT_CALL(callback_, OnDomainFound(name_, name_retry_));
  manager_.OnProbeSuccess(ongoing_probe);
  EXPECT_FALSE(manager_.HasOngoingProbe(name_));
  EXPECT_FALSE(manager_.HasCompletedProbe(name_));
  EXPECT_TRUE(manager_.HasCompletedProbe(name_retry_));
}

}  // namespace discovery
}  // namespace openscreen
