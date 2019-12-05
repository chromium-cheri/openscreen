// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/receiver/channel/device_auth_test_helpers.h"

#include "gtest/gtest.h"

namespace cast {
namespace channel {

void InitStaticCredentialsFromFiles(StaticCredentialsProvider* creds,
                                    bssl::UniquePtr<X509>* parsed_cert,
                                    certificate::TrustStore* fake_trust_store,
                                    const std::string& privkey_filename,
                                    const std::string& chain_filename,
                                    const std::string& tls_filename) {
  auto pkey = certificate::testing::ReadKeyFromPemFile(privkey_filename);
  ASSERT_TRUE(pkey);
  std::vector<std::string> certs =
      certificate::testing::ReadCertificatesFromPemFile(chain_filename);
  ASSERT_GT(certs.size(), 1u);

  // Use the root of the chain as the trust store for the test.
  const uint8_t* data = (const uint8_t*)certs.back().data();
  X509* fake_root = d2i_X509(nullptr, &data, certs.back().size());
  ASSERT_TRUE(fake_root);
  certs.pop_back();
  if (fake_trust_store) {
    fake_trust_store->certs.emplace_back(fake_root);
  } else {
    X509_free(fake_root);
  }

  creds->device_creds =
      DeviceCredentials{std::move(certs), std::string(), std::move(pkey)};

  std::vector<std::string> tls_cert =
      certificate::testing::ReadCertificatesFromPemFile(tls_filename);
  ASSERT_EQ(tls_cert.size(), 1u);
  data = reinterpret_cast<const uint8_t*>(tls_cert[0].data());
  if (parsed_cert) {
    *parsed_cert =
        bssl::UniquePtr<X509>(d2i_X509(nullptr, &data, tls_cert[0].size()));
    ASSERT_TRUE(*parsed_cert);
  }
  creds->tls_cert_der.assign(
      reinterpret_cast<const uint8_t*>(tls_cert[0].data()),
      reinterpret_cast<const uint8_t*>(tls_cert[0].data()) +
          tls_cert[0].size());
}

}  // namespace channel
}  // namespace cast
