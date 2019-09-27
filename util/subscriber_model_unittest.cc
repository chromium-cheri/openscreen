// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/subscriber_model.h"

#include <chrono>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {

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

TEST(SubscriberModelTest, TestUnsubscribeAndBlock) {
  // Run the test repeatedly - condition_variables will act up
  // nondeterministically, so repeated checks help ensure that failures are
  // found in UTs.
  for (int i = 0; i < 50; i++) {
    SubscriberModel<int> model;
    model.Subscribe(1);
    EXPECT_EQ(model.subscribers().size(), size_t{1});
    EXPECT_EQ(model.subscribers()[0], 1);

    bool has_started = false;
    int x = 0;
    std::thread thread([&x, &model, &has_started]() {
      has_started = true;
      model.UnsubscribeAndBlock(1);
      x = 1;
    });
    while (!has_started) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x, 0);
    EXPECT_EQ(model.subscribers().size(), size_t{0});

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x, 1);
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

    bool has_started = false;
    int x = 0;
    std::thread thread([&x, &model, &has_started]() {
      has_started = true;
      model.BlockUntilSubscribersPresent();
      x = 1;
    });
    while (!has_started) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x, 0);

    model.Subscribe(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(x, 1);
    thread.join();
  }
}

TEST(SubscriberModelTest, BlockUntilSubscibersPresentShouldntBlock) {
  SubscriberModel<int> model;
  EXPECT_EQ(model.subscribers().size(), size_t{0});
  model.Subscribe(0);

  bool has_started = false;
  int x = 0;
  std::thread thread([&x, &model, &has_started]() {
    has_started = true;
    model.BlockUntilSubscribersPresent();
    x = 1;
  });
  while (!has_started) {
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(x, 1);
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
