// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

#include <chrono>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"

namespace openscreen {

using std::chrono::seconds;

namespace {
// clang-format off
const char kTestRsaPrivateKey[] = R"(-----BEGIN RSA PRIVATE KEY-----
MIICXAIBAAKBgQCoDpMO8X59QEyK28lG4afJvV1GiliI3IbyJ7tkerEdFxIN7j2W
uIpwPYewbSjMsHMMGUI4qt0ymHLP7hC4l05oChfQMz5+Hh/QgTjgGUCULTvCrvbv
m8NB2gDqfpl5sAP+b44x8Y7VMCx3tD9cCjDArKcBrlrlZpjPgF70R9+6DwIDAQAB
AoGAJp2qtu1QxlEHBYU5O7tQRa/ohfP2IgSiUXRFv/HdTrTlZiQPLNncnavVyPlc
PaRx2x7Ws5S3XQ/gmdy3JONHlYsdEvIT79e9tF1kMwZlo+mCFaGqO84lgNfT/D6g
sAXCCB93rTBlw82tq56vHEGp+Iy1CA0pRuW7th1r79aGZ3ECQQDTe6gYrHOYameo
uJmKdhAONX0sNGH+pPiUs4jXE+KkRVzv1sxt14nVO3+oFUGtnIw79IzWsuVAT6ee
3sPLhsDHAkEAy27IfoxdURoY+qCIvq9pxNuMdIiPEbtQRNz6QEzZwzSg5TnBwRGt
yyYfS0W2L4aM5UnyHFRyEKbTx9SFX/iEeQJAJPYRtO4+7b57C3Pn8dkciT8z90vY
bKbsgyC1o9Fn5QnyakCCThhDkK7rarb8WZlosPnWu5dcldbWFuAcHDAa9QJAHEDg
m6LG+xKS0YwmMcWO/QY49Z5ZsG7BcS73mqKRw+i1R+DDphrcdlBvHDcsXGFlhBrH
A2Q/C00AMMq1U2TikQJBALNJBoW3bBWvpQNDcNJlPemAMjrkw295/hKl1ypk7NR1
YxgD9X9lEeXWGdYT7cGQc4yJ6zNuxh9+iM84A6uEcX0=
-----END RSA PRIVATE KEY-----
)";

const char kTestRsaPublicKey[] = R"(-----BEGIN PUBLIC KEY-----
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCoDpMO8X59QEyK28lG4afJvV1G
iliI3IbyJ7tkerEdFxIN7j2WuIpwPYewbSjMsHMMGUI4qt0ymHLP7hC4l05oChfQ
Mz5+Hh/QgTjgGUCULTvCrvbvm8NB2gDqfpl5sAP+b44x8Y7VMCx3tD9cCjDArKcB
rlrlZpjPgF70R9+6DwIDAQAB
-----END PUBLIC KEY-----
)";
// clang-format on
}  // namespace

TEST(TlsCredentialsTest, CredentialsConstructWithoutError) {
  bssl::UniquePtr<RSA> rsa(RSA_new());
  bssl::UniquePtr<BIO> bio_private_key(BIO_new_mem_buf(kTestRsaPrivateKey, -1));
  bssl::UniquePtr<BIO> bio_public_key(BIO_new_mem_buf(kTestRsaPublicKey, -1));

  RSA* rsa_raw = rsa.get();
  PEM_read_bio_RSAPrivateKey(bio_private_key.get(), &rsa_raw, nullptr, nullptr);
  std::cout << "1: " << ERR_error_string(ERR_get_error(), nullptr) << std::endl;

  PEM_read_bio_RSAPublicKey(bio_public_key.get(), &rsa_raw, nullptr, nullptr);
  std::cout << "2: " << ERR_error_string(ERR_get_error(), nullptr) << std::endl;

  ErrorOr<TlsCredentials> creds_or_error =
      TlsCredentials::Create("test", seconds(31556952), std::move(rsa));

  EXPECT_TRUE(creds_or_error.is_value());

  TlsCredentials credentials = creds_or_error.MoveValue();

  EXPECT_GE(credentials.raw_der_certificate().size(), 0u);
  EXPECT_GE(credentials.private_key_base64().size(), 0u);
  EXPECT_GE(credentials.public_key_base64().size(), 0u);
  EXPECT_GE(credentials.public_key_hash().size(), 0u);
}

}  // namespace openscreen
