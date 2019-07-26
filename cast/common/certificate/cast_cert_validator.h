// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_H_
#define CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "platform/base/macros.h"

namespace cast {
namespace certificate {

class CastCRL;

// Describes the policy for a Device certificate.
enum class CastDeviceCertPolicy {
  // The device certificate is unrestricted.
  NONE,

  // The device certificate is for an audio-only device.
  AUDIO_ONLY,
};

enum class CRLPolicy {
  // Revocation is only checked if a CRL is provided.
  CRL_OPTIONAL,

  // Revocation is always checked. A missing CRL results in failure.
  CRL_REQUIRED,
};

enum class CastCertError {
  OK,

  // Certificates were not provided for verification.
  ERR_CERTS_MISSING,

  // The certificates provided could not be parsed.
  ERR_CERTS_PARSE,

  // Key usage is missing or is not set to Digital Signature.
  // This error could also be thrown if the CN is missing.
  ERR_CERTS_RESTRICTIONS,

  // The current date is before the notBefore date or after the notAfter date.
  ERR_CERTS_DATE_INVALID,

  // The certificate failed to chain to a trusted root.
  ERR_CERTS_VERIFY_GENERIC,

  // The CRL is missing or failed to verify.
  ERR_CRL_INVALID,

  // One of the certificates in the chain is revoked.
  ERR_CERTS_REVOKED,

  // The pathlen constraint of the root certificate was exceeded.
  ERR_CERTS_PATHLEN,

  // An internal coding error.
  ERR_UNEXPECTED,
};

enum class DigestAlgorithm {
  SHA1,
  SHA256,
  SHA384,
  SHA512,
};

struct ConstDataSpan {
  const uint8_t* data;
  uint32_t length;
};

struct GeneralizedTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

// An object of this type is returned by the VerifyDeviceCert function, and can
// be used for additional certificate-related operations, using the verified
// certificate.
class CertVerificationContext {
 public:
  CertVerificationContext() = default;
  virtual ~CertVerificationContext() = default;

  // Use the public key from the verified certificate to verify a
  // |digest_algorithm|WithRSAEncryption |signature| over arbitrary |data|.
  // Both |signature| and |data| hold raw binary data. Returns true if the
  // signature was correct.
  virtual bool VerifySignatureOverData(
      const ConstDataSpan& signature,
      const ConstDataSpan& data,
      DigestAlgorithm digest_algorithm) const = 0;

  // Retrieve the Common Name attribute of the subject's distinguished name from
  // the verified certificate, if present.  Returns an empty string if no Common
  // Name is found.
  virtual const std::string& GetCommonName() const = 0;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(CertVerificationContext);
};

struct TrustStore;

// Verifies a cast device certficate given a chain of DER-encoded certificates,
// using the built-in Cast trust anchors.
//
// Inputs:
//
// * |certs| is a chain of DER-encoded certificates:
//   * |certs[0]| is the target certificate (i.e. the device certificate).
//   * |certs[1..n-1]| are intermediates certificates to use in path building.
//     Their ordering does not matter.
//
// * |time| is the unix timestamp to use for determining if the certificate
//   is expired.
//
// * |crl| is the CRL to check for certificate revocation status.
//   If this is a nullptr, then revocation checking is currently disabled.
//
// * |crl_policy| is for choosing how to handle the absence of a CRL.
//   If CRL_REQUIRED is passed, then an empty |crl| input would result
//   in a failed verification. Otherwise, |crl| is ignored if it is absent.
//
// Outputs:
//
// Returns CastCertError::OK on success. Otherwise, the corresponding
// CastCertError. On success, the output parameters are filled with more
// details:
//
//   * |context| is filled with an object that can be used to verify signatures
//     using the device certificate's public key, as well as to extract other
//     properties from the device certificate (Common Name).
//   * |policy| is filled with an indication of the device certificate's policy
//     (i.e. is it for audio-only devices or is it unrestricted?)
MAYBE_NODISCARD CastCertError
VerifyDeviceCert(const std::vector<std::string>& certs,
                 const GeneralizedTime& time,
                 std::unique_ptr<CertVerificationContext>* context,
                 CastDeviceCertPolicy* policy,
                 const CastCRL* crl,
                 CRLPolicy crl_policy);

// TODO(btolsch): Collapse these.
MAYBE_NODISCARD CastCertError VerifyDeviceCertUsingCustomTrustStore(
    const std::vector<std::string>& certs,
    const GeneralizedTime& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy,
    TrustStore* trust_store);

}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_H_
