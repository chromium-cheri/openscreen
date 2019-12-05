// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_CHANNEL_DEVICE_AUTH_TEST_HELPERS_H_
#define CAST_RECEIVER_CHANNEL_DEVICE_AUTH_TEST_HELPERS_H_

#include <openssl/x509.h>

#include "cast/common/certificate/test_helpers.h"
#include "cast/receiver/channel/device_auth_namespace_handler.h"

namespace cast {
namespace channel {

class StaticCredentialsProvider final
    : public DeviceAuthNamespaceHandler::CredentialsProvider {
 public:
  StaticCredentialsProvider() = default;
  ~StaticCredentialsProvider() = default;

  absl::Span<uint8_t> GetCurrentTlsCertAsDer() override {
    return absl::Span<uint8_t>(tls_cert_der);
  }
  DeviceCredentials* GetCurrentDeviceCredentials() override {
    return &device_creds;
  }

  DeviceCredentials device_creds;
  std::vector<uint8_t> tls_cert_der;
};

void InitStaticCredentialsFromFiles(StaticCredentialsProvider* creds,
                                    bssl::UniquePtr<X509>* parsed_cert,
                                    certificate::TrustStore* fake_trust_store,
                                    const std::string& privkey_filename,
                                    const std::string& chain_filename,
                                    const std::string& tls_filename);

}  // namespace channel
}  // namespace cast

#endif  // CAST_RECEIVER_CHANNEL_DEVICE_AUTH_TEST_HELPERS_H_
