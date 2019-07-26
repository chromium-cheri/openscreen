// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/cast_cert_validator.h"

#include <openssl/digest.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "cast/common/certificate/cast_cert_validator_internal.h"

namespace cast {
namespace certificate {
namespace {

// -------------------------------------------------------------------------
// Cast trust anchors.
// -------------------------------------------------------------------------

// There are two trusted roots for Cast certificate chains:
//
//   (1) CN=Cast Root CA    (kCastRootCaDer)
//   (2) CN=Eureka Root CA  (kEurekaRootCaDer)
//
// These constants are defined by the files included next:

#include "cast/common/certificate/cast_root_ca_cert_der-inc.h"
#include "cast/common/certificate/eureka_root_ca_der-inc.h"

constexpr static int32_t kMinRsaModulusLengthBits = 2048;

// Singleton for the Cast trust store.
class CastTrustStore {
 public:
  static CastTrustStore* GetInstance() {
    static CastTrustStore* store = new CastTrustStore();
    return store;
  }

  static TrustStore* Get() { return &GetInstance()->trust_store_; }

  CastTrustStore() {
    trust_store_.certs.emplace_back(MakeTrustAnchor(kCastRootCaDer));
    trust_store_.certs.emplace_back(MakeTrustAnchor(kEurekaRootCaDer));
  }

  // Adds a trust anchor given a DER-encoded certificate from static
  // storage.
  template <size_t N>
  bssl::UniquePtr<X509> MakeTrustAnchor(const uint8_t (&data)[N]) {
    const uint8_t* dptr = data;
    return bssl::UniquePtr<X509>{d2i_X509(nullptr, &dptr, N)};
  }

  TrustStore trust_store_;
  OSP_DISALLOW_COPY_AND_ASSIGN(CastTrustStore);
};

struct CertPathStep {
  X509* cert;
  uint32_t trust_store_i;
  uint32_t intermediate_cert_i;
};

enum KeyUsageBits {
  kDigitalSignature = 0,
};

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
  if (!EVP_DigestVerifyUpdate(ctx.get(), data.data, data.length)) {
    return false;
  }
  return (EVP_DigestVerifyFinal(ctx.get(), signature.data, signature.length) ==
          1);
}

// Returns the OID for the Audio-Only Cast policy
// (1.3.6.1.4.1.11129.2.5.2) in DER form.
ConstDataSpan AudioOnlyPolicyOid() {
  static const uint8_t kAudioOnlyPolicy[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                             0xD6, 0x79, 0x02, 0x05, 0x02};
  return ConstDataSpan{kAudioOnlyPolicy, sizeof(kAudioOnlyPolicy)};
}

class CertVerificationContextImpl final : public CertVerificationContext {
 public:
  CertVerificationContextImpl(X509* cert, std::string&& common_name)
      : public_key_{X509_get_pubkey(cert)},
        common_name_(std::move(common_name)) {}

  ~CertVerificationContextImpl() override = default;

  bool VerifySignatureOverData(
      const ConstDataSpan& signature,
      const ConstDataSpan& data,
      DigestAlgorithm digest_algorithm) const override {
    const EVP_MD* digest;
    switch (digest_algorithm) {
      case DigestAlgorithm::SHA1:
        digest = EVP_sha1();
        break;
      case DigestAlgorithm::SHA256:
        digest = EVP_sha256();
        break;
      case DigestAlgorithm::SHA384:
        digest = EVP_sha384();
        break;
      case DigestAlgorithm::SHA512:
        digest = EVP_sha512();
        break;
      default:
        return false;
    }

    return VerifySignedData(digest, public_key_.get(), data, signature);
  }

  const std::string& GetCommonName() const override { return common_name_; }

 private:
  const bssl::UniquePtr<EVP_PKEY> public_key_;
  const std::string common_name_;
};

bool CertInPath(X509_NAME* name,
                const CertPathStep* steps,
                uint32_t step_count) {
  for (uint32_t i = 0; i < step_count; ++i) {
    if (X509_NAME_cmp(name, X509_get_subject_name(steps[i].cert)) == 0) {
      return true;
    }
  }
  return false;
}

// Parses GeneralizedTime with additional restrictions laid out by RFC 5280
// 4.1.2.5.2.
bool ToGeneralizedTime(ASN1_GENERALIZEDTIME* time, GeneralizedTime* out) {
  static constexpr uint8_t kDaysPerMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };

  if (time->length != 15) {
    return false;
  }
  if (time->data[14] != 'Z') {
    return false;
  }
  for (int i = 0; i < 14; ++i) {
    if (time->data[i] < '0' || time->data[i] > '9') {
      return false;
    }
  }
  out->year = (time->data[0] - '0') * 1000 + (time->data[1] - '0') * 100 +
              (time->data[2] - '0') * 10 + (time->data[3] - '0');
  out->month = (time->data[4] - '0') * 10 + (time->data[5] - '0');
  out->day = (time->data[6] - '0') * 10 + (time->data[7] - '0');
  out->hour = (time->data[8] - '0') * 10 + (time->data[9] - '0');
  out->minute = (time->data[10] - '0') * 10 + (time->data[11] - '0');
  out->second = (time->data[12] - '0') * 10 + (time->data[13] - '0');
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

bool IsTimeBefore(const GeneralizedTime& a, const GeneralizedTime& b) {
  if (a.year < b.year) {
    return true;
  } else if (a.year > b.year) {
    return false;
  }
  if (a.month < b.month) {
    return true;
  } else if (a.month > b.month) {
    return false;
  }
  if (a.day < b.day) {
    return true;
  } else if (a.day > b.day) {
    return false;
  }
  if (a.hour < b.hour) {
    return true;
  } else if (a.hour > b.hour) {
    return false;
  }
  if (a.minute < b.minute) {
    return true;
  } else if (a.minute > b.minute) {
    return false;
  }
  if (a.second < b.second) {
    return true;
  } else if (a.second > b.second) {
    return false;
  }
  return false;
}

bool VerifyCertTime(X509* cert,
                    const GeneralizedTime& time,
                    CastCertError* error) {
  ASN1_GENERALIZEDTIME* not_before_asn1 = ASN1_TIME_to_generalizedtime(
      cert->cert_info->validity->notBefore, nullptr);
  ASN1_GENERALIZEDTIME* not_after_asn1 = ASN1_TIME_to_generalizedtime(
      cert->cert_info->validity->notAfter, nullptr);
  if (!not_before_asn1 || !not_after_asn1) {
    *error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
    return false;
  }
  GeneralizedTime not_before;
  GeneralizedTime not_after;
  bool times_valid;
  times_valid = ToGeneralizedTime(not_before_asn1, &not_before);
  times_valid = times_valid && ToGeneralizedTime(not_after_asn1, &not_after);
  ASN1_GENERALIZEDTIME_free(not_before_asn1);
  ASN1_GENERALIZEDTIME_free(not_after_asn1);
  if (!times_valid) {
    *error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
    return false;
  }
  if (IsTimeBefore(time, not_before) || IsTimeBefore(not_after, time)) {
    *error = CastCertError::ERR_CERTS_DATE_INVALID;
    return false;
  }
  return true;
}

bool VerifyPublicKeyLength(EVP_PKEY* public_key) {
  return EVP_PKEY_bits(public_key) >= kMinRsaModulusLengthBits;
}

}  // namespace

CastCertError VerifyDeviceCert(
    const std::vector<std::string>& certs,
    const GeneralizedTime& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy) {
  return VerifyDeviceCertUsingCustomTrustStore(
      certs, time, context, policy, crl, crl_policy, CastTrustStore::Get());
}

CastCertError VerifyDeviceCertUsingCustomTrustStore(
    const std::vector<std::string>& certs,
    const GeneralizedTime& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy,
    TrustStore* trust_store) {
  if (!trust_store)
    // TODO(btolsch): Don't do this, here and elsewhere.
    return VerifyDeviceCert(certs, time, context, policy, crl, crl_policy);

  if (certs.empty())
    return CastCertError::ERR_CERTS_MISSING;

  // Fail early if CRL is required but not provided.
  if (!crl && crl_policy == CRLPolicy::CRL_REQUIRED)
    return CastCertError::ERR_CRL_INVALID;

  bssl::UniquePtr<X509> target_cert;
  std::vector<bssl::UniquePtr<X509>> intermediate_certs;
  for (size_t i = 0; i < certs.size(); ++i) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(certs[i].data());
    X509* cert = d2i_X509(nullptr, &data, certs[i].size());
    if (!cert) {
      return CastCertError::ERR_CERTS_PARSE;
    }
    if (i == 0) {
      target_cert.reset(cert);
    } else {
      intermediate_certs.emplace_back(cert);
    }
  }

  X509* path_head = target_cert.get();
  std::vector<CertPathStep> path;
  path.resize(intermediate_certs.size() + 2);
  path[path.size() - 1].cert = path_head;
  bool hit_trust = false;
  uint32_t step_index = path.size() - 1;
  uint32_t trust_store_i = 0;
  uint32_t intermediate_cert_i = 0;
  CastCertError last_error = CastCertError::OK;
  for (;;) {
    X509_NAME* target_issuer_name = X509_get_issuer_name(path_head);
    X509* next_issuer = nullptr;
    for (uint32_t i = trust_store_i; i < trust_store->certs.size(); ++i) {
      X509* cert = trust_store->certs[i].get();
      // TODO(btolsch): Does this + other name comparisons need to be converted
      // to "normalizing", or does X509_NAME already handle that?
      X509_NAME* name = X509_get_subject_name(cert);
      if (X509_NAME_cmp(name, target_issuer_name) == 0 &&
          !CertInPath(name, &path[step_index], path.size() - 1 - step_index)) {
        CertPathStep& next_step = path[--step_index];
        next_step.cert = cert;
        next_step.trust_store_i = i + 1;
        next_step.intermediate_cert_i = 0;
        next_issuer = cert;
        hit_trust = true;
        break;
      }
    }
    trust_store_i = 0;
    if (!next_issuer) {
      for (uint32_t i = intermediate_cert_i; i < intermediate_certs.size();
           ++i) {
        X509* cert = intermediate_certs[i].get();
        X509_NAME* name = X509_get_subject_name(cert);
        if (X509_NAME_cmp(name, target_issuer_name) == 0 &&
            !CertInPath(name, &path[step_index],
                        path.size() - 1 - step_index)) {
          CertPathStep& next_step = path[--step_index];
          next_step.cert = cert;
          next_step.trust_store_i = trust_store->certs.size();
          next_step.intermediate_cert_i = i + 1;
          next_issuer = cert;
          break;
        }
      }
    }
    intermediate_cert_i = 0;
    if (!next_issuer) {
      if (step_index == (path.size() - 1)) {
        if (last_error == CastCertError::OK) {
          return CastCertError::ERR_CERTS_VERIFY_GENERIC;
        }
        return last_error;
      } else {
        CertPathStep& bad_step = path[step_index++];
        trust_store_i = bad_step.trust_store_i;
        intermediate_cert_i = bad_step.intermediate_cert_i;
        continue;
      }
    }

    // TODO(btolsch): Verify 4.1.1.2
    // TODO(btolsch): unless not target + self-issued, verify subject name in
    // 'permitted_subtress' and not within 'excluded_subtrees'
    // TODO(btolsch): 6.1.3f
    // TODO(btolsch): 6.1.4a,k,l,n,o
    // TODO(btolsch): 6.1.5: wrap-up
    // TODO(btolsch): Check against revocation list
    if (hit_trust) {
      CastCertError error = CastCertError::OK;
      X509* root = path[step_index].cert;
      int basic_constraints_index =
          X509_get_ext_by_NID(root, NID_basic_constraints, -1);
      uint32_t pathlen = 0;
      if (basic_constraints_index != -1) {
        X509_EXTENSION* basic_constraints_extension =
            X509_get_ext(root, basic_constraints_index);
        const uint8_t* in = basic_constraints_extension->value->data;
        bssl::UniquePtr<BASIC_CONSTRAINTS> basic_constraints{
            d2i_BASIC_CONSTRAINTS(nullptr, &in,
                                  basic_constraints_extension->value->length)};
        if (basic_constraints && basic_constraints->ca) {
          // TODO(btolsch): if key usage + keyCertSign
          if (basic_constraints->pathlen) {
            if (basic_constraints->pathlen->length > 1) {
              error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
            } else {
              pathlen = *basic_constraints->pathlen->data;
              // TODO(btolsch): Exclude self-issued from pathlen.
              if ((pathlen + 2) < (path.size() - step_index)) {
                error = CastCertError::ERR_CERTS_PATHLEN;
              }
            }
          }
        } else {
          error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
        }
      } else {
        error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
      }
      uint32_t i = step_index;
      for (; i < path.size() - 1 && error == CastCertError::OK; ++i) {
        X509* subject = path[i + 1].cert;
        X509* issuer = path[i].cert;
        if (i != step_index) {
          if (!VerifyCertTime(issuer, time, &error)) {
            return error;
          }
        }
        bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(issuer)};
        if (!VerifyPublicKeyLength(public_key.get())) {
          return CastCertError::ERR_CERTS_VERIFY_GENERIC;
        }
        int nid = OBJ_obj2nid(subject->sig_alg->algorithm);
        const EVP_MD* digest;
        switch (nid) {
          case NID_sha1WithRSAEncryption:
            digest = EVP_sha1();
            break;
          case NID_sha256WithRSAEncryption:
            digest = EVP_sha256();
            break;
          case NID_sha384WithRSAEncryption:
            digest = EVP_sha384();
            break;
          case NID_sha512WithRSAEncryption:
            digest = EVP_sha512();
            break;
          default:
            return CastCertError::ERR_CERTS_VERIFY_GENERIC;
        }
        if (!VerifySignedData(
                digest, public_key.get(),
                {subject->cert_info->enc.enc,
                 static_cast<uint32_t>(subject->cert_info->enc.len)},
                {subject->signature->data,
                 static_cast<uint32_t>(subject->signature->length)})) {
          break;
        }
      }
      if (i != (path.size() - 1) || error != CastCertError::OK) {
        if (error == CastCertError::OK) {
          error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
        }
        CertPathStep& bad_step = path[step_index++];
        trust_store_i = bad_step.trust_store_i;
        intermediate_cert_i = bad_step.intermediate_cert_i;
        hit_trust = false;
      } else {
        if (VerifyCertTime(target_cert.get(), time, &error)) {
          bssl::UniquePtr<EVP_PKEY> public_key{
              X509_get_pubkey(target_cert.get())};
          if (VerifyPublicKeyLength(public_key.get())) {
            last_error = CastCertError::OK;
            break;
          } else {
            error = CastCertError::ERR_CERTS_VERIFY_GENERIC;
            CertPathStep& bad_step = path[step_index++];
            trust_store_i = bad_step.trust_store_i;
            intermediate_cert_i = bad_step.intermediate_cert_i;
            hit_trust = false;
          }
        } else {
          CertPathStep& bad_step = path[step_index++];
          trust_store_i = bad_step.trust_store_i;
          intermediate_cert_i = bad_step.intermediate_cert_i;
          hit_trust = false;
        }
      }
      last_error = error;
    }
    path_head = next_issuer;
  }

  if (last_error != CastCertError::OK) {
    return last_error;
  }

  // Cast device certificates use the policy 1.3.6.1.4.1.11129.2.5.2 to indicate
  // it is *restricted* to an audio-only device whereas the absence of a policy
  // means it is unrestricted.
  //
  // This is somewhat different than RFC 5280's notion of policies, so policies
  // are checked separately outside of path building.
  //
  // See the unit-tests VerifyCastDeviceCertTest.Policies* for some
  // concrete examples of how this works.
  //
  // Iterate over all the certificates, including the root certificate. If any
  // certificate contains the audio-only policy, the whole chain is considered
  // constrained to audio-only device certificates.
  //
  // Policy mappings are not accounted for. The expectation is that top-level
  // intermediates issued with audio-only will have no mappings. If subsequent
  // certificates in the chain do, it won't matter as the chain is already
  // restricted to being audio-only.
  *policy = CastDeviceCertPolicy::NONE;
  for (uint32_t i = step_index;
       i < path.size() && *policy != CastDeviceCertPolicy::AUDIO_ONLY; ++i) {
    X509* cert = path[path.size() - 1 - i].cert;
    int pos = X509_get_ext_by_NID(cert, NID_certificate_policies, -1);
    if (pos != -1) {
      X509_EXTENSION* policies_extension = X509_get_ext(cert, pos);
      const uint8_t* in = policies_extension->value->data;
      CERTIFICATEPOLICIES* policies = d2i_CERTIFICATEPOLICIES(
          nullptr, &in, policies_extension->value->length);
      if (policies) {
        uint32_t policy_count = sk_POLICYINFO_num(policies);
        for (uint32_t i = 0; i < policy_count; ++i) {
          POLICYINFO* info = sk_POLICYINFO_value(policies, i);
          ConstDataSpan audio_only_policy_oid = AudioOnlyPolicyOid();
          if (info->policyid->length ==
                  static_cast<int>(audio_only_policy_oid.length) &&
              memcmp(info->policyid->data, audio_only_policy_oid.data,
                     audio_only_policy_oid.length) == 0) {
            *policy = CastDeviceCertPolicy::AUDIO_ONLY;
            break;
          }
        }
        CERTIFICATEPOLICIES_free(policies);
      }
    }
  }

  // Checks that the properties on the target certificate include a key usage of
  // 'digital signature'.
  int pos = X509_get_ext_by_NID(target_cert.get(), NID_key_usage, -1);
  if (pos == -1) {
    return CastCertError::ERR_CERTS_RESTRICTIONS;
  }
  X509_EXTENSION* key_usage = X509_get_ext(target_cert.get(), pos);
  const uint8_t* value = key_usage->value->data;
  ASN1_BIT_STRING* key_usage_bit_string = nullptr;
  if (!d2i_ASN1_BIT_STRING(&key_usage_bit_string, &value,
                           key_usage->value->length)) {
    return CastCertError::ERR_CERTS_RESTRICTIONS;
  }
  int bit = ASN1_BIT_STRING_get_bit(key_usage_bit_string,
                                    KeyUsageBits::kDigitalSignature);
  ASN1_BIT_STRING_free(key_usage_bit_string);
  if (!bit) {
    return CastCertError::ERR_CERTS_RESTRICTIONS;
  }

  X509_NAME* target_subject = X509_get_subject_name(target_cert.get());
  std::string common_name;
  common_name.resize(target_subject->canon_enclen);
  int len = X509_NAME_get_text_by_NID(target_subject, NID_commonName,
                                      &common_name[0], common_name.size());
  if (len == 0) {
    return CastCertError::ERR_CERTS_RESTRICTIONS;
  }
  common_name.resize(len);

  context->reset(new CertVerificationContextImpl(target_cert.get(),
                                                 std::move(common_name)));

  return CastCertError::OK;
}

}  // namespace certificate
}  // namespace cast
