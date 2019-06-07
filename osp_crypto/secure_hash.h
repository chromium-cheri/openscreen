// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_CRYPTO_SECURE_HASH_H_
#define OSP_CRYPTO_SECURE_HASH_H_

#include <stddef.h>

#include <memory>

#include "openssl/sha.h"
#include "osp_base/macros.h"

namespace openscreen {

// A wrapper to calculate secure hashes incrementally, allowing to
// be used when the full input is not known in advance. The end result will the
// same as if we have the full input in advance.
class SecureHash {
 public:
  SecureHash();
  SecureHash(const SecureHash& other);
  SecureHash(SecureHash&& other);
  SecureHash& operator=(const SecureHash&);
  SecureHash& operator=(SecureHash&&);

  ~SecureHash();

  void Update(const void* input, size_t len);
  void Finish(void* output, size_t len);
  size_t GetHashLength() const;

 private:
  SHA256_CTX ctx_;
};

}  // namespace openscreen

#endif  // OSP_CRYPTO_SECURE_HASH_H_
