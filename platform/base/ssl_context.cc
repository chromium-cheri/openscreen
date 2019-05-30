// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/ssl_context.h"

#include <stdio.h>

#include <string>
#include <utility>

#include "osp_base/error.h"
#include "platform/api/logging.h"
#include "third_party/boringssl/src/include/openssl/err.h"

namespace openscreen {
namespace platform {
namespace {

void InitOpenSSL() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

void CleanupOpenSSL() {
  EVP_cleanup();
}

void LogLastError() {
  OSP_LOG_ERROR << ERR_error_string(ERR_get_error(), nullptr);
}

SSL_CTX* CreateContext() {
  const SSL_METHOD* method;
  SSL_CTX* ctx;

  method = SSLv23_server_method();

  ctx = SSL_CTX_new(method);
  OSP_DCHECK(ctx);

  return ctx;
}

Error ConfigureContext(SSL_CTX* ctx,
                       absl::string_view cert_filename,
                       absl::string_view key_filename) {
  if (SSL_CTX_use_certificate_file(ctx, std::string(cert_filename).c_str(),
                                   SSL_FILETYPE_PEM) <= 0) {
    LogLastError();
    return Error::Code::kFileLoadFailure;
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, std::string(key_filename).c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    LogLastError();
    return Error::Code::kFileLoadFailure;
  }

  return Error::None();
}
}  // namespace

void SSLDeleter(SSL* ssl) {
  SSL_free(ssl);
}

ErrorOr<SSLContext> SSLContext::Create(absl::string_view cert_filename,
                                       absl::string_view key_filename) {
  InitOpenSSL();

  SSLContext context;
  context.context_ = CreateContext();
  Error error = ConfigureContext(context.context_, cert_filename, key_filename);
  if (!error.ok()) {
    return error;
  }
  return std::move(context);
}

SSLContext::SSLContext(SSLContext&& other) {
  *this = std::move(other);
}

SSLContext& SSLContext::operator=(SSLContext&& other) {
  context_ = other.context_;
  other.context_ = nullptr;

  return *this;
}

SSLContext::~SSLContext() {
  if (context_ != nullptr) {
    SSL_CTX_free(context_);
    context_ = nullptr;
    CleanupOpenSSL();
  }
}

std::unique_ptr<SSL, decltype(&SSLDeleter)> SSLContext::GetNewSSL() const {
  return std::unique_ptr<SSL, decltype(&SSLDeleter)>(SSL_new(context_),
                                                     SSLDeleter);
}
}  // namespace platform
}  // namespace openscreen
