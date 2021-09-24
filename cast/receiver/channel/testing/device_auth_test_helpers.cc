// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/receiver/channel/testing/device_auth_test_helpers.h"

#include <string>
#include <utility>

#include "cast/common/certificate/testing/test_helpers.h"
#include "gtest/gtest.h"
#include "util/crypto/pem_helpers.h"

namespace openscreen {
namespace cast {

void InitStaticCredentialsFromFiles(StaticCredentialsProvider* creds,
                                    bssl::UniquePtr<X509>* parsed_cert,
                                    TrustStore* fake_trust_store,
                                    absl::string_view privkey_filename,
                                    absl::string_view chain_filename,
                                    absl::string_view tls_filename) {
  auto private_key = ReadKeyFromPemFile(privkey_filename);
  ASSERT_TRUE(private_key);
  std::vector<std::vector<uint8_t>> certs =
      ReadCertificatesFromPemFile(chain_filename);
  ASSERT_GT(certs.size(), 1u);

  // Use the root of the chain as the trust store for the test.
  const uint8_t* data = certs.back().data();
  auto fake_root =
      bssl::UniquePtr<X509>(d2i_X509(nullptr, &data, certs.back().size()));
  ASSERT_TRUE(fake_root);
  certs.pop_back();
  if (fake_trust_store) {
    fake_trust_store->certs.emplace_back(fake_root.release());
  }

  creds->device_creds = DeviceCredentials{
      std::move(certs), std::move(private_key), std::string()};

  std::vector<uint8_t> tls_cert = ReadCertificateFromPemFile(tls_filename);
  const auto* tls_cert_data = tls_cert.data();
  if (parsed_cert) {
    *parsed_cert = bssl::UniquePtr<X509>(
        d2i_X509(nullptr, &tls_cert_data, tls_cert.size()));
    ASSERT_TRUE(*parsed_cert);
  }
  creds->tls_cert_der.assign(tls_cert_data, tls_cert_data + tls_cert.size());
}

}  // namespace cast
}  // namespace openscreen
