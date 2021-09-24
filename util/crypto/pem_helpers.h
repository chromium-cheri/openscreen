// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CRYPTO_PEM_HELPERS_H_
#define UTIL_CRYPTO_PEM_HELPERS_H_

#include <openssl/evp.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace openscreen {

// Reads one or more X509 certificates from the file at |filename|.  |filename|
// can be an absolute or relative pathname.
std::vector<std::vector<uint8_t>> ReadCertificatesFromPemFile(
    absl::string_view filename);

// Reads exactly one X509 certificates from the file at |filename|.  |filename|
// can be an absolute or relative pathname.
std::vector<uint8_t> ReadCertificateFromPemFile(absl::string_view filename);

// Reads an EVP_PKEY object from the file at |filename|.  |filename| can be an
// absolute or relative pathname.
bssl::UniquePtr<EVP_PKEY> ReadKeyFromPemFile(absl::string_view filename);

}  // namespace openscreen

#endif  // UTIL_CRYPTO_PEM_HELPERS_H_
