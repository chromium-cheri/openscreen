// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_PARSED_CERTIFICATE_H_
#define CAST_COMMON_PUBLIC_PARSED_CERTIFICATE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cast/common/public/certificate_types.h"
#include "platform/base/error.h"

namespace openscreen {
namespace cast {

enum class DigestAlgorithm {
  kSha1,
  kSha256,
  kSha384,
  kSha512,
};

// This class represents a certificate that may already be parsed into its
// component fields for easier access.  The field access is limited to what is
// relevant to Cast device authentication.
class ParsedCertificate {
 public:
  static ErrorOr<std::unique_ptr<ParsedCertificate>> ParseFromDER(
      const std::vector<uint8_t>& der_cert);

  virtual ~ParsedCertificate() = default;

  // |front_spacing| is the number of bytes to add as padding to the result
  // vector.  This is used to place a nonce value in front during Cast
  // authentication.
  virtual ErrorOr<std::vector<uint8_t>> SerializeToDER(
      int front_spacing) const = 0;

  virtual ErrorOr<DateTime> GetNotBeforeTime() const = 0;
  virtual ErrorOr<DateTime> GetNotAfterTime() const = 0;

  // Retrieve the Common Name attribute of the subject's distinguished name from
  // the verified certificate, if present.  Returns an empty string if no Common
  // Name is found.
  virtual std::string GetCommonName() const = 0;

  virtual std::string GetSpkiTlv() const = 0;

  // This is only needed for certificates where we already know the serial
  // number will fit in 64 bits, so it doesn't need a big-number type.
  virtual ErrorOr<uint64_t> GetSerialNumber() const = 0;

  // Use the public key from the verified certificate to verify a
  // |digest_algorithm|WithRSAEncryption |signature| over arbitrary |data|.
  // Both |signature| and |data| hold raw binary data. Returns true if the
  // signature was correct.
  virtual bool VerifySignedData(DigestAlgorithm algorithm,
                                const ConstDataSpan& data,
                                const ConstDataSpan& signature) const = 0;

  virtual bool HasPolicyOid(const ConstDataSpan& oid) const = 0;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_PUBLIC_PARSED_CERTIFICATE_H_
