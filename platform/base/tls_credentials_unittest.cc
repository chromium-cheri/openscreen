// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <algorithm>
#include <chrono>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"
#include "util/std_util.h"

namespace openscreen {

using std::chrono::seconds;
using testing::EndsWith;
using testing::StartsWith;

namespace {

bssl::UniquePtr<EVP_PKEY> GenerateRsaKeypair() {
  bssl::UniquePtr<BIGNUM> prime(BN_new());
  EXPECT_NE(0, BN_set_word(prime.get(), RSA_F4));

  bssl::UniquePtr<RSA> rsa(RSA_new());
  EXPECT_NE(0, RSA_generate_key_ex(rsa.get(), 2048, prime.get(), nullptr));

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  EXPECT_NE(0, EVP_PKEY_set1_RSA(pkey.get(), rsa.get()));

  return pkey;
}

}  // namespace

TEST(TlsCredentialsTest, CredentialsAreGeneratedAppropriately) {
  bssl::UniquePtr<EVP_PKEY> pkey = GenerateRsaKeypair();
  ErrorOr<TlsCredentials> creds_or_error =
      TlsCredentials::Create("test.com", seconds(31556952), std::move(pkey));
  EXPECT_TRUE(creds_or_error.is_value());
  TlsCredentials credentials = creds_or_error.MoveValue();

  // Validate the generated certificate. A const cast is necessary because
  // openssl is not const correct for this method.
  EXPECT_NE(0, X509_verify(const_cast<X509*>(&credentials.certificate()),
                           const_cast<EVP_PKEY*>(&credentials.keypair())));
  EXPECT_GT(credentials.raw_der_certificate().size(), 0u);
  const std::string certificate_string =
      ToString(credentials.raw_der_certificate());

  // Calling d2i_X509 does validation of the certificate, beyond the checking
  // done in the i2d_X509 method that creates the raw_der_certificate.
  const unsigned char* raw_cert_begin =
      credentials.raw_der_certificate().data();
  const bssl::UniquePtr<X509> x509_certificate(d2i_X509(
      nullptr, &raw_cert_begin, credentials.raw_der_certificate().size()));
  EXPECT_NE(nullptr, x509_certificate);

  // Validate the private key
  EXPECT_GT(credentials.private_key_base64().size(), 0u);
  const std::string private_key_string =
      ToString(credentials.private_key_base64());
  EXPECT_THAT(private_key_string, StartsWith("-----BEGIN PRIVATE KEY-----\n"));
  EXPECT_THAT(private_key_string, EndsWith("-----END PRIVATE KEY-----\n"));

  // Validate the public key
  EXPECT_GT(credentials.public_key_base64().size(), 0u);
  const std::string public_key_string =
      ToString(credentials.public_key_base64());
  EXPECT_THAT(public_key_string, StartsWith("-----BEGIN PUBLIC KEY-----\n"));
  EXPECT_THAT(public_key_string, EndsWith("-----END PUBLIC KEY-----\n"));

  // Validate the hash
  // A SHA-256 hash should always be 256 bits, or 32 bytes.
  const unsigned int kSha256HashSizeInBytes = 32;
  EXPECT_EQ(credentials.public_key_hash().size(), kSha256HashSizeInBytes);
}

}  // namespace openscreen
