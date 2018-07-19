// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder/screen_listener_impl.h"

#include "base/make_unique.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Expectation;

using StateTransition = ScreenListenerImpl::StateTransition;

class MockObserver final : public ScreenListenerObserver {
 public:
  ~MockObserver() = default;

  MOCK_METHOD0(OnRunning, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());
  MOCK_METHOD0(OnSearching, void());

  MOCK_METHOD1(OnScreenAdded, void(const ScreenInfo& info));
  MOCK_METHOD1(OnScreenChanged, void(const ScreenInfo& info));
  MOCK_METHOD1(OnScreenRemoved, void(const ScreenInfo& info));
  MOCK_METHOD0(OnAllScreensRemoved, void());

  MOCK_METHOD1(OnError, void(ScreenListenerError));

  MOCK_METHOD1(OnMetrics, void(ScreenListenerMetrics));
};

}  // namespace

TEST(ScreenListenerImplTest, NormalStartStop) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  ASSERT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
  EXPECT_TRUE(screen_listener->Start());
  EXPECT_FALSE(screen_listener->Start());
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(), ElementsAre());
  screen_listener->SetState(ScreenListenerState::STARTING);
  EXPECT_EQ(ScreenListenerState::STARTING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());

  EXPECT_TRUE(screen_listener->Stop());
  EXPECT_FALSE(screen_listener->Stop());
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_EQ(ScreenListenerState::STOPPING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::STOPPED);
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
}

TEST(ScreenListenerImplTest, BatchStartStop) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  ASSERT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
  EXPECT_TRUE(screen_listener->Start());
  EXPECT_FALSE(screen_listener->Start());
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
  EXPECT_TRUE(screen_listener->Stop());
  EXPECT_FALSE(screen_listener->Stop());
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart, StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STARTING);
  EXPECT_EQ(ScreenListenerState::STARTING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_EQ(ScreenListenerState::STOPPING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::STOPPED);
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
}

TEST(ScreenListenerImplTest, StopBeforeRunning) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->Start());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  EXPECT_EQ(ScreenListenerState::STARTING, screen_listener->state());

  EXPECT_TRUE(screen_listener->Stop());
  EXPECT_FALSE(screen_listener->Stop());
  EXPECT_EQ(ScreenListenerState::STARTING, screen_listener->state());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_EQ(ScreenListenerState::STOPPING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::STOPPED);
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
}

TEST(ScreenListenerImplTest, StartSuspended) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->StartAndSuspend());
  EXPECT_FALSE(screen_listener->Start());
  EXPECT_FALSE(screen_listener->Suspend());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStartSuspended));
  screen_listener->SetState(ScreenListenerState::STARTING);
  EXPECT_EQ(ScreenListenerState::STARTING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());

  EXPECT_FALSE(screen_listener->StartAndSuspend());
  EXPECT_FALSE(screen_listener->Start());
  EXPECT_FALSE(screen_listener->Suspend());
}

TEST(ScreenListenerImplTest, SuspendAndResume) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_FALSE(screen_listener->Resume());
  EXPECT_TRUE(screen_listener->Suspend());
  EXPECT_FALSE(screen_listener->Suspend());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());

  EXPECT_FALSE(screen_listener->Start());
  EXPECT_FALSE(screen_listener->Suspend());
  EXPECT_TRUE(screen_listener->Resume());
  EXPECT_FALSE(screen_listener->Resume());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());
}

TEST(ScreenListenerImplTest, SearchWhileRunning) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_FALSE(screen_listener->SearchNow());
  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_TRUE(screen_listener->SearchNow());
  EXPECT_FALSE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());
}

TEST(ScreenListenerImplTest, SearchWhileSuspended) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_FALSE(screen_listener->SearchNow());
  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_FALSE(screen_listener->Resume());
  EXPECT_TRUE(screen_listener->Suspend());
  EXPECT_FALSE(screen_listener->Suspend());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());

  EXPECT_TRUE(screen_listener->SearchNow());
  EXPECT_FALSE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());
}

TEST(ScreenListenerImplTest, StopWhileSearching) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_TRUE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  EXPECT_TRUE(screen_listener->Stop());
  EXPECT_FALSE(screen_listener->Stop());

  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_EQ(ScreenListenerState::STOPPING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::STOPPED);
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
}

TEST(ScreenListenerImplTest, StopWhileSearchingImmediate) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_TRUE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  EXPECT_TRUE(screen_listener->Stop());
  EXPECT_FALSE(screen_listener->Stop());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_EQ(ScreenListenerState::STOPPING, screen_listener->state());
  screen_listener->SetState(ScreenListenerState::STOPPED);
  EXPECT_EQ(ScreenListenerState::STOPPED, screen_listener->state());
}

TEST(ScreenListenerImplTest, ResumeWhileSearching) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_FALSE(screen_listener->SearchNow());
  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_FALSE(screen_listener->Resume());
  EXPECT_TRUE(screen_listener->Suspend());
  EXPECT_FALSE(screen_listener->Suspend());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());

  EXPECT_TRUE(screen_listener->SearchNow());
  EXPECT_FALSE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  EXPECT_TRUE(screen_listener->Resume());
  EXPECT_FALSE(screen_listener->Resume());

  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());
}

TEST(ScreenListenerImplTest, ResumeWhileSearchingImmediate) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_FALSE(screen_listener->SearchNow());
  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_FALSE(screen_listener->Resume());
  EXPECT_TRUE(screen_listener->Suspend());
  EXPECT_FALSE(screen_listener->Suspend());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());

  EXPECT_TRUE(screen_listener->SearchNow());
  EXPECT_FALSE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  EXPECT_TRUE(screen_listener->Resume());
  EXPECT_FALSE(screen_listener->Resume());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());
}

TEST(ScreenListenerImplTest, SuspendWhileSearching) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_TRUE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  EXPECT_FALSE(screen_listener->Resume());
  EXPECT_TRUE(screen_listener->Suspend());
  EXPECT_FALSE(screen_listener->Suspend());

  screen_listener->SetState(ScreenListenerState::RUNNING);
  EXPECT_EQ(ScreenListenerState::RUNNING, screen_listener->state());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());
}

TEST(ScreenListenerImplTest, SuspendWhileSearchingImmediate) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);

  EXPECT_TRUE(screen_listener->Start());
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  EXPECT_TRUE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_EQ(ScreenListenerState::SEARCHING, screen_listener->state());

  EXPECT_FALSE(screen_listener->Resume());
  EXPECT_TRUE(screen_listener->Suspend());
  EXPECT_FALSE(screen_listener->Suspend());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  screen_listener->SetState(ScreenListenerState::SUSPENDED);
  EXPECT_EQ(ScreenListenerState::SUSPENDED, screen_listener->state());
}

TEST(ScreenListenerImplTest, ObserveTransitions) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);
  MockObserver observer;
  screen_listener->SetObserver(&observer);

  screen_listener->Start();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  Expectation start_from_stopped = EXPECT_CALL(observer, OnRunning());
  screen_listener->SetState(ScreenListenerState::RUNNING);

  screen_listener->SearchNow();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  Expectation search_from_running =
      EXPECT_CALL(observer, OnSearching()).After(start_from_stopped);
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_CALL(observer, OnRunning()).Times(0);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  screen_listener->Suspend();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(search_from_running);
  screen_listener->SetState(ScreenListenerState::SUSPENDED);

  screen_listener->SearchNow();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  Expectation search_from_suspended =
      EXPECT_CALL(observer, OnSearching()).After(suspend_from_running);
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_CALL(observer, OnSuspended()).Times(0);
  screen_listener->SetState(ScreenListenerState::SUSPENDED);

  screen_listener->Resume();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnRunning()).After(suspend_from_running);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  screen_listener->Stop();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  screen_listener->SetState(ScreenListenerState::STOPPED);
}

TEST(ScreenListenerImplTest, ObserveBatchTransitions) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);
  MockObserver observer;
  screen_listener->SetObserver(&observer);

  screen_listener->Start();
  screen_listener->SearchNow();
  screen_listener->Suspend();
  screen_listener->SearchNow();
  screen_listener->Resume();
  screen_listener->Stop();

  EXPECT_THAT(
      screen_listener->TakeNewStateTransitions(),
      ElementsAre(StateTransition::kStart, StateTransition::kSearchNow,
                  StateTransition::kSuspend, StateTransition::kSearchNow,
                  StateTransition::kResume, StateTransition::kStop));
  screen_listener->SetState(ScreenListenerState::STARTING);
  Expectation start_from_stopped = EXPECT_CALL(observer, OnRunning());
  screen_listener->SetState(ScreenListenerState::RUNNING);

  Expectation search_from_running =
      EXPECT_CALL(observer, OnSearching()).After(start_from_stopped);
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_CALL(observer, OnRunning()).Times(0);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(search_from_running);
  screen_listener->SetState(ScreenListenerState::SUSPENDED);

  Expectation search_from_suspended =
      EXPECT_CALL(observer, OnSearching()).After(suspend_from_running);
  screen_listener->SetState(ScreenListenerState::SEARCHING);
  EXPECT_CALL(observer, OnSuspended()).Times(0);
  screen_listener->SetState(ScreenListenerState::SUSPENDED);

  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnRunning()).After(suspend_from_running);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  screen_listener->SetState(ScreenListenerState::STOPPING);
  EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  screen_listener->SetState(ScreenListenerState::STOPPED);
}

TEST(ScreenListenerImplTest, ObserveFromSearching) {
  auto screen_listener = MakeUnique<ScreenListenerImpl>(nullptr);
  MockObserver observer;
  screen_listener->SetObserver(&observer);

  screen_listener->Start();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  screen_listener->SetState(ScreenListenerState::STARTING);
  screen_listener->SetState(ScreenListenerState::RUNNING);

  screen_listener->SearchNow();

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);

  screen_listener->Suspend();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  EXPECT_CALL(observer, OnSuspended());
  screen_listener->SetState(ScreenListenerState::SUSPENDED);

  EXPECT_TRUE(screen_listener->SearchNow());

  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  screen_listener->SetState(ScreenListenerState::SEARCHING);

  screen_listener->Resume();
  EXPECT_THAT(screen_listener->TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  EXPECT_CALL(observer, OnRunning());
  screen_listener->SetState(ScreenListenerState::RUNNING);
}

TEST(ScreenListenerImplTest, ScreenObserverPassThrough) {
  ScreenList list;
  const ScreenInfo screen1{
      "id1", "name1", "eth0", {{192, 168, 1, 10}, 12345}, {{}, 0}};
  const ScreenInfo screen2{
      "id2", "name2", "eth0", {{192, 168, 1, 11}, 12345}, {{}, 0}};
  const ScreenInfo screen3{
      "id3", "name3", "eth0", {{192, 168, 1, 12}, 12345}, {{}, 0}};
  const ScreenInfo screen1_alt_name{
      "id1", "name1 alt", "eth0", {{192, 168, 1, 10}, 12345}, {{}, 0}};
  auto screen_listener = MakeUnique<ScreenListenerImpl>(&list);
  MockObserver observer;
  screen_listener->SetObserver(&observer);

  EXPECT_CALL(observer, OnScreenAdded(screen1));
  screen_listener->OnScreenAdded(screen1);
  EXPECT_CALL(observer, OnScreenChanged(screen1_alt_name));
  screen_listener->OnScreenChanged(screen1_alt_name);
  EXPECT_CALL(observer, OnScreenRemoved(screen1_alt_name));
  screen_listener->OnScreenRemoved(screen1_alt_name);

  auto verify_info_member = [&screen_listener](const ScreenInfo& info) {
    const auto& screens = screen_listener->GetScreens();
    EXPECT_TRUE(std::find(screens.begin(), screens.end(), info) !=
                screens.end());
  };
  list.OnScreenAdded(screen1);
  list.OnScreenAdded(screen2);
  list.OnScreenAdded(screen3);
  EXPECT_CALL(observer, OnScreenAdded(screen1))
      .WillOnce(::testing::Invoke(verify_info_member));
  screen_listener->OnScreenAdded(screen1);
  EXPECT_CALL(observer, OnScreenAdded(screen2))
      .WillOnce(::testing::Invoke(verify_info_member));
  screen_listener->OnScreenAdded(screen2);
  EXPECT_CALL(observer, OnScreenAdded(screen3))
      .WillOnce(::testing::Invoke(verify_info_member));
  screen_listener->OnScreenAdded(screen3);

  EXPECT_THAT(screen_listener->GetScreens(),
              ElementsAre(screen1, screen2, screen3));

  list.OnAllScreensRemoved();
  EXPECT_CALL(observer, OnAllScreensRemoved())
      .WillOnce(::testing::Invoke([&screen_listener]() {
        EXPECT_TRUE(screen_listener->GetScreens().empty());
      }));
  screen_listener->OnAllScreensRemoved();
}

}  // namespace openscreen
