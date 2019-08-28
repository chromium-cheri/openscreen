// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_random.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsRandomTest, InitialQueryDelay) {
  MdnsRandom mdns_random;
  Clock::duration delay = mdns_random.GetInitialQueryDelay();
  EXPECT_GE(delay, std::chrono::milliseconds(20));
  EXPECT_LE(delay, std::chrono::milliseconds(120));
}

TEST(MdnsRandomTest, RecordTtlVariation) {
  MdnsRandom mdns_random;
  double variation = mdns_random.GetRecordTtlVariation();
  EXPECT_GE(variation, 0.0);
  EXPECT_LE(variation, 2.0);
}

TEST(MdnsRandomTest, SharedRecordResponseDelay) {
  MdnsRandom mdns_random;
  Clock::duration delay = mdns_random.GetSharedRecordResponseDelay();
  EXPECT_GE(delay, std::chrono::milliseconds(20));
  EXPECT_LE(delay, std::chrono::milliseconds(120));
}

TEST(MdnsRandomTest, TruncatedQueryResponseDelay) {
  MdnsRandom mdns_random;
  Clock::duration delay = mdns_random.GetTruncatedQueryResponseDelay();
  EXPECT_GE(delay, std::chrono::milliseconds(400));
  EXPECT_LE(delay, std::chrono::milliseconds(500));
}

}  // namespace mdns
}  // namespace cast
