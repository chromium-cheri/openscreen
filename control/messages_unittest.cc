// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "control/messages.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

// TODO(btolsch): This is in the current (draft) spec, but should we actually
// allow this?
TEST(MessagesTest, EncodeRequestZeroUrls) {
  uint8_t buffer[256];
  std::vector<std::string> urls;
  int32_t bytes_out =
      EncodePresentationUrlAvailabilityRequest(3, urls, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<int32_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);

  PresentationUrlAvailabilityRequest decoded_request;
  int32_t bytes_read = DecodePresentationUrlAvailabilityRequest(
      buffer, bytes_out, &decoded_request);
  ASSERT_EQ(bytes_read, bytes_out);
  EXPECT_EQ(3u, decoded_request.request_id);
  EXPECT_EQ(urls, decoded_request.urls);
}

TEST(MessagesTest, EncodeRequestOneUrl) {
  uint8_t buffer[256];
  std::vector<std::string> urls{"https://example.com/receiver.html"};
  int32_t bytes_out =
      EncodePresentationUrlAvailabilityRequest(7, urls, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<int32_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);

  PresentationUrlAvailabilityRequest decoded_request;
  int32_t bytes_read = DecodePresentationUrlAvailabilityRequest(
      buffer, bytes_out, &decoded_request);
  ASSERT_EQ(bytes_read, bytes_out);
  EXPECT_EQ(7u, decoded_request.request_id);
  EXPECT_EQ(urls, decoded_request.urls);
}

TEST(MessagesTest, EncodeRequestMultipleUrls) {
  uint8_t buffer[256];
  std::vector<std::string> urls{"https://example.com/receiver.html",
                                "https://openscreen.org/demo_receiver.html",
                                "https://turt.le/asdfXCV"};
  int32_t bytes_out =
      EncodePresentationUrlAvailabilityRequest(7, urls, buffer, sizeof(buffer));
  ASSERT_LE(bytes_out, static_cast<int32_t>(sizeof(buffer)));
  ASSERT_GT(bytes_out, 0);

  PresentationUrlAvailabilityRequest decoded_request;
  int32_t bytes_read = DecodePresentationUrlAvailabilityRequest(
      buffer, bytes_out, &decoded_request);
  ASSERT_EQ(bytes_read, bytes_out);
  EXPECT_EQ(7u, decoded_request.request_id);
  EXPECT_EQ(urls, decoded_request.urls);
}

}  // namespace openscreen
