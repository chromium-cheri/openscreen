// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <time.h>

#include <chrono>
#include <utility>

#include "absl/strings/str_cat.h"
#include "platform/api/logging.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

namespace {

// OpenSSL methods return either 0 or nullptr when any errors occur.
#define OPENSSL_CHECK(return_code)                        \
  {                                                       \
    if (return_code == 0) {                               \
      OSP_LOG_WARN << "OpenSSL function error occurred."; \
      return nullptr;                                     \
    }                                                     \
  }

// Returns whether or not the certificate field successfully was added.
bool AddCertificateField(X509_NAME* certificate_name,
                         absl::string_view field,
                         absl::string_view value) {
  return X509_NAME_add_entry_by_txt(
             certificate_name, std::string(field).c_str(), MBSTRING_ASC,
             reinterpret_cast<const unsigned char*>(value.data()),
             value.length(), -1, 0) == 1;
}

bssl::UniquePtr<ASN1_TIME> ToAsn1Time(const Clock::time_point time) {
  // We don't have access to system_clock::to_time_t.
  const std::time_t time_as_t =
      std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch())
          .count();

  return bssl::UniquePtr<ASN1_TIME>(ASN1_TIME_set(nullptr, time_as_t));
}

bssl::UniquePtr<X509> CreateCertificate(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    EVP_PKEY key_pair,
    const Clock::time_point now_time_point) {
  bssl::UniquePtr<X509> certificate(X509_new());

  // Serial numbers must be unique for this session. As a pretend CA, we should
  // not issue certificates with the same serial number in the same session.
  static std::atomic_int serial_number(1);
  OPENSSL_CHECK(ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()),
                                 serial_number++));

  const bssl::UniquePtr<ASN1_TIME> now(ToAsn1Time(now_time_point));
  const bssl::UniquePtr<ASN1_TIME> expiration_time(
      ToAsn1Time(now_time_point + certificate_duration));

  OPENSSL_CHECK(X509_set_notBefore(certificate.get(), now.get()));
  OPENSSL_CHECK(X509_set_notAfter(certificate.get(), expiration_time.get()));

  X509_NAME* certificate_name = X509_get_subject_name(certificate.get());

  if (!AddCertificateField(certificate_name, "CN", name)) {
    return nullptr;
  }

  OPENSSL_CHECK(X509_set_issuer_name(certificate.get(), certificate_name));
  OPENSSL_CHECK(X509_set_pubkey(certificate.get(), &key_pair));
  OPENSSL_CHECK(X509_sign(certificate.get(), &key_pair, EVP_sha256()));
  OPENSSL_CHECK(X509_verify(certificate.get(), &key_pair));
  return certificate;
}

ErrorOr<std::vector<uint8_t>> Sha256Digest(std::vector<uint8_t> data) {
  bssl::UniquePtr<EVP_MD_CTX> context(EVP_MD_CTX_new());
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  if (!EVP_Digest(data.data(), data.size(), digest, &digest_len, EVP_sha256(),
                  nullptr)) {
    return Error::Code::kParseError;
  }

  return std::vector<uint8_t>(digest, digest + digest_len);
}

ErrorOr<std::vector<uint8_t>> WriteOutKey(EVP_PKEY key, bool is_public) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));

  if (is_public) {
    if (!PEM_write_bio_PUBKEY(bio.get(), &key)) {
      return Error::Code::kParseError;
    }
  } else {
    if (!PEM_write_bio_PrivateKey(
            bio.get(), &key, nullptr /* EVP_CIPHER */, nullptr /* kstr */,
            -1 /* klen */, nullptr /* pem_password_cb */, nullptr /* u */)) {
      return Error::Code::kParseError;
    }
  }

  char* data = nullptr;
  const int len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0) {
    return Error::Code::kParseError;
  }

  return std::vector<uint8_t>(data, data + len);
}

}  // namespace

ErrorOr<TlsCredentials> TlsCredentials::Create(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    ClockNowFunctionPtr now_function,
    bssl::UniquePtr<EVP_PKEY> key_pair) {
  EnsureOpenSSLInit();

  bssl::UniquePtr<X509> certificate =
      CreateCertificate(name, certificate_duration, *key_pair, now_function());

  if (!certificate) {
    return Error::Code::kItemNotFound;
  }

  return TlsCredentials(std::move(certificate), std::move(key_pair));
}

const ErrorOr<std::vector<uint8_t>>& TlsCredentials::private_key_base64() {
  if (!private_key_base64_) {
    private_key_base64_ = std::make_unique<ErrorOr<std::vector<uint8_t>>>(
        WriteOutKey(*key_pair_, false /* is_public */));
  }

  return *private_key_base64_;
}

const ErrorOr<std::vector<uint8_t>>& TlsCredentials::public_key_base64() {
  if (!public_key_base64_) {
    public_key_base64_ = std::make_unique<ErrorOr<std::vector<uint8_t>>>(
        WriteOutKey(*key_pair_, true /* is_public */));
  }

  return *public_key_base64_;
}

const ErrorOr<std::vector<uint8_t>>& TlsCredentials::public_key_hash() {
  if (!public_key_hash_) {
    const ErrorOr<std::vector<uint8_t>>& public_key = public_key_base64();

    if (public_key.is_error() || public_key.value().empty()) {
      public_key_hash_ = std::make_unique<ErrorOr<std::vector<uint8_t>>>(
          Error::Code::kParameterInvalid);

      return *public_key_hash_;
    }

    public_key_hash_ = std::make_unique<ErrorOr<std::vector<uint8_t>>>(
        Sha256Digest(public_key.value()));
  }

  return *public_key_hash_;
}

const ErrorOr<std::vector<uint8_t>>& TlsCredentials::raw_der_certificate() {
  if (!raw_der_certificate_) {
    unsigned char* buffer = nullptr;
    const int len = i2d_X509(certificate_.get(), &buffer);
    if (len <= 0) {
      raw_der_certificate_ = std::make_unique<ErrorOr<std::vector<uint8_t>>>(
          Error::Code::kItemNotFound);
      return *raw_der_certificate_;
    }

    raw_der_certificate_ = std::make_unique<ErrorOr<std::vector<uint8_t>>>(
        std::vector<uint8_t>(buffer, buffer + len));

    // TODO(jophba): remove when BoringSSL complies with OpenSSL's definition
    // of a temporary buffer. In OpenSSL, i2d_X509 uses a temporary buffer
    // that should not be freed.
    OPENSSL_free(buffer);
  }

  return *raw_der_certificate_;
}

TlsCredentials::TlsCredentials(bssl::UniquePtr<X509> certificate,
                               bssl::UniquePtr<EVP_PKEY> key_pair)
    : certificate_(std::move(certificate)), key_pair_(std::move(key_pair)) {}

}  // namespace platform
}  // namespace openscreen
