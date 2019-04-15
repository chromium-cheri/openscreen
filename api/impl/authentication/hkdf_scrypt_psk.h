// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_AUTHENTICATION_HKDF_SCRYPT_PSK_H_
#define API_IMPL_AUTHENTICATION_HKDF_SCRYPT_PSK_H_

#include <array>
#include <vector>

#include "base/error.h"

namespace openscreen {
namespace authentication {

ErrorOr<std::vector<uint8_t>> ComputeHkdfScryptPsk(const std::vector<uint8_t>& psk,
                  const std::vector<uint8_t>& salt,
                  uint64_t scrypt_cost,
                  const std::vector<uint8_t>& hkdf_info);

}  // namespace authentication
}  // namespace openscreen

#endif  // API_IMPL_AUTHENTICATION_HKDF_SCRYPT_PSK_H_