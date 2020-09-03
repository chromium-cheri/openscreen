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

TEST(AesKeyUnitTest, EqualityChecks) {
  ErrorOr<AESKey> eok_one = AESKey::Create();
  ErrorOr<AESKey> eok_two = AESKey::Create();
  ASSERT_TRUE(eok_one.is_value());
  ASSERT_TRUE(eok_two.is_value());

  const AESKey& key_one = eok_one.value();
  const AESKey& key_two = eok_two.value();
  ASSERT_TRUE(key_one == key_one);
  ASSERT_TRUE(key_one != key_two);
  ASSERT_TRUE(key_two == key_two);
  ASSERT_TRUE(key_two != key_one);
  ASSERT_FALSE(key_one != key_one);
  ASSERT_FALSE(key_one == key_two);
  ASSERT_FALSE(key_two != key_two);
  ASSERT_FALSE(key_two == key_one);
}

TEST(AesKeyUnitTest, KeysAreNotIdentical) {
  constexpr int kNumKeys = 100;
  std::vector<AESKey> keys;
  for (int i = 0; i < kNumKeys; ++i) {
    ErrorOr<AESKey> eok = AESKey::Create();
    ASSERT_TRUE(eok.is_value());
    keys.push_back(std::move(eok.value()));
  }

  for (int i = 1; i < kNumKeys; ++i) {
    ASSERT_TRUE(keys[i - 1] != keys[i]);
  }
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
