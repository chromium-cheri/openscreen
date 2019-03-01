// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/endpoint_request_ids.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

TEST(EndpointRequestIdsTest, StrictlyIncreasingRequestIdSequence) {
  EndpointRequestIds request_ids;

  EXPECT_EQ(1u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids.GetNextRequestId(3));
  EXPECT_EQ(4u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(3));
}

TEST(EndpointRequestIdsTest, ResetRequestId) {
  EndpointRequestIds request_ids;

  EXPECT_EQ(1u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(7));
  request_ids.ResetRequestId(7);
  EXPECT_EQ(1u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids.GetNextRequestId(3));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(3));
  request_ids.ResetRequestId(7);
  EXPECT_EQ(1u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(7));
  request_ids.ResetRequestId(9);
}

TEST(EndpointRequestIdsTest, ResetAll) {
  EndpointRequestIds request_ids;

  EXPECT_EQ(1u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids.GetNextRequestId(3));
  EXPECT_EQ(2u, request_ids.GetNextRequestId(3));
  request_ids.Reset();
  EXPECT_EQ(1u, request_ids.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids.GetNextRequestId(3));
}

}  // namespace openscreen
