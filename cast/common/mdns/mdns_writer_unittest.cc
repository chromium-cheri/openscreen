// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_writer.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsWriterTest, WriteDomainName) {
  // clang-format off
  constexpr uint8_t kExpectedResult[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00
  };
  // clang-format on
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.Write(DomainName{"testing", "local"}));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_CompressedMessage) {
  // clang-format off
  constexpr uint8_t kExpectedResultCompressed[] = {
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
  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.Write(DomainName{"testing", "local"}));
  ASSERT_TRUE(writer.Write(DomainName{"prefix", "local"}));
  ASSERT_TRUE(writer.Write(DomainName{"new", "prefix", "local"}));
  ASSERT_TRUE(writer.Write(DomainName{"prefix", "local"}));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_NotEnoughSpace) {
  // clang-format off
  constexpr uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x09, 'd', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't',
    0x06, 'd', 'o', 'm', 'a', 'i', 'n',
    0x00
  };
  // clang-format on
  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.Write(DomainName{"testing", "local"}));
  // Not enough space to write this domain name. Failure to write it must not
  // affect correct successful write of the next domain name.
  ASSERT_FALSE(writer.Write(DomainName{"a", "different", "domain"}));
  ASSERT_TRUE(writer.Write(DomainName{"different", "domain"}));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_Long) {
  constexpr char kLongLabel[] =
      "12345678901234567890123456789012345678901234567890";
  // clang-format off
  constexpr uint8_t kExpectedResult[] = {
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
  DomainName name{kLongLabel, kLongLabel, kLongLabel, kLongLabel};
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.Write(name));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_Empty) {
  DomainName name;
  uint8_t result[256];
  MdnsWriter writer(result, sizeof(result));
  EXPECT_FALSE(writer.Write(name));
  // The writer should not have moved its internal pointer when it fails to
  // write. It should fail without any side effects.
  EXPECT_EQ(0u, writer.offset());
}

TEST(MdnsWriterTest, WriteDomainName_NoCompressionForBigOffsets) {
  // clang-format off
  constexpr uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
  };
  // clang-format on

  DomainName name{"testing", "local"};
  // Maximum supported value for label pointer offset is 0x3FFF.
  // Labels written into a buffer at greater offsets must not
  // produce compression label pointers.
  std::vector<uint8_t> buffer(0x4000 + sizeof(kExpectedResultCompressed));
  {
    MdnsWriter writer(buffer.data(), buffer.size());
    writer.Skip(0x4000);
    ASSERT_TRUE(writer.Write(name));
    ASSERT_TRUE(writer.Write(name));
    EXPECT_EQ(0UL, writer.remaining());
  }
  buffer.erase(buffer.begin(), buffer.begin() + 0x4000);
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteSrvRecordRdata) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x15,  // RDLENGTH = 21
      0x00, 0x05,  // PRIORITY = 5
      0x00, 0x06,  // WEIGHT = 6
      0x1f, 0x49,  // PORT = 8009
      0x07, 't',  'e', 's', 't', 'i', 'n',  'g',
      0x05, 'l',  'o', 'c', 'a', 'l', 0x00,
  };
  std::vector<uint8_t> buffer(sizeof(kExpectedRdata));
  MdnsWriter writer(buffer.data(), buffer.size());
  SrvRecordRdata rdata(5, 6, 8009, DomainName{"testing", "local"});
  EXPECT_TRUE(writer.Write(rdata));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedRdata));
}

TEST(MdnsWriterTest, WriteARecordRdata) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x4,               // RDLENGTH = 4
      0x08, 0x08, 0x08, 0x08,  // ADDRESS = 8.8.8.8
  };
  std::vector<uint8_t> buffer(sizeof(kExpectedRdata));
  MdnsWriter writer(buffer.data(), buffer.size());
  ARecordRdata rdata(IPAddress{8, 8, 8, 8});
  EXPECT_TRUE(writer.Write(rdata));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedRdata));
}

TEST(MdnsWriterTest, WriteAAAARecordRdata) {
  // clang-format off
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x10,  // RDLENGTH = 16
      // ADDRESS = FE80:0000:0000:0000:0202:B3FF:FE1E:8329
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  // clang-format on
  std::vector<uint8_t> buffer(sizeof(kExpectedRdata));
  MdnsWriter writer(buffer.data(), buffer.size());
  AAAARecordRdata rdata(IPAddress{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e,
                                  0x83, 0x29});
  EXPECT_TRUE(writer.Write(rdata));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedRdata));
}

TEST(MdnsWriterTest, WritePtrRecordRdata) {
  // clang-format off
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x18,  // RDLENGTH = 24
      0x08, 'm', 'y', 'd', 'e', 'v', 'i', 'c', 'e',
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a',  'l',
      0x00,
  };
  // clang-format on
  std::vector<uint8_t> buffer(sizeof(kExpectedRdata));
  MdnsWriter writer(buffer.data(), buffer.size());
  PtrRecordRdata rdata(DomainName{"mydevice", "testing", "local"});
  EXPECT_TRUE(writer.Write(rdata));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedRdata));
}

TEST(MdnsWriterTest, WriteTxtRecordRdata) {
  // clang-format off
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x0C,  // RDLENGTH = 12
      0x05, 'f', 'o', 'o', '=', '1',
      0x05, 'b', 'a', 'r', '=', '2',
  };
  // clang-format on
  std::vector<uint8_t> buffer(sizeof(kExpectedRdata));
  MdnsWriter writer(buffer.data(), buffer.size());
  TxtRecordRdata rdata{"foo=1", "bar=2"};
  EXPECT_TRUE(writer.Write(rdata));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedRdata));
}

TEST(MdnsWriterTest, WriteEmptyTxtRecordRdata) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x01,  // RDLENGTH = 1
      0x00,        // empty string
  };
  std::vector<uint8_t> buffer(sizeof(kExpectedRdata));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(TxtRecordRdata()));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedRdata));
}

TEST(MdnsWriterTest, WriteMdnsRecord_ARecordRdata) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0xac, 0x00, 0x00, 0x01,  // 172.0.0.1
  };
  // clang-format on
  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  MdnsRecord record(DomainName{"testing", "local"}, kTypeA,
                    kClassIN | kCacheFlushBit, 120,
                    ARecordRdata(IPAddress{172, 0, 0, 1}));
  EXPECT_TRUE(writer.Write(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsWriterTest, WriteMdnsRecord_PtrRecordRdata) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x08, '_', 's', 'e', 'r', 'v', 'i', 'c', 'e',
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x02,              // RDLENGTH = 2 bytes
      0xc0, 0x09,              // Domain name label pointer to byte
  };
  // clang-format on
  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  MdnsRecord record(DomainName{"_service", "testing", "local"}, kTypePTR,
                    kClassIN, 120,
                    PtrRecordRdata(DomainName{"testing", "local"}));
  EXPECT_TRUE(writer.Write(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsWriterTest, WriteMdnsQuestion) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x04, 'w', 'i', 'r', 'e',
      0x06, 'f', 'o', 'r', 'm', 'a', 't',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,  // TYPE = PTR (12)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
  };
  // clang-format on
  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  MdnsQuestion question(DomainName{"wire", "format", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  EXPECT_TRUE(writer.Write(question));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

}  // namespace mdns
}  // namespace cast
