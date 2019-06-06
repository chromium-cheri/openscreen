// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

#include <memory>

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsReaderTest, ReadDomainName) {
  const uint8_t kMessage[] = {
      // First name
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',  // Byte: 0
      0x05, 'l', 'o', 'c', 'a', 'l',            // Byte: 8
      0x00,                                     // Byte: 14
      // Second name
      0x07, 's', 'e', 'r', 'v', 'i', 'c', 'e',  // Byte: 15
      0xc0, 0x00,                               // Byte: 23
      // Third name
      0x06, 'd', 'e', 'v', 'i', 'c', 'e',  // Byte: 25
      0xc0, 0x0f,                          // Byte: 32
      // Fourth name
      0xc0, 0x20,  // Byte: 34
  };
  MdnsReader reader(kMessage, sizeof(kMessage));
  EXPECT_EQ(reader.begin(), kMessage);
  EXPECT_EQ(reader.length(), sizeof(kMessage));
  EXPECT_EQ(reader.offset(), 0u);
  DomainName name;
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "service.testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "device.service.testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "service.testing.local");
  EXPECT_EQ(reader.offset(), sizeof(kMessage));
  EXPECT_EQ(0UL, reader.remaining());
  EXPECT_FALSE(reader.ReadDomainName(&name));
}

TEST(MdnsReaderTest, ReadDomainName_Empty) {
  const uint8_t kDomainName[] = {0x00};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(0UL, reader.remaining());
}

// There should be no side effects for failing to read a domain name.
// The underlying pointer should not have changed.

TEST(MdnsReaderTest, ReadDomainName_TooShort) {
  const uint8_t kDomainName[] = {0x03, 'a', 'b'};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_TooLong) {
  std::vector<uint8_t> kDomainName;
  for (uint8_t letter = 'a'; letter <= 'z'; ++letter) {
    const uint8_t repetitions = 10;
    kDomainName.push_back(repetitions);
    kDomainName.insert(kDomainName.end(), repetitions, letter);
  }
  kDomainName.push_back(0);

  MdnsReader reader(kDomainName.data(), kDomainName.size());
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_LabelPointerOutOfBounds) {
  const uint8_t kDomainName[] = {0xc0, 0x02};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_InvalidLabel) {
  const uint8_t kDomainName[] = {0x80};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_CircularCompression) {
  // clang-format off
  const uint8_t kDomainName[] = {
      // NOTE: Circular label pointer at end of name.
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',  // Byte: 0
      0x05, 'l', 'o', 'c', 'a', 'l',            // Byte: 8
      0xc0, 0x00,                               // Byte: 14
  };
  // clang-format on
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

}  // namespace mdns
}  // namespace cast
