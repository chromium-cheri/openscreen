// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TLS_CREDENTIALS_H_
#define PLATFORM_BASE_TLS_CREDENTIALS_H_

#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TlsCredentials {
 public:
  // Empty constructor is used for ErrorOr.
  TlsCredentials() = default;

  // We are move only due to unique pointers.
  TlsCredentials(TlsCredentials&&) = default;
  TlsCredentials& operator=(TlsCredentials&&) = default;

  // TlsCredentials generates a self signed certificate given (1) the name
  // to use for the certificate, (2) the length of time the certificate will
  // be valid, and (3) a private/public key_pair.
  static ErrorOr<TlsCredentials> Create(
      absl::string_view name,
      std::chrono::seconds certificate_duration,
      ClockNowFunctionPtr now_function,
      bssl::UniquePtr<EVP_PKEY> key_pair);

  // The OpenSSL encoded self signed certificate.
  const X509& certificate() const { return *certificate_; }

  // The original key pair provided on construction.
  const EVP_PKEY& key_pair() const { return *key_pair_; }

  // A base64 encoded version of the private/public key_pair's private key
  // provided on construction.
  const ErrorOr<std::vector<uint8_t>>& private_key_base64();

  // A base64 encoded version of the private/public key_pair's associated public
  // key.
  const ErrorOr<std::vector<uint8_t>>& public_key_base64();

  // A SHA-256 digest of the private/public key_pair's associated public key.
  const ErrorOr<std::vector<uint8_t>>& public_key_hash();

  // The raw, generated self signed certficate.
  const ErrorOr<std::vector<uint8_t>>& raw_der_certificate();

 private:
  TlsCredentials(bssl::UniquePtr<X509> certificate,
                 bssl::UniquePtr<EVP_PKEY> key_pair);

  bssl::UniquePtr<X509> certificate_;
  bssl::UniquePtr<EVP_PKEY> key_pair_;

  // These member variables are initialized upon the first call of their
  // respective public getter method.
  std::unique_ptr<ErrorOr<std::vector<uint8_t>>> private_key_base64_;
  std::unique_ptr<ErrorOr<std::vector<uint8_t>>> public_key_base64_;
  std::unique_ptr<ErrorOr<std::vector<uint8_t>>> public_key_hash_;
  std::unique_ptr<ErrorOr<std::vector<uint8_t>>> raw_der_certificate_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TLS_CREDENTIALS_H_
