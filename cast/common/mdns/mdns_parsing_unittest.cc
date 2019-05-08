// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_parsing.h"

#include <memory>

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsDomainNameTest, BasicDomainNames) {
  DomainName name;
  EXPECT_TRUE(name.PushLabel("MyDevice"));
  EXPECT_TRUE(name.PushLabel("_mYSERvice"));
  EXPECT_TRUE(name.PushLabel("local"));
  ASSERT_EQ(3U, name.labels().size());
  EXPECT_EQ("MyDevice", name.labels().at(0));
  EXPECT_EQ("_mYSERvice", name.labels().at(1));
  EXPECT_EQ("local", name.labels().at(2));
  EXPECT_EQ("MyDevice._mYSERvice.local", name.as_string());

  DomainName other_name;
  EXPECT_TRUE(other_name.PushLabel("OtherDevice"));
  EXPECT_TRUE(other_name.PushLabel("_MYservice"));
  EXPECT_TRUE(other_name.PushLabel("LOcal"));
  ASSERT_EQ(3U, other_name.labels().size());
  EXPECT_EQ("OtherDevice", other_name.labels().at(0));
  EXPECT_EQ("_MYservice", other_name.labels().at(1));
  EXPECT_EQ("LOcal", other_name.labels().at(2));
  EXPECT_EQ("OtherDevice._MYservice.LOcal", other_name.as_string());

  // Check the SubName for the labels.
  EXPECT_EQ("OtherDevice._MYservice.LOcal", other_name.SubName(0));
  EXPECT_EQ("_MYservice.LOcal", other_name.SubName(1));
  EXPECT_EQ("LOcal", other_name.SubName(2));
  EXPECT_EQ("", other_name.SubName(3));
  EXPECT_EQ("", other_name.SubName(8));

  // Verify that the hashes match for the various labels and sub-labels.
  EXPECT_NE(name.hashes().at(0), other_name.hashes().at(0));
  EXPECT_EQ(name.hashes().at(1), other_name.hashes().at(1));
  EXPECT_EQ(name.hashes().at(2), other_name.hashes().at(2));
  EXPECT_NE(name.SubHash(0), other_name.SubHash(0));
  EXPECT_EQ(name.SubHash(1), other_name.SubHash(1));
  EXPECT_EQ(name.SubHash(2), other_name.SubHash(2));
  EXPECT_EQ(name.SubHash(3), other_name.SubHash(3));
}

TEST(MdnsDomainNameTest, CopyAndAssignAndClear) {
  auto name = std::make_unique<DomainName>();
  name->PushLabel("testing");
  name->PushLabel("local");
  EXPECT_EQ(15u, name->max_wire_size());

  auto name_copy = std::make_unique<DomainName>(*name);
  EXPECT_TRUE(name_copy->IsEqual(*name));
  EXPECT_EQ(15u, name_copy->max_wire_size());

  // Delete the original name to verify that lingering pointers aren't kept
  // on the copied objects.
  name.reset(nullptr);
  auto name_assign = std::make_unique<DomainName>();
  *name_assign = *name_copy;
  EXPECT_TRUE(name_assign->IsEqual(*name_copy));
  EXPECT_EQ(15u, name_assign->max_wire_size());

  name_copy->Clear();
  EXPECT_EQ(1u, name_copy->max_wire_size());
  EXPECT_FALSE(name_assign->IsEqual(*name_copy));
}

TEST(MdnsDomainNameTest, IsEqual) {
  DomainName first;
  first.PushLabel("testing");
  first.PushLabel("local");
  DomainName second;
  second.PushLabel("TeStInG");
  second.PushLabel("LOCAL");
  DomainName third;
  third.PushLabel("testing");
  DomainName fourth;
  fourth.PushLabel("testing.local");
  DomainName fifth;
  fifth.PushLabel("Testing.Local");

  EXPECT_TRUE(first.IsEqual(second));
  EXPECT_TRUE(fourth.IsEqual(fifth));

  EXPECT_FALSE(first.IsEqual(third));
  EXPECT_FALSE(first.IsEqual(fourth));
}

TEST(MdnsDomainNameTest, PushLabel_InvalidLabels) {
  DomainName name;
  EXPECT_TRUE(name.PushLabel("testing"));
  EXPECT_FALSE(name.PushLabel(""));                    // Empty label
  EXPECT_FALSE(name.PushLabel(std::string(64, 'a')));  // Label too long
}

TEST(MdnsDomainNameTest, PushLabel_NameTooLong) {
  std::string maximum_label(63, 'a');

  DomainName name;
  EXPECT_TRUE(name.PushLabel(maximum_label));         // 64 bytes
  EXPECT_TRUE(name.PushLabel(maximum_label));         // 128 bytes
  EXPECT_TRUE(name.PushLabel(maximum_label));         // 192 bytes
  EXPECT_FALSE(name.PushLabel(maximum_label));        // NAME > 255 bytes
  EXPECT_TRUE(name.PushLabel(std::string(62, 'a')));  // NAME = 255
  EXPECT_EQ(256u, name.max_wire_size());
}

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
  };
  MdnsReader reader(kMessage, sizeof(kMessage));
  EXPECT_EQ(reader.buffer(), kMessage);
  EXPECT_EQ(reader.length(), sizeof(kMessage));
  EXPECT_EQ(reader.offset(), 0u);
  DomainName name;
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.as_string(), "testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.as_string(), "service.testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.as_string(), "device.service.testing.local");
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

TEST(MdnsReaderTest, ReadDomainName_TooShort) {
  const uint8_t kDomainName[] = {0x03, 'a', 'b'};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  // There should be no side effects for failing to read a domain name. The
  // underlying pointer should not have changed.
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_CircularCompression) {
  // clang-format off
  const uint8_t kDomainName[] = {
      // NOTE: Circular label pointer at end of name.
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',  // Byte: 0
      0x05, 'l', 'o', 'c', 'a', 'l',  // Byte: 8
      0xc0, 0x00,  // Byte: 14
  };
  // clang-format on
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  // There should be no side effects for failing to read a domain name. The
  // underlying pointer should not have changed.
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsWriterTest, WriteDomainName_Simple) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00
  };
  // clang-format on
  DomainName name;
  EXPECT_TRUE(name.PushLabel("testing"));
  EXPECT_TRUE(name.PushLabel("local"));
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.WriteDomainName(name, true));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_UncompressedMessage) {
  // clang-format off
  const uint8_t kExpectedResultUncompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x06, 'p', 'r', 'e', 'f', 'i', 'x',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x03, 'n', 'e', 'w',
    0x06, 'p', 'r', 'e', 'f', 'i', 'x',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x06, 'p', 'r', 'e', 'f', 'i', 'x',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
  };
  // clang-format on
  DomainName name1;
  name1.PushLabel("testing");
  name1.PushLabel("local");

  DomainName name2;
  name2.PushLabel("prefix");
  name2.PushLabel("local");

  DomainName name3;
  name3.PushLabel("new");
  name3.PushLabel("prefix");
  name3.PushLabel("local");

  DomainName name4;
  name4.PushLabel("prefix");
  name4.PushLabel("local");

  uint8_t result[sizeof(kExpectedResultUncompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultUncompressed));
  ASSERT_TRUE(writer.WriteDomainName(name1, false /* allow_compression */));
  ASSERT_TRUE(writer.WriteDomainName(name2, false /* allow_compression */));
  ASSERT_TRUE(writer.WriteDomainName(name3, false /* allow_compression */));
  ASSERT_TRUE(writer.WriteDomainName(name4, false /* allow_compression */));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultUncompressed));
}

TEST(MdnsWriterTest, WriteDomainName_CompressedMessage) {
  // clang-format off
  const uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x06, 'p', 'r', 'e', 'f', 'i', 'x',
    0xC0, 0x08,  // byte 8
    0x03, 'n', 'e', 'w',
    0xC0, 0x0F,  // byte 15
    0xC0, 0x0F,  // byte 15
  };
  // clang-format on
  DomainName name1;
  name1.PushLabel("testing");
  name1.PushLabel("local");

  DomainName name2;
  name2.PushLabel("prefix");
  name2.PushLabel("local");

  DomainName name3;
  name3.PushLabel("new");
  name3.PushLabel("prefix");
  name3.PushLabel("local");

  DomainName name4;
  name4.PushLabel("prefix");
  name4.PushLabel("local");

  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.WriteDomainName(name1, true /* allow_compression */));
  ASSERT_TRUE(writer.WriteDomainName(name2, true /* allow_compression */));
  ASSERT_TRUE(writer.WriteDomainName(name3, true /* allow_compression */));
  ASSERT_TRUE(writer.WriteDomainName(name4, true /* allow_compression */));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_Long) {
  const char kLongLabel[] =
      "12345678901234567890123456789012345678901234567890";
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x00,
  };
  // clang-format on
  DomainName name;
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.WriteDomainName(name, true));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_Empty) {
  DomainName name;
  uint8_t result[256];
  MdnsWriter writer(result, sizeof(result));
  EXPECT_FALSE(writer.WriteDomainName(name, true));
  // The writer should not have moved its internal pointer when it fails to
  // write. It should fail without any side effects.
  EXPECT_EQ(0u, writer.offset());
}

}  // namespace mdns
}  // namespace cast
