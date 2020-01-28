// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/public/dns_sd_service_watcher.h"

#include <algorithm>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

using testing::_;
using testing::ContainerEq;
using testing::IsSubsetOf;
using testing::IsSupersetOf;
using testing::StrictMock;

namespace openscreen {
namespace discovery {
namespace {

static const IPAddress kAddressV4(192, 168, 0, 0);
static const IPEndpoint kEndpointV4{kAddressV4, 0};
static const std::string kCastServiceId = "_googlecast._tcp";
static const std::string kCastDomainId = "local";

class MockDnsSdService : public DnsSdService {
 public:
  MockDnsSdService() : querier_(this) {}

  DnsSdQuerier* GetQuerier() override { return &querier_; }
  DnsSdPublisher* GetPublisher() override { return nullptr; }

  MOCK_METHOD2(StartQuery,
               void(const std::string& service, DnsSdQuerier::Callback* cb));
  MOCK_METHOD2(StopQuery,
               void(const std::string& service, DnsSdQuerier::Callback* cb));
  MOCK_METHOD1(ReinitializeQueries, void(const std::string& service));

 private:
  class MockQuerier : public DnsSdQuerier {
   public:
    explicit MockQuerier(MockDnsSdService* service) : mock_service_(service) {
      OSP_DCHECK(service);
    }

    void StartQuery(const std::string& service,
                    DnsSdQuerier::Callback* cb) override {
      mock_service_->StartQuery(service, cb);
    }

    void StopQuery(const std::string& service,
                   DnsSdQuerier::Callback* cb) override {
      mock_service_->StopQuery(service, cb);
    }

    void ReinitializeQueries(const std::string& service) override {
      mock_service_->ReinitializeQueries(service);
    }

   private:
    MockDnsSdService* const mock_service_;
  };

  MockQuerier querier_;
};

}  // namespace

class TestServiceWatcher : public DnsSdServiceWatcher<std::string> {
 public:
  explicit TestServiceWatcher(MockDnsSdService* service)
      : DnsSdServiceWatcher<std::string>(
            service,
            &task_runner_,
            kCastServiceId,
            [this](const DnsSdInstanceRecord& record) {
              return Convert(record);
            },
            [this](std::vector<std::string> ref) { Callback(std::move(ref)); }),
        clock_(Clock::now()),
        task_runner_(&clock_) {}

  MOCK_METHOD1(Callback, void(std::vector<std::string>));

  using DnsSdServiceWatcher<std::string>::OnInstanceCreated;
  using DnsSdServiceWatcher<std::string>::OnInstanceUpdated;
  using DnsSdServiceWatcher<std::string>::OnInstanceDeleted;

  FakeTaskRunner& task_runner() { return task_runner_; }

 private:
  std::string Convert(const DnsSdInstanceRecord& record) {
    return record.instance_id();
  }

  FakeClock clock_;
  FakeTaskRunner task_runner_;
};

class DnsSdServiceWatcherTests : public testing::Test {
 public:
  DnsSdServiceWatcherTests() : watcher_(&service_) {
    // Start service discovery, since all other tests need it
    EXPECT_FALSE(watcher_.is_running());
    EXPECT_CALL(service_, StartQuery(kCastServiceId, _));
    watcher_.StartDiscovery();
    watcher_.task_runner().RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&service_);
  }

 protected:
  std::vector<std::string> GetServices() {
    EXPECT_EQ(watcher_.task_runner().ready_task_count(), size_t{0});
    std::vector<std::string> fetched_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &fetched_services](
                      std::vector<std::string> value) { *services = value; });
    watcher_.GetServices();
    watcher_.task_runner().RunTasksUntilIdle();
    return fetched_services;
  }

  void CreateNewInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before = GetServices();
    const size_t count = services_before.size();

    EXPECT_EQ(watcher_.task_runner().ready_task_count(), size_t{0});
    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<std::string> value) { *services = value; });
    watcher_.OnInstanceCreated(record);
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    std::vector<std::string> fetched_services = GetServices();
    EXPECT_EQ(fetched_services.size(), count + 1);

    EXPECT_THAT(fetched_services, IsSupersetOf(callbacked_services));
    EXPECT_THAT(fetched_services, IsSubsetOf(callbacked_services));
    EXPECT_THAT(fetched_services, IsSupersetOf(services_before));
  }

  void CreateExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before = GetServices();
    const size_t count = services_before.size();

    EXPECT_EQ(watcher_.task_runner().ready_task_count(), size_t{0});
    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<std::string> value) { *services = value; });
    watcher_.OnInstanceCreated(record);
    watcher_.task_runner().RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    std::vector<std::string> fetched_services = GetServices();
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(fetched_services, IsSupersetOf(callbacked_services));
    EXPECT_THAT(fetched_services, IsSubsetOf(callbacked_services));
    EXPECT_THAT(fetched_services, IsSupersetOf(services_before));
    EXPECT_THAT(fetched_services, IsSubsetOf(services_before));
  }

  void UpdateExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before = GetServices();
    const size_t count = services_before.size();

    EXPECT_EQ(watcher_.task_runner().ready_task_count(), size_t{0});
    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<std::string> value) { *services = value; });
    watcher_.OnInstanceUpdated(record);
    watcher_.task_runner().RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    std::vector<std::string> fetched_services = GetServices();
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(fetched_services, IsSupersetOf(callbacked_services));
    EXPECT_THAT(fetched_services, IsSubsetOf(callbacked_services));
    EXPECT_THAT(fetched_services, IsSupersetOf(services_before));
    EXPECT_THAT(fetched_services, IsSubsetOf(services_before));
  }

  void DeleteExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before = GetServices();
    const size_t count = services_before.size();

    EXPECT_EQ(watcher_.task_runner().ready_task_count(), size_t{0});
    std::vector<std::string> callbacked_services;
    EXPECT_CALL(watcher_, Callback(_))
        .WillOnce([services = &callbacked_services](
                      std::vector<std::string> value) { *services = value; });
    watcher_.OnInstanceDeleted(record);
    watcher_.task_runner().RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services = GetServices();
    EXPECT_EQ(fetched_services.size(), count - 1);
  }

  void UpdateNonExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before = GetServices();
    const size_t count = services_before.size();

    EXPECT_CALL(watcher_, Callback(_)).Times(0);
    watcher_.OnInstanceUpdated(record);
    watcher_.task_runner().RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services = GetServices();
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(services_before, IsSupersetOf(fetched_services));
    EXPECT_THAT(services_before, IsSubsetOf(fetched_services));
  }

  void DeleteNonExistingInstance(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services_before = GetServices();
    const size_t count = services_before.size();

    EXPECT_CALL(watcher_, Callback(_)).Times(0);
    watcher_.OnInstanceDeleted(record);
    watcher_.task_runner().RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&watcher_);

    const std::vector<std::string> fetched_services = GetServices();
    EXPECT_EQ(fetched_services.size(), count);

    EXPECT_THAT(services_before, IsSupersetOf(fetched_services));
    EXPECT_THAT(services_before, IsSubsetOf(fetched_services));
  }

  bool ContainsService(const DnsSdInstanceRecord& record) {
    const std::vector<std::string> services = GetServices();
    const std::string& service = record.instance_id();
    return std::find_if(services.begin(), services.end(),
                        [&service](const std::string& ref) {
                          return service == ref;
                        }) != services.end();
  }

  StrictMock<MockDnsSdService> service_;
  StrictMock<TestServiceWatcher> watcher_;
  std::vector<std::string> fetched_services;
};

TEST_F(DnsSdServiceWatcherTests, StartStopDiscoveryWorks) {
  EXPECT_TRUE(watcher_.is_running());
  EXPECT_CALL(service_, StopQuery(kCastServiceId, _));
  watcher_.StopDiscovery();
  EXPECT_FALSE(watcher_.is_running());
}

TEST(DnsSdServiceWatcherTest, RefreshFailsBeforeDiscoveryStarts) {
  StrictMock<MockDnsSdService> service;
  StrictMock<TestServiceWatcher> watcher(&service);
  EXPECT_FALSE(watcher.DiscoverNow().ok());
  EXPECT_FALSE(watcher.ForceRefresh().ok());
}

TEST_F(DnsSdServiceWatcherTests, RefreshDiscoveryWorks) {
  const DnsSdInstanceRecord record("Instance", kCastServiceId, kCastDomainId,
                                   kEndpointV4, DnsSdTxtRecord{});
  CreateNewInstance(record);

  // Refresh services.
  EXPECT_CALL(service_, ReinitializeQueries(kCastServiceId));
  EXPECT_TRUE(watcher_.DiscoverNow().ok());
  EXPECT_EQ(GetServices().size(), size_t{1});
  testing::Mock::VerifyAndClearExpectations(&service_);

  EXPECT_CALL(service_, ReinitializeQueries(kCastServiceId));
  EXPECT_TRUE(watcher_.ForceRefresh().ok());
  EXPECT_EQ(GetServices().size(), size_t{0});
  testing::Mock::VerifyAndClearExpectations(&service_);
}

TEST_F(DnsSdServiceWatcherTests, CreatingUpdatingDeletingInstancesWork) {
  const DnsSdInstanceRecord record("Instance", kCastServiceId, kCastDomainId,
                                   kEndpointV4, DnsSdTxtRecord{});
  const DnsSdInstanceRecord record2("Instance2", kCastServiceId, kCastDomainId,
                                    kEndpointV4, DnsSdTxtRecord{});

  EXPECT_FALSE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  CreateNewInstance(record);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  CreateExistingInstance(record);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  UpdateNonExistingInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  DeleteNonExistingInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));

  CreateNewInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  UpdateExistingInstance(record2);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  UpdateExistingInstance(record);
  EXPECT_TRUE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  DeleteExistingInstance(record);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  UpdateNonExistingInstance(record);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  DeleteNonExistingInstance(record);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_TRUE(ContainsService(record2));

  DeleteExistingInstance(record2);
  EXPECT_FALSE(ContainsService(record));
  EXPECT_FALSE(ContainsService(record2));
}

}  // namespace discovery
}  // namespace openscreen
