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

// Compiler checks.
static_assert(sizeof(NullOptType) == sizeof(bool),
              "nullopt_t shouldn't add to size of optional");

TEST(OptionalTest, SizeOfIsCorrect) {
  const NullOptType kNullOptInstance{NullOptTypeInit()};
  const Optional<Dummy> kEmpty;
  const Optional<Dummy> kDummy(Dummy(123));
  const Optional<Dummy> kNullOptDummy(kNullOpt);

  // Due to alignment the size may be padded up to another machine word.
  EXPECT_EQ(sizeof(kNullOptInstance), sizeof(bool));
  EXPECT_LE(sizeof(kEmpty), sizeof(char) + sizeof(bool) + sizeof(size_t));
  EXPECT_LE(sizeof(kNullOptDummy),
            sizeof(char) + sizeof(bool) + sizeof(size_t));
  EXPECT_LE(sizeof(kDummy), sizeof(Dummy) + sizeof(bool) + sizeof(size_t));
}

TEST(OptionalTest, CanConstruct) {
  const Optional<Dummy> kNoArgCtor;
  const Optional<Dummy> kNullOptCtor(kNullOpt);
  const Optional<Dummy> kValue(Dummy(123));
  const Optional<Dummy> kCopy(kValue);
  Optional<Dummy> movee(Dummy(456));
  Optional<Dummy> mover(std::move(movee));

  EXPECT_FALSE(kNoArgCtor.has_value());
  EXPECT_FALSE(kNullOptCtor.has_value());
  ASSERT_TRUE(kValue.has_value());
  EXPECT_EQ(123, kValue->dummy);
  ASSERT_TRUE(mover.has_value());
  EXPECT_EQ(456, mover->dummy);
}

TEST(OptionalTest, CanAssign) {
  const Optional<Dummy> copyable(Dummy(123));
  Optional<Dummy> value;
  EXPECT_FALSE(value.has_value());
  Optional<Dummy> movee(Dummy(456));
  value = copyable;
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(123, value->dummy);

  value = std::move(movee);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(456, value->dummy);
}

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

TEST(OptionalTest, CanCheckHasValue) {
  Optional<Dummy> opt(Dummy(31337));
  EXPECT_TRUE(opt.has_value());

  Optional<Dummy> empty;
  EXPECT_FALSE(empty.has_value());
}
}  // namespace openscreen
