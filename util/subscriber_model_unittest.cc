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

class MockCancellationToken : public SubscriberModelCancellationToken {
 public:
  MOCK_METHOD0(CancelRunningOperations, void());
};

TEST(SubscriberModelTest, TestSubscribeAndUnsubscribe) {
  SubscriberModel<int> model;
  EXPECT_EQ(model.subscribers().size(), size_t{0});

  model.Subscribe(1);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], 1);

  model.Subscribe(2);
  EXPECT_EQ(model.subscribers().size(), size_t{2});
  EXPECT_TRUE(std::find(model.subscribers().begin(), model.subscribers().end(),
                        2) != model.subscribers().end());

  model.Subscribe(2);
  EXPECT_EQ(model.subscribers().size(), size_t{2});

  model.Unsubscribe(1);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], 2);

  model.Unsubscribe(1);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], 2);

  model.Unsubscribe(2);
  EXPECT_EQ(model.subscribers().size(), size_t{0});
}

TEST(SubscriberModelTest, TestWithPointers) {
  int one = 1;
  int two = 2;
  SubscriberModel<int*> model;
  EXPECT_EQ(model.subscribers().size(), size_t{0});

  model.Subscribe(&one);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], &one);

  model.Subscribe(&two);
  EXPECT_EQ(model.subscribers().size(), size_t{2});
  EXPECT_TRUE(std::find(model.subscribers().begin(), model.subscribers().end(),
                        &two) != model.subscribers().end());

  model.Subscribe(&two);
  EXPECT_EQ(model.subscribers().size(), size_t{2});

  model.Unsubscribe(&one);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], &two);

  model.Unsubscribe(&one);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0], &two);

  model.Unsubscribe(&two);
  EXPECT_EQ(model.subscribers().size(), size_t{0});
}

TEST(SubscriberModelTest, TestUnsubscribeAndBlock) {
  // Run the test repeatedly - condition_variables will act up
  // nondeterministically, so repeated checks help ensure that failures are
  // found in UTs.
  for (int i = 0; i < 50; i++) {
    MockCancellationToken token;
    SubscriberModel<int> model(&token);
    EXPECT_CALL(token, CancelRunningOperations()).Times(1);
    model.Subscribe(1);
    EXPECT_EQ(model.subscribers().size(), size_t{1});
    EXPECT_EQ(model.subscribers()[0], 1);

    std::atomic_bool has_started{false};
    std::atomic_bool has_completed{false};
    std::atomic_int x{0};
    std::thread thread([&x, &model, &has_started, &has_completed]() {
      has_started = true;
      model.UnsubscribeAndBlock(1);
      x = 1;
      has_completed = true;
    });
    while (!has_started) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x.load(), 0);
    EXPECT_EQ(model.subscribers().size(), size_t{0});

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    while (!has_completed) {
    }
    EXPECT_EQ(x.load(), 1);
    thread.join();
  }
}

TEST(SubscriberModelTest, BlockUntilSubscibersPresentShouldBlock) {
  // Run the test repeatedly - condition_variables will act up
  // nondeterministically, so repeated checks help ensure that failures are
  // found in UTs.
  for (int i = 0; i < 50; i++) {
    SubscriberModel<int> model;
    EXPECT_EQ(model.subscribers().size(), size_t{0});

    std::atomic_bool has_started{false};
    std::atomic_bool has_completed{false};
    std::atomic_int x{0};
    std::thread thread([&x, &model, &has_started, &has_completed]() {
      has_started = true;
      model.BlockUntilSubscribersPresent();
      x = 1;
      has_completed = true;
    });
    while (!has_started) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x.load(), 0);

    model.Subscribe(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    while (!has_completed) {
    }
    EXPECT_EQ(x.load(), 1);
    thread.join();
  }
}

TEST(SubscriberModelTest, BlockUntilSubscibersPresentShouldntBlock) {
  SubscriberModel<int> model;
  EXPECT_EQ(model.subscribers().size(), size_t{0});
  model.Subscribe(0);

  std::atomic_bool has_started{false};
  std::atomic_int x{0};
  std::thread thread([&x, &model, &has_started]() {
    has_started = true;
    model.BlockUntilSubscribersPresent();
    x = 1;
  });
  while (!has_started) {
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(x.load(), 1);
  thread.join();
}

TEST(SubscriberModelTest, TestMapSubscribeAndUnsubscribe) {
  MapSubscriberModel<int, char> model(' ');
  EXPECT_EQ(model.subscribers().size(), size_t{0});

  model.Subscribe(1, 'c');
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0].first, 1);
  EXPECT_EQ(model.subscribers()[0].second, 'c');

  model.Subscribe(2, 'd');
  EXPECT_EQ(model.subscribers().size(), size_t{2});
  EXPECT_TRUE(std::find(model.subscribers().begin(), model.subscribers().end(),
                        std::make_pair(2, 'd')) != model.subscribers().end());

  model.Subscribe(2, 'e');
  EXPECT_EQ(model.subscribers().size(), size_t{2});
  EXPECT_TRUE(std::find(model.subscribers().begin(), model.subscribers().end(),
                        std::make_pair(2, 'e')) != model.subscribers().end());

  model.Unsubscribe(1);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0].first, 2);
  EXPECT_EQ(model.subscribers()[0].second, 'e');

  model.Unsubscribe(1);
  EXPECT_EQ(model.subscribers().size(), size_t{1});
  EXPECT_EQ(model.subscribers()[0].first, 2);
  EXPECT_EQ(model.subscribers()[0].second, 'e');

  model.Unsubscribe(2);
  EXPECT_EQ(model.subscribers().size(), size_t{0});
}

}  // namespace openscreen
