// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_crypto/secure_hash.h"

#include <stddef.h>

#include <cstring>

#include "openssl/mem.h"
#include "osp_crypto/openssl_util.h"
#include "platform/api/logging.h"

namespace openscreen {

SecureHash::SecureHash() {
  SHA256_Init(&ctx_);
}

SecureHash::SecureHash(const SecureHash& other) = default;
SecureHash::SecureHash(SecureHash&& other) = default;
SecureHash& SecureHash::operator=(const SecureHash&) = default;
SecureHash& SecureHash::operator=(SecureHash&&) = default;

SecureHash::~SecureHash() {
  OPENSSL_cleanse(&ctx_, sizeof(ctx_));
}

void SecureHash::Update(const void* input, size_t len) {
  SHA256_Update(&ctx_, static_cast<const unsigned char*>(input), len);
}

void SecureHash::Finish(void* output, size_t len) {
  ScopedOpenSSLSafeSizeBuffer<SHA256_DIGEST_LENGTH> result(
      static_cast<unsigned char*>(output), len);
  SHA256_Final(result.safe_buffer(), &ctx_);
}

size_t SecureHash::GetHashLength() const {
  return SHA256_DIGEST_LENGTH;
}

}  // namespace openscreen
