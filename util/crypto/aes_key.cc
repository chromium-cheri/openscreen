// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/aes_key.h"

#include "openssl/rand.h"

namespace openscreen {
AESKey::AESKey(const AESKey& other) noexcept = default;
AESKey::AESKey(AESKey&& other) noexcept = default;
AESKey& AESKey::operator=(const AESKey& other) = default;
AESKey& AESKey::operator=(AESKey&& other) = default;
AESKey::~AESKey() = default;

bool AESKey::operator==(const AESKey& other) const {
  return (key_ == other.key_) && (iv_ == other.iv_);
}

bool AESKey::operator!=(const AESKey& other) const {
  return !(*this == other);
}

// static
ErrorOr<AESKey> AESKey::Create() {
  AESKey key;
  if (RAND_bytes(key.key_.data(), key.key_.size()) != 1) {
    return Error::Code::kRandomBytesFailure;
  }
  if (RAND_bytes(key.iv_.data(), key.iv_.size()) != 1) {
    return Error::Code::kRandomBytesFailure;
  }
  return key;
}

AESKey::AESKey() = default;

}  // namespace openscreen
