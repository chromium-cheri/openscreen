// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_PEM_HELPERS_H_
#define CAST_COMMON_CERTIFICATE_PEM_HELPERS_H_

#include <openssl/evp.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace openscreen {

std::vector<std::string> ReadCertificatesFromPemFile(
    absl::string_view filename);

bssl::UniquePtr<EVP_PKEY> ReadKeyFromPemFile(absl::string_view filename);

}  // namespace openscreen

#endif  // CAST_COMMON_CERTIFICATE_PEM_HELPERS_H_
