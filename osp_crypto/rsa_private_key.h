// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_CRYPTO_RSA_PRIVATE_KEY_H_
#define OSP_CRYPTO_RSA_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "osp_base/macros.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace openscreen {

// Encapsulates an RSA private key. Can be used to generate new keys, export
// keys to other formats, or to extract a public key.
// TODO(hclam): This class should be ref-counted so it can be reused easily.
class RSAPrivateKey {
 public:
  ~RSAPrivateKey();

  // Create a new random instance. Can return NULL if initialization fails.
  static std::unique_ptr<RSAPrivateKey> Create(uint16_t num_bits);

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  static std::unique_ptr<RSAPrivateKey> CreateFromPrivateKeyInfo(
      const std::vector<uint8_t>& input);

  // Create a new instance from an existing EVP_PKEY, taking a
  // reference to it. |key| must be an RSA key. Returns NULL on
  // failure.
  static std::unique_ptr<RSAPrivateKey> CreateFromKey(EVP_PKEY* key);

  EVP_PKEY* key() { return key_.get(); }

  // Creates a copy of the object.
  std::unique_ptr<RSAPrivateKey> Copy() const;

  // Exports the private key to a PKCS #8 PrivateKeyInfo block.
  bool ExportPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the public key to an X509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8_t>* output) const;

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  RSAPrivateKey();

  bssl::UniquePtr<EVP_PKEY> key_;

  OSP_DISALLOW_COPY_AND_ASSIGN(RSAPrivateKey);
};

}  // namespace openscreen

#endif  // OSP_CRYPTO_RSA_PRIVATE_KEY_H_
