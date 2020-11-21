// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/base64.h"

#include <stddef.h>

#include "third_party/modp_b64/modp_b64.h"
#include "util/std_util.h"

namespace openscreen {
namespace base64 {

namespace {
absl::Span<const uint8_t> ToSpan(absl::string_view input) {
  return absl::MakeConstSpan(reinterpret_cast<const uint8_t*>(input.begin()),
                             reinterpret_cast<const uint8_t*>(input.end()));
}
}  // namespace

std::string Encode(absl::Span<const uint8_t> input) {
  std::string output;
  output.resize(modp_b64_encode_len(input.size()));

  const size_t output_size = modp_b64_encode(
      data(output), reinterpret_cast<const char*>(input.data()), input.size());
  if (output_size == MODP_B64_ERROR) {
    return {};
  }

  output.resize(output_size);
  return output;
}

std::string Encode(absl::string_view input) {
  return Encode(ToSpan(input));
}

bool Decode(absl::string_view input, std::string* output) {
  std::string out;
  out.resize(modp_b64_decode_len(input.size()));

  // We don't null terminate the result since it is binary data.
  const size_t output_size =
      modp_b64_decode(data(out), input.data(), input.size());
  if (output_size == MODP_B64_ERROR) {
    return false;
  }

  out.resize(output_size);
  output->swap(out);
  return true;
}

}  // namespace base64
}  // namespace openscreen
