// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_crypto/sha2.h"

#include <stddef.h>

#include <memory>

#include "osp_base/std_util.h"
#include "osp_crypto/secure_hash.h"

namespace openscreen {

void SHA256HashString(absl::string_view str, void* output, size_t len) {
  std::unique_ptr<SecureHash> ctx(SecureHash::Create(SecureHash::SHA256));
  ctx->Update(str.data(), str.length());
  ctx->Finish(output, len);
}

std::string SHA256HashString(absl::string_view str) {
  std::string output(kSHA256Length, 0);
  SHA256HashString(str, data(output), output.size());
  return output;
}

}  // namespace openscreen
