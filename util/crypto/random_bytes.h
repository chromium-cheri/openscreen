// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CRYPTO_RANDOM_BYTES_H_
#define UTIL_CRYPTO_RANDOM_BYTES_H_

#include <openssl/base.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "platform/base/error.h"

namespace openscreen {
namespace crypto {
std::array<uint8_t, 16> GenerateRandomBytes16();
void GenerateRandomBytes(uint8_t* out, int len);
}  // namespace crypto
}  // namespace openscreen

#endif  // UTIL_CRYPTO_RANDOM_BYTES_H_
