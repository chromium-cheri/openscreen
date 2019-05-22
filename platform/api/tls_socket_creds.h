// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jophba): Need unittests.

#ifndef PLATFORM_API_TLS_SOCKET_CREDS_H_
#define PLATFORM_API_TLS_SOCKET_CREDS_H_

#include <memory>
#include <string>
#include <vector>

#include "osp_base/macros.h"

namespace openscreen {

// TODO(jophba): flesh out once we have the net/crypto deps completed
class TlsSocketCreds {
 public:
  TlsSocketCreds();
  virtual ~TlsSocketCreds();

  static std::unique_ptr<TlsSocketCreds> Create(const std::string& common_name,
                                                int64_t cert_duration_secs);
  static std::unique_ptr<TlsSocketCreds> CreateWithKey(
      const std::string& common_name,
      int64_t cert_duration_secs,
      const std::string& key_bin);

  std::unique_ptr<TlsSocketCreds> Clone() const;

  const std::string& tls_cert_der() const { return tls_cert_der_; }
  const std::string& private_key_base64() const { return private_key_base64_; }
  const std::string& public_key_base64() const { return public_key_base64_; }
  const std::string& tls_pk_sha256_hash() const { return tls_pk_sha256_hash_; }

 private:
  // TODO(jophba): add TLS certificate
  // TODO(jophba): add RSA Private key
  std::string tls_cert_der_;
  std::string tls_pk_sha256_hash_;

  std::string private_key_base64_;
  std::string public_key_base64_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsSocketCreds);
};

}  // namespace openscreen

#endif  // PLATFORM_API_TLS_SOCKET_CREDS_H_
