// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/authentication/hkdf_scrypt_psk.h"

#include <numeric>
#include <vector>

//#include "api/impl/quic/quic_client.h"
//#include "api/impl/quic/testing/quic_test_support.h"
//#include "api/impl/testing/fake_clock.h"
//#include "api/public/network_service_manager.h"
//#include "api/public/testing/message_demuxer_test_support.h"
//#include "msgs/osp_messages.h"
//#include "platform/api/logging.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace authentication {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;

TEST(ComputeHkdfScryptPskTest, ComputeProofForPredefinedInput) {
  const size_t scrypt_cost = 32768;  // 2^15
  const std::vector<uint8_t> psk = {'1', '3', '3', '7', '5', '3', 'C',
                                    'R', '3', '7', 'P', '1', 'N'};
  std::vector<uint8_t> salt(32);
  std::iota(salt.begin(), salt.end(), 0);
  std::vector<uint8_t> hkdf_info(64);
  std::iota(hkdf_info.begin(), hkdf_info.end(), 0);

  ErrorOr<std::vector<uint8_t>> result =
      ComputeHkdfScryptPsk(psk, salt, scrypt_cost, hkdf_info);

  const std::vector<uint8_t> expected_proof = {
      192, 248, 9,   135, 133, 161, 194, 84,  92,  189, 185,
      26,  49,  234, 97,  48,  28,  52,  209, 172, 214, 43,
      90,  75,  103, 191, 45,  29,  173, 78,  194, 93};

  ASSERT_TRUE(result.is_value());
  const std::vector<uint8_t>& result_proof = result.value();
  ASSERT_EQ(result_proof.size(), expected_proof.size());

  for (size_t i = 0; i < result_proof.size(); ++i) {
    ASSERT_EQ(result_proof[i], expected_proof[i]);
  }
}

}  // namespace authentication
}  // namespace openscreen
