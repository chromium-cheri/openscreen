// Copyright (c) 2013 Google Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_socket_creds.h"

#include <memory>
#include <utility>

namespace openscreen {
TlsSocketCreds::TlsSocketCreds() = default;
TlsSocketCreds::~TlsSocketCreds() = default;

// static
std::unique_ptr<TlsSocketCreds> TlsSocketCreds::Create(
    const std::string& common_name,
    int64_t cert_duration_secs) {
  // TODO(jophba): implement once we have RSA keys here.
  return nullptr;
}

// static
std::unique_ptr<TlsSocketCreds> TlsSocketCreds::CreateWithKey(
    const std::string& common_name,
    int64_t cert_duration_secs,
    const std::string& key_bin) {
  // TODO(jophba): implement once we have RSA keys here.
  return nullptr;
}

std::unique_ptr<TlsSocketCreds> TlsSocketCreds::Clone() const {
  auto creds = std::make_unique<TlsSocketCreds>();
  creds->tls_cert_der_ = tls_cert_der_;
  creds->private_key_base64_ = private_key_base64_;
  creds->public_key_base64_ = public_key_base64_;
  return creds;
}

}  // namespace openscreen
