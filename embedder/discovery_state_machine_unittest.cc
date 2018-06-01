// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder/discovery_state_machine.h"

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

using ::testing::ElementsAre;

using State = DiscoveryStateMachine::State;
using StateTransition = DiscoveryStateMachine::StateTransition;

TEST(DiscoveryStateMachineTest, NormalStartStop) {
  DiscoveryStateMachine state_machine;

  ASSERT_EQ(State::kStopped, state_machine.state());
  EXPECT_TRUE(state_machine.Start());
  EXPECT_FALSE(state_machine.Start());
  EXPECT_EQ(State::kStopped, state_machine.state());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  EXPECT_THAT(state_machine.TakeNewStateTransitions(), ElementsAre());
  state_machine.SetState(State::kStarting);
  EXPECT_EQ(State::kStarting, state_machine.state());
  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());

  EXPECT_TRUE(state_machine.Stop());
  EXPECT_FALSE(state_machine.Stop());
  EXPECT_EQ(State::kRunning, state_machine.state());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  state_machine.SetState(State::kStopping);
  EXPECT_EQ(State::kStopping, state_machine.state());
  state_machine.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, state_machine.state());
}

TEST(DiscoveryStateMachineTest, BatchStartStop) {
  DiscoveryStateMachine state_machine;

  ASSERT_EQ(State::kStopped, state_machine.state());
  EXPECT_TRUE(state_machine.Start());
  EXPECT_FALSE(state_machine.Start());
  EXPECT_EQ(State::kStopped, state_machine.state());
  EXPECT_TRUE(state_machine.Stop());
  EXPECT_FALSE(state_machine.Stop());
  EXPECT_EQ(State::kStopped, state_machine.state());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart, StateTransition::kStop));
  state_machine.SetState(State::kStarting);
  EXPECT_EQ(State::kStarting, state_machine.state());
  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());
  state_machine.SetState(State::kStopping);
  EXPECT_EQ(State::kStopping, state_machine.state());
  state_machine.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, state_machine.state());
}

TEST(DiscoveryStateMachineTest, StopBeforeRunning) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.Start());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  EXPECT_EQ(State::kStarting, state_machine.state());

  EXPECT_TRUE(state_machine.Stop());
  EXPECT_FALSE(state_machine.Stop());
  EXPECT_EQ(State::kStarting, state_machine.state());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  state_machine.SetState(State::kStopping);
  EXPECT_EQ(State::kStopping, state_machine.state());
  state_machine.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, state_machine.state());
}

TEST(DiscoveryStateMachineTest, StartSuspended) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.StartAndSuspend());
  EXPECT_FALSE(state_machine.Start());
  EXPECT_FALSE(state_machine.Suspend());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStartSuspended));
  state_machine.SetState(State::kStarting);
  EXPECT_EQ(State::kStarting, state_machine.state());
  EXPECT_EQ(StateTransition::kStartSuspended,
            state_machine.SetState(State::kSuspended));
  EXPECT_EQ(State::kSuspended, state_machine.state());

  EXPECT_FALSE(state_machine.StartAndSuspend());
  EXPECT_FALSE(state_machine.Start());
  EXPECT_FALSE(state_machine.Suspend());
}

TEST(DiscoveryStateMachineTest, SuspendAndResume) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_FALSE(state_machine.Resume());
  EXPECT_TRUE(state_machine.Suspend());
  EXPECT_FALSE(state_machine.Suspend());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());

  EXPECT_FALSE(state_machine.Start());
  EXPECT_FALSE(state_machine.Suspend());
  EXPECT_TRUE(state_machine.Resume());
  EXPECT_FALSE(state_machine.Resume());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());
}

TEST(DiscoveryStateMachineTest, SearchWhileRunning) {
  DiscoveryStateMachine state_machine;

  EXPECT_FALSE(state_machine.SearchNow());
  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_TRUE(state_machine.SearchNow());
  EXPECT_FALSE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromRunning);
  EXPECT_EQ(State::kSearchingFromRunning, state_machine.state());

  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());
}

TEST(DiscoveryStateMachineTest, SearchWhileSuspended) {
  DiscoveryStateMachine state_machine;

  EXPECT_FALSE(state_machine.SearchNow());
  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_FALSE(state_machine.Resume());
  EXPECT_TRUE(state_machine.Suspend());
  EXPECT_FALSE(state_machine.Suspend());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());

  EXPECT_TRUE(state_machine.SearchNow());
  EXPECT_FALSE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromSuspended);
  EXPECT_EQ(State::kSearchingFromSuspended, state_machine.state());

  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());
}

TEST(DiscoveryStateMachineTest, StopWhileSearching) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_TRUE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromRunning);
  EXPECT_EQ(State::kSearchingFromRunning, state_machine.state());

  EXPECT_TRUE(state_machine.Stop());
  EXPECT_FALSE(state_machine.Stop());

  state_machine.SetState(State::kRunning);
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  state_machine.SetState(State::kStopping);
  EXPECT_EQ(State::kStopping, state_machine.state());
  state_machine.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, state_machine.state());
}

TEST(DiscoveryStateMachineTest, StopWhileSearchingImmediate) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_TRUE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromRunning);
  EXPECT_EQ(State::kSearchingFromRunning, state_machine.state());

  EXPECT_TRUE(state_machine.Stop());
  EXPECT_FALSE(state_machine.Stop());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  state_machine.SetState(State::kStopping);
  EXPECT_EQ(State::kStopping, state_machine.state());
  state_machine.SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, state_machine.state());
}

TEST(DiscoveryStateMachineTest, ResumeWhileSearching) {
  DiscoveryStateMachine state_machine;

  EXPECT_FALSE(state_machine.SearchNow());
  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_FALSE(state_machine.Resume());
  EXPECT_TRUE(state_machine.Suspend());
  EXPECT_FALSE(state_machine.Suspend());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());

  EXPECT_TRUE(state_machine.SearchNow());
  EXPECT_FALSE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromSuspended);
  EXPECT_EQ(State::kSearchingFromSuspended, state_machine.state());

  EXPECT_TRUE(state_machine.Resume());
  EXPECT_FALSE(state_machine.Resume());

  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());
}

TEST(DiscoveryStateMachineTest, ResumeWhileSearchingImmediate) {
  DiscoveryStateMachine state_machine;

  EXPECT_FALSE(state_machine.SearchNow());
  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_FALSE(state_machine.Resume());
  EXPECT_TRUE(state_machine.Suspend());
  EXPECT_FALSE(state_machine.Suspend());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());

  EXPECT_TRUE(state_machine.SearchNow());
  EXPECT_FALSE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromSuspended);
  EXPECT_EQ(State::kSearchingFromSuspended, state_machine.state());

  EXPECT_TRUE(state_machine.Resume());
  EXPECT_FALSE(state_machine.Resume());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());
}

TEST(DiscoveryStateMachineTest, SuspendWhileSearching) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_TRUE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromRunning);
  EXPECT_EQ(State::kSearchingFromRunning, state_machine.state());

  EXPECT_FALSE(state_machine.Resume());
  EXPECT_TRUE(state_machine.Suspend());
  EXPECT_FALSE(state_machine.Suspend());

  state_machine.SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, state_machine.state());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());
}

TEST(DiscoveryStateMachineTest, SuspendWhileSearchingImmediate) {
  DiscoveryStateMachine state_machine;

  EXPECT_TRUE(state_machine.Start());
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  EXPECT_TRUE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromRunning);
  EXPECT_EQ(State::kSearchingFromRunning, state_machine.state());

  EXPECT_FALSE(state_machine.Resume());
  EXPECT_TRUE(state_machine.Suspend());
  EXPECT_FALSE(state_machine.Suspend());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  state_machine.SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, state_machine.state());
}

TEST(DiscoveryStateMachineTest, ObserveTransitions) {
  DiscoveryStateMachine state_machine;

  state_machine.Start();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStarting));
  EXPECT_EQ(StateTransition::kStart, state_machine.SetState(State::kRunning));

  state_machine.SearchNow();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  EXPECT_EQ(StateTransition::kSearchNow,
            state_machine.SetState(State::kSearchingFromRunning));

  state_machine.Suspend();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  EXPECT_EQ(StateTransition::kSuspend,
            state_machine.SetState(State::kSuspended));

  state_machine.SearchNow();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  EXPECT_EQ(StateTransition::kSearchNow,
            state_machine.SetState(State::kSearchingFromSuspended));

  state_machine.Resume();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  EXPECT_EQ(StateTransition::kResume, state_machine.SetState(State::kRunning));

  state_machine.Stop();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStopping));
  EXPECT_EQ(StateTransition::kStop, state_machine.SetState(State::kStopped));

  state_machine.Start();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStarting));
  EXPECT_EQ(StateTransition::kStart, state_machine.SetState(State::kRunning));

  state_machine.Suspend();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  EXPECT_EQ(StateTransition::kSuspend,
            state_machine.SetState(State::kSuspended));

  state_machine.Stop();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStop));
  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStopping));
  EXPECT_EQ(StateTransition::kStop, state_machine.SetState(State::kStopped));
}

TEST(DiscoveryStateMachineTest, ObserveBatchTransitions) {
  DiscoveryStateMachine state_machine;

  state_machine.Start();
  state_machine.SearchNow();
  state_machine.Suspend();
  state_machine.SearchNow();
  state_machine.Resume();
  state_machine.Stop();
  state_machine.Start();
  state_machine.Suspend();
  state_machine.Stop();

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart, StateTransition::kSearchNow,
                          StateTransition::kSuspend,
                          StateTransition::kSearchNow, StateTransition::kResume,
                          StateTransition::kStop, StateTransition::kStart,
                          StateTransition::kSuspend, StateTransition::kStop));

  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStarting));
  EXPECT_EQ(StateTransition::kStart, state_machine.SetState(State::kRunning));

  EXPECT_EQ(StateTransition::kSearchNow,
            state_machine.SetState(State::kSearchingFromRunning));

  EXPECT_EQ(StateTransition::kSuspend,
            state_machine.SetState(State::kSuspended));

  EXPECT_EQ(StateTransition::kSearchNow,
            state_machine.SetState(State::kSearchingFromSuspended));

  EXPECT_EQ(StateTransition::kResume, state_machine.SetState(State::kRunning));

  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStopping));
  EXPECT_EQ(StateTransition::kStop, state_machine.SetState(State::kStopped));

  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStarting));
  EXPECT_EQ(StateTransition::kStart, state_machine.SetState(State::kRunning));

  EXPECT_EQ(StateTransition::kSuspend,
            state_machine.SetState(State::kSuspended));

  EXPECT_EQ(StateTransition::kNone, state_machine.SetState(State::kStopping));
  EXPECT_EQ(StateTransition::kStop, state_machine.SetState(State::kStopped));
}

TEST(DiscoveryStateMachineTest, ObserveFromSearching) {
  DiscoveryStateMachine state_machine;

  state_machine.Start();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kStart));
  state_machine.SetState(State::kStarting);
  state_machine.SetState(State::kRunning);

  state_machine.SearchNow();

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromRunning);

  state_machine.Suspend();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSuspend));
  EXPECT_EQ(StateTransition::kSuspend,
            state_machine.SetState(State::kSuspended));

  EXPECT_TRUE(state_machine.SearchNow());

  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kSearchNow));
  state_machine.SetState(State::kSearchingFromSuspended);

  state_machine.Resume();
  EXPECT_THAT(state_machine.TakeNewStateTransitions(),
              ElementsAre(StateTransition::kResume));
  EXPECT_EQ(StateTransition::kResume, state_machine.SetState(State::kRunning));
}

}  // namespace openscreen
