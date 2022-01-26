// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_TRUST_STORE_H_
#define CAST_COMMON_PUBLIC_TRUST_STORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "cast/common/public/certificate_types.h"
#include "cast/common/public/parsed_certificate.h"
#include "platform/base/error.h"

namespace openscreen {
namespace cast {

// This class represents a set of certificates that form a root trust set.  The
// only operation on this set is to check whether a given set of certificates
// can be used to form a valid certificate chain to one of the root
// certificates.
class TrustStore {
 public:
  using CertificatePathResult = std::vector<std::unique_ptr<ParsedCertificate>>;

  static std::unique_ptr<TrustStore> CreateInstanceFromPemFile(
      absl::string_view file_path);

  TrustStore() = default;
  virtual ~TrustStore() = default;

  // Checks whether a subset of the certificates in |der_certs| can form a valid
  // certificate chain to one of the root certificates in this trust store,
  // where the time at which all certificates need to be valid is |time|.
  // Returns an error if no path is found, otherwise returns the certificate
  // chain that is found.
  virtual ErrorOr<CertificatePathResult> FindCertificatePath(
      const std::vector<std::string>& der_certs,
      const DateTime& time) = 0;
};

// Singleton that is the root of trust for Cast device certificates.
class CastTrustStore {
 public:
  static TrustStore* GetInstance();
  static void ResetInstance();

  static TrustStore* CreateInstanceForTest(
      const std::vector<uint8_t>& trust_anchor_der);

  static TrustStore* CreateInstanceFromPemFile(absl::string_view file_path);
};

// Singleton that is the root of trust for signed CRL data.
class CastCRLTrustStore {
 public:
  static TrustStore* GetInstance();
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_PUBLIC_TRUST_STORE_H_
