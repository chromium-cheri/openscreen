// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_SSL_CONTEXT_H_
#define PLATFORM_BASE_SSL_CONTEXT_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace openscreen {
template <typename Value>
class ErrorOr;

namespace platform {
void SSLDeleter(SSL* ssl);

class SSLContext {
 public:
  static ErrorOr<SSLContext> Create(absl::string_view cert_filename,
                                    absl::string_view key_filename);

  // Since we manage the internal context, we must be move only.
  SSLContext() = default;
  SSLContext(SSLContext&&);
  SSLContext(const SSLContext&) = delete;

  SSLContext& operator=(SSLContext&&);
  SSLContext& operator=(const SSLContext&) = delete;

  virtual ~SSLContext();

  std::unique_ptr<SSL, decltype(&SSLDeleter)> GetNewSSL() const;

 private:
  SSL_CTX* context_ = nullptr;
};
}  // namespace platform
}  // namespace openscreen
#endif  // PLATFORM_BASE_SSL_CONTEXT_H_
