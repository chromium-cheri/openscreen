// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_FRAME_CRYPTO_H_
#define STREAMING_CAST_FRAME_CRYPTO_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <vector>

#include "base/macros.h"
#include "streaming/cast/encoded_frame.h"

namespace openscreen {
namespace cast_streaming {

// A subclass of EncodedFrame that represents an EncodedFrame with encrypted
// payload data. It can only be value-constructed by FrameCrypto, but can be
// moved freely thereafter. Use FrameCrypto (below) to explicitly convert
// between EncryptedFrames and EncodedFrames.
struct EncryptedFrame : public EncodedFrame {
  ~EncryptedFrame();
  EncryptedFrame(EncryptedFrame&&) MAYBE_NOEXCEPT;
  EncryptedFrame& operator=(EncryptedFrame&&) MAYBE_NOEXCEPT;

 private:
  friend class FrameCrypto;
  EncryptedFrame();

  DISALLOW_COPY_AND_ASSIGN(EncryptedFrame);
};

// Encrypts EncodedFrames before sending, or decrypts EncryptedFrames that have
// been received.
class FrameCrypto {
 public:
  using SixteenBytes = std::array<uint8_t, 16>;

  // Construct with the given 16-bytes AES key and IV mask. Both arguments
  // should be randomly-generated for each new streaming session.
  // GenerateRandomBytes() can be used to create them.
  FrameCrypto(SixteenBytes aes_key, SixteenBytes cast_iv_mask);

  ~FrameCrypto();

  EncryptedFrame Encrypt(const EncodedFrame& encoded_frame) const;
  EncodedFrame Decrypt(const EncryptedFrame& encrypted_frame) const;

  // Returns random bytes from a cryptographically-secure RNG source.
  static SixteenBytes GenerateRandomBytes();

 private:
  // Provide opaque storage space for the AES_KEY struct (from boringssl). This
  // is used in-lieu of including the relevant boringssl header file in this
  // file and using the AES_KEY type here, which would result in a rather
  // complex BUILD.gn situation.
  alignas(alignof(std::max_align_t)) const uint8_t aes_key_storage_[256];

  // Random bytes used in the custom heuristic to generate a different
  // initialization vector for each frame.
  const SixteenBytes cast_iv_mask_;

  // AES-CTR is symmetric. Thus, the "meat" of both Encrypt() and Decrypt() is
  // the same.
  void EncryptCommon(FrameId frame_id,
                     const std::vector<uint8_t>& in,
                     uint8_t* out) const;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_FRAME_CRYPTO_H_
