// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/subscriber_model.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {

class MockCancellationToken {
 public:
  MOCK_METHOD0(Cancel, void());
};

TEST(SubscriberModelTest, TestSubscribeAndUnsubscribe) {
  SubscriberModel<int> model;
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{0});

  model.Subscribe(1);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], 1);

  model.Subscribe(2);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{2});
  EXPECT_TRUE(std::find(model.subscribers().begin(), model.subscribers().end(),
                        2) != model.subscribers().end());

  model.Subscribe(2);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{2});

  model.Unsubscribe(1);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], 2);

  model.Unsubscribe(1);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], 2);

  model.Unsubscribe(2);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{0});
}

TEST(SubscriberModelTest, TestWithPointers) {
  int one = 1;
  int two = 2;
  SubscriberModel<int*> model;
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{0});

  model.Subscribe(&one);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], &one);

  model.Subscribe(&two);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{2});
  EXPECT_TRUE(std::find(model.subscribers().begin(), model.subscribers().end(),
                        &two) != model.subscribers().end());

  model.Subscribe(&two);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{2});

  model.Unsubscribe(&one);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], &two);

  model.Unsubscribe(&one);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], &two);

  model.Unsubscribe(&two);
  model.ApplyPendingChanges();
  EXPECT_EQ(model.subscribers().size(), size_t{0});
}

TEST(SubscriberModelTest, TestUnsubscribeBlocking) {
  // Run the test repeatedly - condition_variables will act up
  // nondeterministically, so repeated checks help ensure that failures are
  // found in UTs.
  for (int i = 0; i < 50; i++) {
    MockCancellationToken token;
    SubscriberModel<int> model([&token]() { token.Cancel(); });
    EXPECT_CALL(token, Cancel()).Times(1);
    model.Subscribe(1);
    model.ApplyPendingChanges();
    EXPECT_EQ(model.subscribers().size(), size_t{1});
    EXPECT_EQ(model.subscribers()[0], 1);

    std::atomic_bool has_started{false};
    std::atomic_bool has_completed{false};
    std::atomic_int x{0};
    std::thread thread([&x, &model, &has_started, &has_completed]() {
      has_started = true;
      model.UnsubscribeBlocking(1);
      x = 1;
      has_completed = true;
    });
    while (!has_started) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x.load(), 0);
    model.ApplyPendingChanges();
    EXPECT_EQ(model.subscribers().size(), size_t{0});

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    while (!has_completed) {
    }
    EXPECT_EQ(x.load(), 1);
    thread.join();
  }
}

}  // namespace openscreen
