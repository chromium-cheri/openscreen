// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/optional.h"

#include <string>

#include "gtest/gtest.h"

namespace {

class Dummy {
 public:
  Dummy() = default;
  explicit Dummy(int i) : dummy(i) {}
  ~Dummy() = default;

  int dummy;
};

}  // namespace

namespace openscreen {

TEST(OptionalTest, DefaultIsEmpty) {
  Optional<Dummy> opt;
  EXPECT_FALSE(opt.has_value());
}

TEST(OptionalTest, CanConstructFromValue) {
  Optional<int> opt(42);
  EXPECT_EQ(42, opt.value());
}

TEST(OptionalTest, CanAssignFromValue) {
  Optional<Dummy> opt;
  opt = Dummy(1234);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(1234, opt->dummy);
}

TEST(OptionalTest, CanDereferenceLikePointer) {
  Optional<Dummy> opt(Dummy(1337));
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(1337, opt->dummy);
}

TEST(OptionalTest, CanGetValue) {
  Optional<Dummy> opt(Dummy(31337));
  EXPECT_EQ(31337, opt.value().dummy);

  const Optional<Dummy> const_opt(Dummy(-1));
  EXPECT_EQ(-1, const_opt.value().dummy);
}

TEST(OptionalTest, CanCheckAsBool) {
  Optional<Dummy> opt(Dummy(31337));
  EXPECT_TRUE(opt);

  Optional<Dummy> empty;
  EXPECT_FALSE(empty);
}
}  // namespace openscreen
