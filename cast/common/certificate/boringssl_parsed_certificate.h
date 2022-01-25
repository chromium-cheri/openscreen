// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_BORINGSSL_PARSED_CERTIFICATE_H_
#define CAST_COMMON_CERTIFICATE_BORINGSSL_PARSED_CERTIFICATE_H_

#include <openssl/x509.h>

#include <string>
#include <vector>

#include "cast/common/public/parsed_certificate.h"
#include "platform/base/error.h"

namespace openscreen {
namespace cast {

bool VerifySignedData(const EVP_MD* digest,
                      EVP_PKEY* public_key,
                      const ConstDataSpan& data,
                      const ConstDataSpan& signature);

ErrorOr<DateTime> GetNotBeforeTime(X509* cert);
ErrorOr<DateTime> GetNotAfterTime(X509* cert);

class BoringSSLParsedCertificate final : public ParsedCertificate {
 public:
  BoringSSLParsedCertificate();
  explicit BoringSSLParsedCertificate(bssl::UniquePtr<X509> cert);
  ~BoringSSLParsedCertificate() override;

  ErrorOr<std::vector<uint8_t>> SerializeToDER(
      int front_spacing) const override;

  ErrorOr<DateTime> GetNotBeforeTime() const override;
  ErrorOr<DateTime> GetNotAfterTime() const override;

  std::string GetCommonName() const override;

  std::string GetSpkiTlv() const override;

  ErrorOr<uint64_t> GetSerialNumber() const override;

  bool VerifySignedData(DigestAlgorithm algorithm,
                        const ConstDataSpan& data,
                        const ConstDataSpan& signature) const override;

  bool HasPolicyOid(const ConstDataSpan& oid) const override;

  X509* get() { return cert_.get(); }

 private:
  bssl::UniquePtr<X509> cert_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_CERTIFICATE_BORINGSSL_PARSED_CERTIFICATE_H_
