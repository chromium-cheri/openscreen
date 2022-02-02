// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/boringssl_parsed_certificate.h"

#include <openssl/x509v3.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "util/crypto/certificate_utils.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

const EVP_MD* MapDigestAlgorithm(DigestAlgorithm algorithm) {
  switch (algorithm) {
    case DigestAlgorithm::kSha1:
      return EVP_sha1();
    case DigestAlgorithm::kSha256:
      return EVP_sha256();
    case DigestAlgorithm::kSha384:
      return EVP_sha384();
    case DigestAlgorithm::kSha512:
      return EVP_sha512();
    default:
      return nullptr;
  }
}

// Parse the data in |time| at |index| as a two-digit ascii number. Note this
// function assumes the caller already did a bounds check and checked the inputs
// are digits.
uint8_t ParseAsn1TimeDoubleDigit(absl::string_view time, size_t index) {
  OSP_DCHECK_LT(index + 1, time.size());
  OSP_DCHECK('0' <= time[index] && time[index] <= '9');
  OSP_DCHECK('0' <= time[index + 1] && time[index + 1] <= '9');
  return (time[index] - '0') * 10 + (time[index + 1] - '0');
}

// Parses DateTime with additional restrictions laid out by RFC 5280
// 4.1.2.5.2.
bool ParseAsn1GeneralizedTime(ASN1_GENERALIZEDTIME* time, DateTime* out) {
  static constexpr uint8_t kDaysPerMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };

  absl::string_view time_str{
      reinterpret_cast<const char*>(ASN1_STRING_get0_data(time)),
      static_cast<size_t>(ASN1_STRING_length(time))};
  if (time_str.size() != 15) {
    return false;
  }
  if (time_str[14] != 'Z') {
    return false;
  }
  for (size_t i = 0; i < 14; ++i) {
    if (time_str[i] < '0' || time_str[i] > '9') {
      return false;
    }
  }
  out->year = ParseAsn1TimeDoubleDigit(time_str, 0) * 100 +
              ParseAsn1TimeDoubleDigit(time_str, 2);
  out->month = ParseAsn1TimeDoubleDigit(time_str, 4);
  out->day = ParseAsn1TimeDoubleDigit(time_str, 6);
  out->hour = ParseAsn1TimeDoubleDigit(time_str, 8);
  out->minute = ParseAsn1TimeDoubleDigit(time_str, 10);
  out->second = ParseAsn1TimeDoubleDigit(time_str, 12);
  if (out->month == 0 || out->month > 12) {
    return false;
  }
  int days_per_month = kDaysPerMonth[out->month - 1];
  if (out->month == 2) {
    if (out->year % 4 == 0 && (out->year % 100 != 0 || out->year % 400 == 0)) {
      days_per_month = 29;
    } else {
      days_per_month = 28;
    }
  }
  if (out->day == 0 || out->day > days_per_month) {
    return false;
  }
  if (out->hour > 23) {
    return false;
  }
  if (out->minute > 59) {
    return false;
  }
  // Leap seconds are allowed.
  if (out->second > 60) {
    return false;
  }
  return true;
}

}  // namespace

bool VerifySignedData(const EVP_MD* digest,
                      EVP_PKEY* public_key,
                      const ConstDataSpan& data,
                      const ConstDataSpan& signature) {
  // This code assumes the signature algorithm was RSASSA PKCS#1 v1.5 with
  // |digest|.
  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr, public_key)) {
    return false;
  }
  return (EVP_DigestVerify(ctx.get(), signature.data, signature.length,
                           data.data, data.length) == 1);
}

ErrorOr<DateTime> GetNotBeforeTime(X509* cert) {
  bssl::UniquePtr<ASN1_GENERALIZEDTIME> not_before_asn1{
      ASN1_TIME_to_generalizedtime(X509_get0_notBefore(cert), nullptr)};
  if (!not_before_asn1) {
    return Error(Error::Code::kErrCertsParse,
                 "Failed to retrieve cert notBefore");
  }
  DateTime result;
  if (!ParseAsn1GeneralizedTime(not_before_asn1.get(), &result)) {
    return Error(Error::Code::kErrCertsParse, "Failed to parse cert notBefore");
  }
  return result;
}

ErrorOr<DateTime> GetNotAfterTime(X509* cert) {
  bssl::UniquePtr<ASN1_GENERALIZEDTIME> not_after_asn1{
      ASN1_TIME_to_generalizedtime(X509_get0_notAfter(cert), nullptr)};
  if (!not_after_asn1) {
    return Error(Error::Code::kErrCertsParse,
                 "Failed to retrieve parse cert notAfter");
  }
  DateTime result;
  if (!ParseAsn1GeneralizedTime(not_after_asn1.get(), &result)) {
    return Error(Error::Code::kErrCertsParse, "Failed to parse cert notAfter");
  }
  return result;
}

// static
ErrorOr<std::unique_ptr<ParsedCertificate>> ParsedCertificate::ParseFromDER(
    const std::vector<uint8_t>& der_cert) {
  ErrorOr<bssl::UniquePtr<X509>> parsed =
      ImportCertificate(der_cert.data(), der_cert.size());
  if (!parsed) {
    return parsed.error();
  }
  std::unique_ptr<ParsedCertificate> result =
      std::make_unique<BoringSSLParsedCertificate>(std::move(parsed.value()));
  return result;
}

BoringSSLParsedCertificate::BoringSSLParsedCertificate() = default;

BoringSSLParsedCertificate::BoringSSLParsedCertificate(
    bssl::UniquePtr<X509> cert)
    : cert_(std::move(cert)) {}

BoringSSLParsedCertificate::~BoringSSLParsedCertificate() = default;

ErrorOr<std::vector<uint8_t>> BoringSSLParsedCertificate::SerializeToDER(
    int front_spacing) const {
  OSP_DCHECK_GE(front_spacing, 0);
  int cert_len = i2d_X509(cert_.get(), nullptr);
  if (cert_len <= 0) {
    return Error(Error::Code::kErrCertSerialize, "Serializing cert failed.");
  }
  std::vector<uint8_t> cert_der(front_spacing + cert_len, 0);
  uint8_t* data = &cert_der[front_spacing];
  if (!i2d_X509(cert_.get(), &data)) {
    return Error(Error::Code::kErrCertSerialize, "Serializing cert failed.");
  }
  return cert_der;
}

ErrorOr<DateTime> BoringSSLParsedCertificate::GetNotBeforeTime() const {
  return openscreen::cast::GetNotBeforeTime(cert_.get());
}

ErrorOr<DateTime> BoringSSLParsedCertificate::GetNotAfterTime() const {
  return openscreen::cast::GetNotAfterTime(cert_.get());
}

std::string BoringSSLParsedCertificate::GetCommonName() const {
  X509_NAME* target_subject = X509_get_subject_name(cert_.get());
  int len =
      X509_NAME_get_text_by_NID(target_subject, NID_commonName, nullptr, 0);
  if (len <= 0) {
    return {};
  }
  // X509_NAME_get_text_by_NID writes one more byte than it reports, for a
  // trailing NUL.
  std::string common_name(len + 1, 0);
  len = X509_NAME_get_text_by_NID(target_subject, NID_commonName,
                                  &common_name[0], common_name.size());
  if (len <= 0) {
    return {};
  }
  common_name.resize(len);
  return common_name;
}

std::string BoringSSLParsedCertificate::GetSpkiTlv() const {
  return openscreen::GetSpkiTlv(cert_.get());
}

ErrorOr<uint64_t> BoringSSLParsedCertificate::GetSerialNumber() const {
  return ParseDerUint64(X509_get0_serialNumber(cert_.get()));
}

bool BoringSSLParsedCertificate::VerifySignedData(
    DigestAlgorithm algorithm,
    const ConstDataSpan& data,
    const ConstDataSpan& signature) const {
  const EVP_MD* digest = MapDigestAlgorithm(algorithm);
  if (!digest) {
    return false;
  }

  bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(cert_.get())};
  return openscreen::cast::VerifySignedData(digest, public_key.get(), data,
                                            signature);
}

bool BoringSSLParsedCertificate::HasPolicyOid(const ConstDataSpan& oid) const {
  // Gets an index into the extension list that points to the
  // certificatePolicies extension, if it exists, -1 otherwise.
  bool has_oid = false;
  int pos = X509_get_ext_by_NID(cert_.get(), NID_certificate_policies, -1);
  if (pos != -1) {
    X509_EXTENSION* policies_extension = X509_get_ext(cert_.get(), pos);
    const ASN1_STRING* value = X509_EXTENSION_get_data(policies_extension);
    const uint8_t* in = ASN1_STRING_get0_data(value);
    CERTIFICATEPOLICIES* policies =
        d2i_CERTIFICATEPOLICIES(nullptr, &in, ASN1_STRING_length(value));

    if (policies) {
      // Check for |oid| in the set of policies.
      uint32_t policy_count = sk_POLICYINFO_num(policies);
      for (uint32_t i = 0; i < policy_count; ++i) {
        POLICYINFO* info = sk_POLICYINFO_value(policies, i);
        if (OBJ_length(info->policyid) == oid.length &&
            memcmp(OBJ_get0_data(info->policyid), oid.data, oid.length) == 0) {
          has_oid = true;
          break;
        }
      }
      CERTIFICATEPOLICIES_free(policies);
    }
  }
  return has_oid;
}

}  // namespace cast
}  // namespace openscreen
