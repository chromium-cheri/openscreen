// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_CREDENTIALS_H_
#define PLATFORM_API_TLS_CREDENTIALS_H_

#include <openssl/crypto.h>

#include <memory>
#include <string>
#include <vector>

#include "platform/base/macros.h"

namespace openscreen {

struct TlsCredentials {
 public:
  bssl::UniquePtr<CRYPTO_BUFFER> certificate;
  std::vector<uint8_t> tls_certificate_der;
  bssl::UniquePtr<EVP_PKEY> private_key;

  std::vector<uint8_t> private_key_base64;
  std::vector<uint8_t> public_key_base64;

  // A SHA-256 digest of the public key
  std::vector<uint8_t> tls_pk_sha256_hash();
};

}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CREDENTIALS_H_
