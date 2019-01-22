// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/receiver_listener_impl.h"

#include "base/make_unique.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Expectation;

using State = ReceiverListener::State;

class MockObserver final : public ReceiverListener::Observer {
 public:
  ~MockObserver() = default;

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());
  MOCK_METHOD0(OnSearching, void());

  MOCK_METHOD1(OnReceiverAdded, void(const ReceiverInfo& info));
  MOCK_METHOD1(OnReceiverChanged, void(const ReceiverInfo& info));
  MOCK_METHOD1(OnReceiverRemoved, void(const ReceiverInfo& info));
  MOCK_METHOD0(OnAllReceiversRemoved, void());

  MOCK_METHOD1(OnError, void(ReceiverListenerError));

  MOCK_METHOD1(OnMetrics, void(ReceiverListener::Metrics));
};

class MockMdnsDelegate final : public ReceiverListenerImpl::Delegate {
 public:
  MockMdnsDelegate() = default;
  ~MockMdnsDelegate() override = default;

  using ReceiverListenerImpl::Delegate::SetState;

  MOCK_METHOD0(StartListener, void());
  MOCK_METHOD0(StartAndSuspendListener, void());
  MOCK_METHOD0(StopListener, void());
  MOCK_METHOD0(SuspendListener, void());
  MOCK_METHOD0(ResumeListener, void());
  MOCK_METHOD1(SearchNow, void(State));
};

class ReceiverListenerImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    receiver_listener_ =
        MakeUnique<ReceiverListenerImpl>(nullptr, &mock_delegate_);
  }

  MockMdnsDelegate mock_delegate_;
  std::unique_ptr<ReceiverListenerImpl> receiver_listener_;
};

}  // namespace

TEST_F(ReceiverListenerImplTest, NormalStartStop) {
  ASSERT_EQ(State::kStopped, receiver_listener_->state());

  EXPECT_CALL(mock_delegate_, StartListener());
  EXPECT_TRUE(receiver_listener_->Start());
  EXPECT_FALSE(receiver_listener_->Start());
  EXPECT_EQ(State::kStarting, receiver_listener_->state());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, receiver_listener_->state());

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(receiver_listener_->Stop());
  EXPECT_FALSE(receiver_listener_->Stop());
  EXPECT_EQ(State::kStopping, receiver_listener_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, StopBeforeRunning) {
  EXPECT_CALL(mock_delegate_, StartListener());
  EXPECT_TRUE(receiver_listener_->Start());
  EXPECT_EQ(State::kStarting, receiver_listener_->state());

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(receiver_listener_->Stop());
  EXPECT_FALSE(receiver_listener_->Stop());
  EXPECT_EQ(State::kStopping, receiver_listener_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, StartSuspended) {
  EXPECT_CALL(mock_delegate_, StartAndSuspendListener());
  EXPECT_CALL(mock_delegate_, StartListener()).Times(0);
  EXPECT_TRUE(receiver_listener_->StartAndSuspend());
  EXPECT_FALSE(receiver_listener_->Start());
  EXPECT_EQ(State::kStarting, receiver_listener_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, SuspendWhileStarting) {
  EXPECT_CALL(mock_delegate_, StartListener()).Times(1);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(1);
  EXPECT_TRUE(receiver_listener_->Start());
  EXPECT_TRUE(receiver_listener_->Suspend());
  EXPECT_EQ(State::kStarting, receiver_listener_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, SuspendAndResume) {
  EXPECT_TRUE(receiver_listener_->Start());
  mock_delegate_.SetState(State::kRunning);

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(2);
  EXPECT_FALSE(receiver_listener_->Resume());
  EXPECT_TRUE(receiver_listener_->Suspend());
  EXPECT_TRUE(receiver_listener_->Suspend());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, receiver_listener_->state());

  EXPECT_CALL(mock_delegate_, StartListener()).Times(0);
  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(0);
  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(2);
  EXPECT_FALSE(receiver_listener_->Start());
  EXPECT_FALSE(receiver_listener_->Suspend());
  EXPECT_TRUE(receiver_listener_->Resume());
  EXPECT_TRUE(receiver_listener_->Resume());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, receiver_listener_->state());

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(0);
  EXPECT_FALSE(receiver_listener_->Resume());
}

TEST_F(ReceiverListenerImplTest, SearchWhileRunning) {
  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(receiver_listener_->SearchNow());
  EXPECT_TRUE(receiver_listener_->Start());
  mock_delegate_.SetState(State::kRunning);

  EXPECT_CALL(mock_delegate_, SearchNow(State::kRunning)).Times(2);
  EXPECT_TRUE(receiver_listener_->SearchNow());
  EXPECT_TRUE(receiver_listener_->SearchNow());

  mock_delegate_.SetState(State::kSearching);
  EXPECT_EQ(State::kSearching, receiver_listener_->state());

  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(receiver_listener_->SearchNow());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, SearchWhileSuspended) {
  EXPECT_CALL(mock_delegate_, SearchNow(_)).Times(0);
  EXPECT_FALSE(receiver_listener_->SearchNow());
  EXPECT_TRUE(receiver_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(receiver_listener_->Suspend());
  mock_delegate_.SetState(State::kSuspended);

  EXPECT_CALL(mock_delegate_, SearchNow(State::kSuspended)).Times(2);
  EXPECT_TRUE(receiver_listener_->SearchNow());
  EXPECT_TRUE(receiver_listener_->SearchNow());

  mock_delegate_.SetState(State::kSearching);
  EXPECT_EQ(State::kSearching, receiver_listener_->state());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, StopWhileSearching) {
  EXPECT_TRUE(receiver_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(receiver_listener_->SearchNow());
  mock_delegate_.SetState(State::kSearching);

  EXPECT_CALL(mock_delegate_, StopListener());
  EXPECT_TRUE(receiver_listener_->Stop());
  EXPECT_FALSE(receiver_listener_->Stop());
  EXPECT_EQ(State::kStopping, receiver_listener_->state());

  mock_delegate_.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, ResumeWhileSearching) {
  EXPECT_TRUE(receiver_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(receiver_listener_->Suspend());
  mock_delegate_.SetState(State::kSuspended);
  EXPECT_TRUE(receiver_listener_->SearchNow());
  mock_delegate_.SetState(State::kSearching);

  EXPECT_CALL(mock_delegate_, ResumeListener()).Times(2);
  EXPECT_TRUE(receiver_listener_->Resume());
  EXPECT_TRUE(receiver_listener_->Resume());

  mock_delegate_.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, SuspendWhileSearching) {
  EXPECT_TRUE(receiver_listener_->Start());
  mock_delegate_.SetState(State::kRunning);
  EXPECT_TRUE(receiver_listener_->SearchNow());
  mock_delegate_.SetState(State::kSearching);

  EXPECT_CALL(mock_delegate_, SuspendListener()).Times(2);
  EXPECT_TRUE(receiver_listener_->Suspend());
  EXPECT_TRUE(receiver_listener_->Suspend());

  mock_delegate_.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, receiver_listener_->state());
}

TEST_F(ReceiverListenerImplTest, ObserveTransitions) {
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  receiver_listener_ =
      MakeUnique<ReceiverListenerImpl>(&observer, &mock_delegate);

  receiver_listener_->Start();
  Expectation start_from_stopped = EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);

  receiver_listener_->SearchNow();
  Expectation search_from_running =
      EXPECT_CALL(observer, OnSearching()).After(start_from_stopped);
  mock_delegate.SetState(State::kSearching);
  EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);

  receiver_listener_->Suspend();
  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(search_from_running);
  mock_delegate.SetState(State::kSuspended);

  receiver_listener_->SearchNow();
  Expectation search_from_suspended =
      EXPECT_CALL(observer, OnSearching()).After(suspend_from_running);
  mock_delegate.SetState(State::kSearching);
  EXPECT_CALL(observer, OnSuspended());
  mock_delegate.SetState(State::kSuspended);

  receiver_listener_->Resume();
  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnStarted()).After(suspend_from_running);
  mock_delegate.SetState(State::kRunning);

  receiver_listener_->Stop();
  Expectation stop =
      EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  mock_delegate.SetState(State::kStopped);

  receiver_listener_->StartAndSuspend();
  EXPECT_CALL(observer, OnSuspended()).After(stop);
  mock_delegate.SetState(State::kSuspended);
}

TEST_F(ReceiverListenerImplTest, ObserveFromSearching) {
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  receiver_listener_ =
      MakeUnique<ReceiverListenerImpl>(&observer, &mock_delegate);

  receiver_listener_->Start();
  mock_delegate.SetState(State::kRunning);

  receiver_listener_->SearchNow();
  mock_delegate.SetState(State::kSearching);

  receiver_listener_->Suspend();
  EXPECT_CALL(observer, OnSuspended());
  mock_delegate.SetState(State::kSuspended);

  EXPECT_TRUE(receiver_listener_->SearchNow());
  mock_delegate.SetState(State::kSearching);

  receiver_listener_->Resume();
  EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);
}

TEST_F(ReceiverListenerImplTest, ReceiverObserverPassThrough) {
  const ReceiverInfo receiver1{
      "id1", "name1", 1, {{192, 168, 1, 10}, 12345}, {}};
  const ReceiverInfo receiver2{
      "id2", "name2", 1, {{192, 168, 1, 11}, 12345}, {}};
  const ReceiverInfo receiver3{
      "id3", "name3", 1, {{192, 168, 1, 12}, 12345}, {}};
  const ReceiverInfo receiver1_alt_name{
      "id1", "name1 alt", 1, {{192, 168, 1, 10}, 12345}, {}};
  MockObserver observer;
  MockMdnsDelegate mock_delegate;
  receiver_listener_ =
      MakeUnique<ReceiverListenerImpl>(&observer, &mock_delegate);

  EXPECT_CALL(observer, OnReceiverAdded(receiver1));
  receiver_listener_->OnReceiverAdded(receiver1);
  EXPECT_THAT(receiver_listener_->GetReceivers(), ElementsAre(receiver1));

  EXPECT_CALL(observer, OnReceiverChanged(receiver1_alt_name));
  receiver_listener_->OnReceiverChanged(receiver1_alt_name);
  EXPECT_THAT(receiver_listener_->GetReceivers(),
              ElementsAre(receiver1_alt_name));

  EXPECT_CALL(observer, OnReceiverChanged(receiver2)).Times(0);
  receiver_listener_->OnReceiverChanged(receiver2);

  EXPECT_CALL(observer, OnReceiverRemoved(receiver1_alt_name));
  receiver_listener_->OnReceiverRemoved(receiver1_alt_name);
  EXPECT_TRUE(receiver_listener_->GetReceivers().empty());

  EXPECT_CALL(observer, OnReceiverRemoved(receiver1_alt_name)).Times(0);
  receiver_listener_->OnReceiverRemoved(receiver1_alt_name);

  EXPECT_CALL(observer, OnReceiverAdded(receiver2));
  receiver_listener_->OnReceiverAdded(receiver2);
  EXPECT_THAT(receiver_listener_->GetReceivers(), ElementsAre(receiver2));

  EXPECT_CALL(observer, OnReceiverAdded(receiver3));
  receiver_listener_->OnReceiverAdded(receiver3);
  EXPECT_THAT(receiver_listener_->GetReceivers(),
              ElementsAre(receiver2, receiver3));

  EXPECT_CALL(observer, OnAllReceiversRemoved());
  receiver_listener_->OnAllReceiversRemoved();
  EXPECT_TRUE(receiver_listener_->GetReceivers().empty());

  EXPECT_CALL(observer, OnAllReceiversRemoved()).Times(0);
  receiver_listener_->OnAllReceiversRemoved();
}

}  // namespace openscreen
