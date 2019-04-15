// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hkdf_scrypt_psk.h"

#include "base/boringssl_util.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace openscreen {
namespace authentication {

namespace {

constexpr uint64_t kScryptBlockSize = 8;
constexpr uint64_t kScryptParallelization = 1;
constexpr size_t kScryptKeyLength = 32;
constexpr size_t kHkdfKeyLength = 32;

}  // namespace

ErrorOr<std::vector<uint8_t>> ComputeHkdfScryptPsk(
    const std::vector<uint8_t>& psk,
    const std::vector<uint8_t>& salt,
    uint64_t scrypt_cost,
    const std::vector<uint8_t>& hkdf_info) {
  const size_t scrypt_max_mem =
      (scrypt_cost + kScryptParallelization + 1) * 2 * kScryptBlockSize * 64;

  std::vector<uint8_t> scrypt_key(kScryptKeyLength);
  int scrypt_result = EVP_PBE_scrypt(
      reinterpret_cast<const char*>(psk.data()), psk.size(), salt.data(),
      salt.size(), scrypt_cost, kScryptBlockSize, kScryptParallelization,
      scrypt_max_mem, scrypt_key.data(), scrypt_key.size());

  if (!scrypt_result) {
    LogAndClearBoringSslErrors();
    return Error::Code::kProofComputationFailed;
  }

  std::vector<uint8_t> hkdf_key(kHkdfKeyLength);
  int hkdf_result = HKDF(hkdf_key.data(), hkdf_key.size(), EVP_sha256(),
                         scrypt_key.data(), scrypt_key.size(), salt.data(),
                         salt.size(), hkdf_info.data(), hkdf_info.size());

  if (!hkdf_result) {
    LogAndClearBoringSslErrors();
    return Error::Code::kProofComputationFailed;
  }

  return hkdf_key;
}

}  // namespace authentication
}  // namespace openscreen
