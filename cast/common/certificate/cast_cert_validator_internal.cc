// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/cast_cert_validator_internal.h"

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/ossl_typ.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <time.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "cast/common/certificate/types.h"
#include "util/crypto/pem_helpers.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr static int32_t kMinRsaModulusLengthBits = 2048;

// TODO(davidben): Switch this to bssl::UniquePtr after
// https://boringssl-review.googlesource.com/c/boringssl/+/46105 lands.
struct FreeNameConstraints {
  void operator()(NAME_CONSTRAINTS* nc) { NAME_CONSTRAINTS_free(nc); }
};
using NameConstraintsPtr =
    std::unique_ptr<NAME_CONSTRAINTS, FreeNameConstraints>;

// These values are bit positions from RFC 5280 4.2.1.3 and will be passed to
// ASN1_BIT_STRING_get_bit.
enum KeyUsageBits {
  kDigitalSignature = 0,
};

int BoringSSLVerifyCB(int current_result, X509_STORE_CTX* ctx) {
  X509* cert = X509_STORE_CTX_get_current_cert(ctx);
  if (current_result == 0) {
    int err = X509_STORE_CTX_get_error(ctx);

    if (err == X509_V_ERR_NAME_CONSTRAINTS_WITHOUT_SANS) {
      // NOTE(btolsch): Ignore a name constraints violation if it's due to the
      // subject-alt-name set being empty.
      current_result = 1;
    } else if (err == X509_V_ERR_CERT_HAS_EXPIRED ||
               err == X509_V_ERR_CERT_NOT_YET_VALID) {
      // NOTE(btolsch): Ignore valid time range on trusted certificates.
      X509_NAME* name = X509_get_subject_name(cert);
      bssl::UniquePtr<STACK_OF(X509)> matches{X509_STORE_get1_certs(ctx, name)};
      if (matches) {
        for (size_t i = 0; i < sk_X509_num(matches.get()); ++i) {
          X509* test = sk_X509_value(matches.get(), i);
          if (X509_cmp(test, cert) == 0) {
            current_result = 1;
            break;
          }
        }
      }
    }
  }

  bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(cert)};
  if (EVP_PKEY_bits(public_key.get()) >= kMinRsaModulusLengthBits) {
    return current_result;
  }
  return 0;
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

bssl::UniquePtr<BASIC_CONSTRAINTS> GetConstraints(X509* issuer) {
  // TODO(davidben): This and other |X509_get_ext_d2i| are missing
  // error-handling for syntax errors in certificates. See BoringSSL
  // documentation for the calling convention.
  return bssl::UniquePtr<BASIC_CONSTRAINTS>{
      reinterpret_cast<BASIC_CONSTRAINTS*>(
          X509_get_ext_d2i(issuer, NID_basic_constraints, nullptr, nullptr))};
}

bssl::UniquePtr<ASN1_BIT_STRING> GetKeyUsage(X509* cert) {
  return bssl::UniquePtr<ASN1_BIT_STRING>{reinterpret_cast<ASN1_BIT_STRING*>(
      X509_get_ext_d2i(cert, NID_key_usage, nullptr, nullptr))};
}

X509* ParseX509Der(const std::string& der) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(der.data());
  return d2i_X509(nullptr, &data, der.size());
}

}  // namespace

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

bool GetCertValidTimeRange(X509* cert,
                           DateTime* not_before,
                           DateTime* not_after) {
  bssl::UniquePtr<ASN1_GENERALIZEDTIME> not_before_asn1{
      ASN1_TIME_to_generalizedtime(X509_get0_notBefore(cert), nullptr)};
  bssl::UniquePtr<ASN1_GENERALIZEDTIME> not_after_asn1{
      ASN1_TIME_to_generalizedtime(X509_get0_notAfter(cert), nullptr)};
  if (!not_before_asn1 || !not_after_asn1) {
    return false;
  }
  return ParseAsn1GeneralizedTime(not_before_asn1.get(), not_before) &&
         ParseAsn1GeneralizedTime(not_after_asn1.get(), not_after);
}

// static
TrustStore TrustStore::CreateInstanceFromPemFile(absl::string_view file_path) {
  TrustStore store;

  std::vector<std::string> certs = ReadCertificatesFromPemFile(file_path);
  for (const auto& der_cert : certs) {
    const uint8_t* data = (const uint8_t*)der_cert.data();
    store.certs.emplace_back(d2i_X509(nullptr, &data, der_cert.size()));
  }

  return store;
}

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

Error FindCertificatePath(const std::vector<std::string>& der_certs,
                          const DateTime& time,
                          CertificatePathResult* result_path,
                          TrustStore* trust_store) {
  if (der_certs.empty()) {
    return Error(Error::Code::kErrCertsMissing, "Missing DER certificates");
  }

  bssl::UniquePtr<X509>& target_cert = result_path->target_cert;
  std::vector<bssl::UniquePtr<X509>>& intermediate_certs =
      result_path->intermediate_certs;
  bssl::UniquePtr<STACK_OF(X509)> bssl_sk{sk_X509_new_null()};
  bssl::UniquePtr<X509_STORE_CTX> bssl_store_ctx{X509_STORE_CTX_new()};
  target_cert.reset(ParseX509Der(der_certs[0]));
  if (!target_cert) {
    return Error(Error::Code::kErrCertsParse,
                 "FindCertificatePath: Invalid target certificate");
  }
  for (size_t i = 1; i < der_certs.size(); ++i) {
    bssl::UniquePtr<X509> cert{ParseX509Der(der_certs[i])};
    if (!cert) {
      return Error(
          Error::Code::kErrCertsParse,
          absl::StrCat(
              "FindCertificatePath: Failed to parse intermediate certificate ",
              i, " of ", der_certs.size()));
    }
    X509_up_ref(cert.get());
    sk_X509_push(bssl_sk.get(), cert.get());
    intermediate_certs.push_back(std::move(cert));
  }

  bssl::UniquePtr<ASN1_BIT_STRING> key_usage = GetKeyUsage(target_cert.get());
  if (!key_usage) {
    return Error(Error::Code::kErrCertsRestrictions,
                 "FindCertificatePath: Failed with no key usage");
  }
  int bit =
      ASN1_BIT_STRING_get_bit(key_usage.get(), KeyUsageBits::kDigitalSignature);
  if (bit == 0) {
    return Error(Error::Code::kErrCertsRestrictions,
                 "FindCertificatePath: Failed to get digital signature");
  }

  // TODO(btolsch): Convert TrustStore to hold X509_STORE* to avoid this.
  bssl::UniquePtr<X509_STORE> bssl_trust_store{X509_STORE_new()};
  for (auto& cert : trust_store->certs) {
    X509_STORE_add_cert(bssl_trust_store.get(), cert.get());
  }

  X509_STORE_set_verify_cb(bssl_trust_store.get(), &BoringSSLVerifyCB);
  X509_STORE_CTX_init(bssl_store_ctx.get(), bssl_trust_store.get(),
                      target_cert.get(), bssl_sk.get());
  X509_VERIFY_PARAM* verify_param =
      X509_STORE_CTX_get0_param(bssl_store_ctx.get());
  X509_VERIFY_PARAM_set_flags(verify_param, 0);
  // NOTE(btolsch): This is an unfortunate conversion since GENERALIZEDTIME is
  // _not_ natively in seconds-since-UNIX-epoch.
  X509_VERIFY_PARAM_set_time(verify_param, DateTimeToSeconds(time).count());

  int ret = X509_verify_cert(bssl_store_ctx.get());

  STACK_OF(X509)* chain = X509_STORE_CTX_get0_chain(bssl_store_ctx.get());
  size_t chain_length = sk_X509_num(chain);
  if (ret == 1) {
    // NOTE(btolsch): Boringssl doesn't enforce a pathlen constraint from the
    // root certificate, but Cast wants to.
    bssl::UniquePtr<BASIC_CONSTRAINTS> basic_constraints =
        GetConstraints(sk_X509_value(chain, chain_length - 1));
    if (basic_constraints && basic_constraints->pathlen) {
      if (basic_constraints->pathlen->length != 1) {
        return Error::Code::kErrCertsVerifyGeneric;
      } else {
        int pathlen = *basic_constraints->pathlen->data;
        if ((pathlen < 0) || ((static_cast<int>(chain_length) - 2) > pathlen)) {
          return Error::Code::kErrCertsPathlen;
        }
      }
    }
  } else {
    int err = X509_STORE_CTX_get_error(bssl_store_ctx.get());
    if (err == X509_V_ERR_CERT_HAS_EXPIRED ||
        err == X509_V_ERR_CERT_NOT_YET_VALID) {
      return Error::Code::kErrCertsDateInvalid;
    } else if (err == X509_V_ERR_PATH_LENGTH_EXCEEDED) {
      return Error::Code::kErrCertsPathlen;
    } else if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
      return Error::Code::kErrCertsVerifyUntrustedCert;
    }
    return Error::Code::kErrCertsVerifyGeneric;
  }

  result_path->path.reserve(chain_length);
  for (uint32_t i = 0; i < chain_length; ++i) {
    result_path->path.push_back(sk_X509_value(chain, chain_length - i - 1));
  }

  OSP_VLOG
      << "FindCertificatePath: Succeeded at validating receiver certificates";
  return Error::Code::kNone;
}

}  // namespace cast
}  // namespace openscreen
