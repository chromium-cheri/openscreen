// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/aes_key.h"

#include <utility>

#include "gtest/gtest.h"
#include "platform/base/error.h"

namespace openscreen {
namespace {

struct NonZero {
  NonZero() = default;
  bool operator()(int n) const { return n > 0; }
};

}  // namespace

TEST(AESKeyUnitTest, CanGenerateAesKey) {
  auto aes_key = AESKey::Create();
  ASSERT_TRUE(aes_key.is_value());

  NonZero pred;
  const auto& key = aes_key.value().key();
  const auto& iv = aes_key.value().iv();
  ASSERT_TRUE(std::any_of(key.begin(), key.end(), pred));
  ASSERT_TRUE(std::any_of(iv.begin(), iv.end(), pred));
}

TEST(AESKeyUnitTest, CanCopyAndMove) {
  auto aes_key = AESKey::Create();
  ASSERT_TRUE(aes_key.is_value());

  AESKey copy = aes_key.value();
  ASSERT_TRUE(copy == aes_key.value());

  AESKey moved = std::move(copy);
  ASSERT_TRUE(moved == aes_key.value());
}

}  // namespace openscreen
