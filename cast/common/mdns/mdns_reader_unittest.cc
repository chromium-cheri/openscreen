// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsReaderTest, ReadDomainName) {
  constexpr uint8_t kMessage[] = {
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
  EXPECT_TRUE(reader.Read(&name));
  EXPECT_EQ(name.ToString(), "testing.local");
  EXPECT_TRUE(reader.Read(&name));
  EXPECT_EQ(name.ToString(), "service.testing.local");
  EXPECT_TRUE(reader.Read(&name));
  EXPECT_EQ(name.ToString(), "device.service.testing.local");
  EXPECT_TRUE(reader.Read(&name));
  EXPECT_EQ(name.ToString(), "service.testing.local");
  EXPECT_EQ(reader.offset(), sizeof(kMessage));
  EXPECT_EQ(0UL, reader.remaining());
  EXPECT_FALSE(reader.Read(&name));
}

TEST(MdnsReaderTest, ReadDomainName_Empty) {
  constexpr uint8_t kDomainName[] = {0x00};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_TRUE(reader.Read(&name));
  EXPECT_EQ(0UL, reader.remaining());
}

// In the tests below there should be no side effects for failing to read a
// domain name. The underlying pointer should not have changed.

TEST(MdnsReaderTest, ReadDomainName_TooShort) {
  // Length 0x03 is longer than available data for the domain name
  constexpr uint8_t kDomainName[] = {0x03, 'a', 'b'};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.Read(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_TooLong) {
  std::vector<uint8_t> kDomainName;
  for (uint8_t letter = 'a'; letter <= 'z'; ++letter) {
    constexpr uint8_t repetitions = 10;
    kDomainName.push_back(repetitions);
    kDomainName.insert(kDomainName.end(), repetitions, letter);
  }
  kDomainName.push_back(0);

  MdnsReader reader(kDomainName.data(), kDomainName.size());
  DomainName name;
  EXPECT_FALSE(reader.Read(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_LabelPointerOutOfBounds) {
  constexpr uint8_t kDomainName[] = {0xc0, 0x02};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.Read(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_InvalidLabel) {
  constexpr uint8_t kDomainName[] = {0x80};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.Read(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_CircularCompression) {
  // clang-format off
  constexpr uint8_t kDomainName[] = {
      // NOTE: Circular label pointer at end of name.
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',  // Byte: 0
      0x05, 'l', 'o', 'c', 'a', 'l',            // Byte: 8
      0xc0, 0x00,                               // Byte: 14
  };
  // clang-format on
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.Read(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadSrvRecordRdata) {
  constexpr uint8_t kSrvRecordRdata[] = {
      0x00, 0x15,  // RDLENGTH = 21
      0x00, 0x05,  // PRIORITY = 5
      0x00, 0x06,  // WEIGHT = 6
      0x1f, 0x49,  // PORT = 8009
      0x07, 't',  'e', 's', 't', 'i', 'n',  'g',
      0x05, 'l',  'o', 'c', 'a', 'l', 0x00,
  };
  MdnsReader reader(kSrvRecordRdata, sizeof(kSrvRecordRdata));
  SrvRecordRdata rdata;
  EXPECT_TRUE(reader.Read(&rdata));
  EXPECT_EQ(rdata, SrvRecordRdata(5, 6, 8009, DomainName{"testing", "local"}));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

TEST(MdnsReaderTest, ReadARecordRdata) {
  constexpr uint8_t kARecordRdata[] = {
      0x00, 0x4,               // RDLENGTH = 4
      0x08, 0x08, 0x08, 0x08,  // ADDRESS = 8.8.8.8
  };
  MdnsReader reader(kARecordRdata, sizeof(kARecordRdata));
  ARecordRdata rdata;
  EXPECT_TRUE(reader.Read(&rdata));
  EXPECT_EQ(rdata, ARecordRdata(IPAddress{8, 8, 8, 8}));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

TEST(MdnsReaderTest, ReadAAAARecordRdata) {
  // clang-format off
  constexpr uint8_t kAAAARecordRdata[] = {
      0x00, 0x10,  // RDLENGTH = 16
      // ADDRESS = FE80:0000:0000:0000:0202:B3FF:FE1E:8329
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  // clang-format on
  MdnsReader reader(kAAAARecordRdata, sizeof(kAAAARecordRdata));
  AAAARecordRdata rdata;
  EXPECT_TRUE(reader.Read(&rdata));
  EXPECT_EQ(rdata, AAAARecordRdata(IPAddress{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x02, 0x02, 0xb3, 0xff,
                                             0xfe, 0x1e, 0x83, 0x29}));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

TEST(MdnsReaderTest, ReadPtrRecordRdata) {
  // clang-format off
  constexpr uint8_t kPtrRecordRdata[] = {
      0x00, 0x18,  // RDLENGTH = 24
      0x08, 'm', 'y', 'd', 'e', 'v', 'i', 'c', 'e',
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a',  'l',
      0x00,
  };
  // clang-format on
  MdnsReader reader(kPtrRecordRdata, sizeof(kPtrRecordRdata));
  PtrRecordRdata rdata;
  EXPECT_TRUE(reader.Read(&rdata));
  EXPECT_EQ(rdata, PtrRecordRdata(DomainName{"mydevice", "testing", "local"}));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

TEST(MdnsReaderTest, ReadTxtRecordRdata) {
  // clang-format off
  constexpr uint8_t kTxtRecordRdata[] = {
      0x00, 0x0C,  // RDLENGTH = 12
      0x05, 'f', 'o', 'o', '=', '1',
      0x05, 'b', 'a', 'r', '=', '2',
  };
  // clang-format on
  MdnsReader reader(kTxtRecordRdata, sizeof(kTxtRecordRdata));
  TxtRecordRdata rdata;
  EXPECT_TRUE(reader.Read(&rdata));
  EXPECT_EQ(rdata, (TxtRecordRdata{"foo=1", "bar=2"}));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

TEST(MdnsReaderTest, ReadEmptyTxtRecordRdata) {
  constexpr uint8_t kTxtRecordRdata[] = {
      0x00, 0x01,  // RDLENGTH = 1
      0x00,        // empty string
  };
  MdnsReader reader(kTxtRecordRdata, sizeof(kTxtRecordRdata));
  TxtRecordRdata rdata;
  EXPECT_TRUE(reader.Read(&rdata));
  EXPECT_EQ(rdata, TxtRecordRdata());
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

// TEST(MdnsRdataTest, TxtRecordRdata_Weird) {
//   // clang-format off
//   constexpr uint8_t kExpectedRdata[] = {
//       0x00, 0x1B,  // RDLENGTH = 27
//       0x05, 'f', 'o', 'o', '=', '1',
//       0x05, 'b', 'a', 'r', '=', '2',
//       0x0e, 'n', 'a', 'm', 'e', '=',
//       'w', 'e', '.',  '.', 'i', 'r', 'd', '/', '/',
//   };
//   // clang-format on
//   TxtRecordRdata rdata{"foo=1", "bar=2", "name=we..ird//"};
//   EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size());

//   MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
//   TxtRecordRdata rdata_read;
//   EXPECT_TRUE(reader.Read(&rdata_read));
//   EXPECT_EQ(rdata_read, rdata);
//   EXPECT_EQ(0UL, reader.remaining());

//   uint8_t buffer[sizeof(kExpectedRdata)];
//   MdnsWriter writer(buffer, sizeof(buffer));
//   EXPECT_TRUE(writer.Write(rdata));
//   EXPECT_EQ(0UL, writer.remaining());
//   EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
// }

}  // namespace mdns
}  // namespace cast
