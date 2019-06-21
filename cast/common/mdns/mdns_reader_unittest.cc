// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace mdns {

namespace {

template <class T>
void TestReadEntry(const uint8_t* data, size_t size, T expected) {
  MdnsReader reader(data, size);
  T entry;
  EXPECT_TRUE(reader.Read(&entry));
  EXPECT_EQ(entry, expected);
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
  ;
}

}  // namespace

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
  TestReadEntry(kDomainName, sizeof(kDomainName), DomainName());
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
  TestReadEntry(kSrvRecordRdata, sizeof(kSrvRecordRdata),
                SrvRecordRdata(5, 6, 8009, DomainName{"testing", "local"}));
}

TEST(MdnsReaderTest, ReadARecordRdata) {
  constexpr uint8_t kARecordRdata[] = {
      0x00, 0x4,               // RDLENGTH = 4
      0x08, 0x08, 0x08, 0x08,  // ADDRESS = 8.8.8.8
  };
  TestReadEntry(kARecordRdata, sizeof(kARecordRdata),
                ARecordRdata(IPAddress{8, 8, 8, 8}));
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
  TestReadEntry(kAAAARecordRdata, sizeof(kAAAARecordRdata),
                AAAARecordRdata(IPAddress{0xfe, 0x80, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x02, 0x02, 0xb3, 0xff,
                                          0xfe, 0x1e, 0x83, 0x29}));
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
  TestReadEntry(kPtrRecordRdata, sizeof(kPtrRecordRdata),
                PtrRecordRdata(DomainName{"mydevice", "testing", "local"}));
}

TEST(MdnsReaderTest, ReadTxtRecordRdata) {
  // clang-format off
  constexpr uint8_t kTxtRecordRdata[] = {
      0x00, 0x0C,  // RDLENGTH = 12
      0x05, 'f', 'o', 'o', '=', '1',
      0x05, 'b', 'a', 'r', '=', '2',
  };
  // clang-format on
  TestReadEntry(kTxtRecordRdata, sizeof(kTxtRecordRdata),
                TxtRecordRdata{"foo=1", "bar=2"});
}

TEST(MdnsReaderTest, ReadEmptyTxtRecordRdata) {
  constexpr uint8_t kTxtRecordRdata[] = {
      0x00, 0x01,  // RDLENGTH = 1
      0x00,        // empty string
  };
  TestReadEntry(kTxtRecordRdata, sizeof(kTxtRecordRdata), TxtRecordRdata());
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

TEST(MdnsReaderTest, ReadMdnsRecord_ARecordRdata) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0x08, 0x08, 0x08, 0x08,  // RDATA = 8.8.8.8
  };
  // clang-format on
  TestReadEntry(kTestRecord, sizeof(kTestRecord),
                MdnsRecord(DomainName{"testing", "local"}, kTypeA,
                           kClassIN | kCacheFlushBit, 120,
                           ARecordRdata(IPAddress{8, 8, 8, 8})));
}

TEST(MdnsReaderTest, ReadMdnsRecord_UnknownRecordType) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x05,              // TYPE = CNAME (5)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x08,              // RDLENGTH = 8 bytes
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };
  const uint8_t kCnameRdata[] = {
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };
  // clang-format on
  TestReadEntry(kTestRecord, sizeof(kTestRecord),
                MdnsRecord(DomainName{"testing", "local"}, kTypeCNAME,
                           kClassIN | kCacheFlushBit, 120,
                           RawRecordRdata(kCnameRdata, sizeof(kCnameRdata))));
}

TEST(MdnsReaderTest, ReadMdnsRecord_CompressedNames) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      // First message
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x06,              // RDLENGTH = 6 bytes
      0x03, 'p',  't',  'r',
      0xc0, 0x00,              // Domain name label pointer to byte 0
      // Second message
      0x03, 'o', 'n', 'e',
      0x03, 't', 'w', 'o',
      0xc0, 0x00,              // Domain name label pointer to byte 0
      0x00, 0x01,              // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0x08, 0x08, 0x08, 0x08,  // RDATA = 8.8.8.8
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));

  MdnsRecord record;
  EXPECT_TRUE(reader.Read(&record));
  EXPECT_EQ(record,
            MdnsRecord(DomainName{"testing", "local"}, kTypePTR, kClassIN, 120,
                       PtrRecordRdata(DomainName{"ptr", "testing", "local"})));
  EXPECT_TRUE(reader.Read(&record));
  EXPECT_EQ(record, MdnsRecord(DomainName{"one", "two", "testing", "local"},
                               kTypeA, kClassIN | kCacheFlushBit, 120,
                               ARecordRdata(IPAddress{8, 8, 8, 8})));
}

TEST(MdnsReaderTest, ReadMdnsRecord_MissingRdata) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
                               // Missing RDATA
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_FALSE(reader.Read(&record));
}

TEST(MdnsReaderTest, ReadMdnsRecord_InvalidHostName) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      // Invalid NAME: length byte too short
      0x03, 'i', 'n', 'v', 'a', 'l', 'i', 'd',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0x08, 0x08, 0x08, 0x08,  // RDATA = 8.8.8.8
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_FALSE(reader.Read(&record));
}

TEST(MdnsReaderTest, ReadMdnsQuestion) {
  // clang-format off
  const uint8_t kTestQuestion[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
  };
  // clang-format on
  TestReadEntry(kTestQuestion, sizeof(kTestQuestion),
                MdnsQuestion(DomainName{"testing", "local"}, kTypeA,
                             kClassIN | kUnicastResponseBit));
}

TEST(MdnsReaderTest, ReadMdnsQuestion_CompressedNames) {
  // clang-format off
  const uint8_t kTestQuestions[] = {
      // First Question
      0x05, 'f', 'i', 'r', 's', 't',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
      // Second Question
      0x06, 's', 'e', 'c', 'o', 'n', 'd',
      0xc0, 0x06,  // Domain name label pointer
      0x00, 0x0c,  // TYPE = PTR (12)
      0x00, 0x01,  // CLASS = IN (1)
  };
  // clang-format on
  MdnsReader reader(kTestQuestions, sizeof(kTestQuestions));
  MdnsQuestion question;
  EXPECT_TRUE(reader.Read(&question));
  EXPECT_EQ(question, MdnsQuestion(DomainName{"first", "local"}, kTypeA,
                                   kClassIN | kUnicastResponseBit));
  EXPECT_TRUE(reader.Read(&question));
  EXPECT_EQ(question,
            MdnsQuestion(DomainName{"second", "local"}, kTypePTR, kClassIN));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));
}

TEST(MdnsReaderTest, ReadMdnsQuestion_InvalidHostName) {
  // clang-format off
  const uint8_t kTestQuestion[] = {
      // Invalid NAME: length byte too short
      0x03, 'i', 'n', 'v', 'a', 'l', 'i', 'd',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x00, 0x01,  // CLASS = IN (1)
  };
  // clang-format on
  MdnsReader reader(kTestQuestion, sizeof(kTestQuestion));
  MdnsQuestion question;
  EXPECT_FALSE(reader.Read(&question));
}

}  // namespace mdns
}  // namespace cast
