// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <time.h>

#include <utility>

#include "absl/strings/str_cat.h"
#include "platform/api/logging.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {

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

bssl::UniquePtr<X509> CreateCertificate(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    EVP_PKEY keypair) {
  bssl::UniquePtr<X509> certificate(X509_new());

  // Serial numbers must be unique.
  static int serial_number = 1;
  OPENSSL_CHECK(ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()),
                                 serial_number++));

  time_t current_time = time(nullptr);
  bssl::UniquePtr<ASN1_TIME> now(ASN1_TIME_adj(nullptr, current_time, 0, 0));
  bssl::UniquePtr<ASN1_TIME> expiration_time(
      ASN1_TIME_adj(nullptr, current_time, 0, certificate_duration.count()));

  OPENSSL_CHECK(X509_set_notBefore(certificate.get(), now.get()));
  OPENSSL_CHECK(X509_set_notAfter(certificate.get(), expiration_time.get()));

  X509_NAME* certificate_name = X509_get_subject_name(certificate.get());

  if (!AddCertificateField(certificate_name, "CN", name)) {
    return nullptr;
  }

  OPENSSL_CHECK(X509_set_issuer_name(certificate.get(), certificate_name));
  OPENSSL_CHECK(X509_set_pubkey(certificate.get(), &keypair));
  OPENSSL_CHECK(X509_sign(certificate.get(), &keypair, EVP_sha256()));
  OPENSSL_CHECK(X509_verify(certificate.get(), &keypair));
  return certificate;
}

std::vector<uint8_t> Sha256Digest(std::vector<uint8_t> data) {
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
  EVP_DigestUpdate(context, data.data(), data.size());

  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  EVP_DigestFinal_ex(context, digest, &digest_len);
  EVP_MD_CTX_free(context);

  return std::vector<uint8_t>(digest, digest + digest_len);
}

std::vector<uint8_t> WriteOutKey(EVP_PKEY key, bool is_public) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));

  if (is_public) {
    if (!PEM_write_bio_PUBKEY(bio.get(), &key)) {
      return {};
    }
  } else {
    if (!PEM_write_bio_PrivateKey(
            bio.get(), &key, nullptr /* EVP_CIPHER */, nullptr /* kstr */,
            -1 /* klen */, nullptr /* pem_password_cb */, nullptr /* u */)) {
      return {};
    }
  }

  char* data = nullptr;
  const int len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0) {
    return {};
  }

  return std::vector<uint8_t>(data, data + len);
}

}  // namespace

ErrorOr<TlsCredentials> TlsCredentials::Create(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    bssl::UniquePtr<EVP_PKEY> keypair) {
  EnsureOpenSSLInit();

  bssl::UniquePtr<X509> certificate =
      CreateCertificate(name, certificate_duration, *keypair);

  return TlsCredentials(std::move(certificate), std::move(keypair));
}

const std::vector<uint8_t>& TlsCredentials::private_key_base64() {
  if (!private_key_base64_) {
    private_key_base64_ = std::make_unique<std::vector<uint8_t>>(
        WriteOutKey(*keypair_, false /* is_public */));
  }

  return *private_key_base64_;
}

const std::vector<uint8_t>& TlsCredentials::public_key_base64() {
  if (!public_key_base64_) {
    public_key_base64_ = std::make_unique<std::vector<uint8_t>>(
        WriteOutKey(*keypair_, true /* is_public */));
  }

  return *public_key_base64_;
}

const std::vector<uint8_t>& TlsCredentials::public_key_hash() {
  if (!public_key_hash_) {
    const std::vector<uint8_t>& public_key = public_key_base64();

    public_key_hash_ =
        std::make_unique<std::vector<uint8_t>>(Sha256Digest(public_key));
  }

  return *public_key_hash_;
}

const std::vector<uint8_t>& TlsCredentials::raw_der_certificate() {
  if (!raw_der_certificate_) {
    unsigned char* buffer = nullptr;
    const int len = i2d_X509(certificate_.get(), &buffer);
    if (len <= 0) {
      raw_der_certificate_ = std::make_unique<std::vector<uint8_t>>();
      return *raw_der_certificate_;
    }

    raw_der_certificate_ =
        std::make_unique<std::vector<uint8_t>>(buffer, buffer + len);

    // TODO(jophba): remove when we update BoringSSL to a more modern version.
    // In more modern versions of OpenSSL, i2d_X509 uses a temporary buffer
    // that should not be freed.
   OPENSSL_free(buffer);
  }

  return *raw_der_certificate_;
}

TlsCredentials::TlsCredentials(bssl::UniquePtr<X509> certificate,
                               bssl::UniquePtr<EVP_PKEY> keypair)
    : certificate_(std::move(certificate)),
      keypair_(std::move(keypair))

{}

}  // namespace openscreen
