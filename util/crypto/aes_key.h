// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CRYPTO_AES_KEY_H_
#define UTIL_CRYPTO_AES_KEY_H_

#include <openssl/base.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "platform/base/error.h"

namespace openscreen {

// AES block size must be 128 bits.
const int kAesBlockSize = 128 / 8;

// Encapsulates an AES key and IV.
// AES Keys may be either 128, 192, or 256 bits long, however we always use 128
// bit long keys. IV masks must be 128 bits long to match the AES block size.
class AESKey {
 public:
  AESKey(const AESKey& other);
  AESKey(AESKey&& other) noexcept;
  AESKey& operator=(const AESKey& other);
  AESKey& operator=(AESKey&& other) noexcept;
  ~AESKey();

  bool operator==(const AESKey& other) const;
  bool operator!=(const AESKey& other) const;

  // Create a new random instance. Can return nullptr if initialization fails.
  static ErrorOr<AESKey> Create();

  // Randomly generated AES key, 128 bits (16 bytes) long.
  const std::array<uint8_t, kAesBlockSize>& key() const { return key_; }

  // Randomly generated IV mask, 128 bits (16 bytes) long.
  const std::array<uint8_t, kAesBlockSize>& iv() const { return iv_; }

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  AESKey();

  std::array<uint8_t, kAesBlockSize> key_;
  std::array<uint8_t, kAesBlockSize> iv_;
};

}  // namespace openscreen

#endif  // UTIL_CRYPTO_AES_KEY_H_
