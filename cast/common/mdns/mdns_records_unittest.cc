// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_records.h"

#include "cast/common/mdns/mdns_reader.h"
#include "cast/common/mdns/mdns_writer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/network_interface.h"

namespace cast {
namespace mdns {

namespace {

template <class T>
void TestCopyAndMove(const T& value) {
  T value_copy_constuct(value);
  EXPECT_EQ(value_copy_constuct, value);
  T value_copy_assign = value;
  EXPECT_EQ(value_copy_assign, value);
  T value_move_constuct(std::move(value_copy_constuct));
  EXPECT_EQ(value_move_constuct, value);
  T value_move_assign = std::move(value_copy_assign);
  EXPECT_EQ(value_move_assign, value);
}

}  // namespace

TEST(MdnsDomainNameTest, Construct) {
  DomainName name1;
  EXPECT_TRUE(name1.empty());
  EXPECT_EQ(name1.max_wire_size(), UINT64_C(1));
  EXPECT_EQ(name1.label_count(), UINT64_C(0));

  DomainName name2{"MyDevice", "_mYSERvice", "local"};
  EXPECT_FALSE(name2.empty());
  EXPECT_EQ(name2.max_wire_size(), UINT64_C(27));
  ASSERT_EQ(name2.label_count(), UINT64_C(3));
  EXPECT_EQ(name2.Label(0), "MyDevice");
  EXPECT_EQ(name2.Label(1), "_mYSERvice");
  EXPECT_EQ(name2.Label(2), "local");
  EXPECT_EQ(name2.ToString(), "MyDevice._mYSERvice.local");

  std::vector<absl::string_view> labels{"OtherDevice", "_MYservice", "LOcal"};
  DomainName name3(labels);
  EXPECT_FALSE(name3.empty());
  EXPECT_EQ(name3.max_wire_size(), UINT64_C(30));
  ASSERT_EQ(name3.label_count(), UINT64_C(3));
  EXPECT_EQ(name3.Label(0), "OtherDevice");
  EXPECT_EQ(name3.Label(1), "_MYservice");
  EXPECT_EQ(name3.Label(2), "LOcal");
  EXPECT_EQ(name3.ToString(), "OtherDevice._MYservice.LOcal");
}

TEST(MdnsDomainNameTest, Compare) {
  DomainName first{"testing", "local"};
  DomainName second{"TeStInG", "LOCAL"};
  DomainName third{"testing"};
  DomainName fourth{"testing.local"};
  DomainName fifth{"Testing.Local"};

  EXPECT_EQ(first, second);
  EXPECT_EQ(fourth, fifth);
  EXPECT_NE(first, third);
  EXPECT_NE(first, fourth);
}

TEST(MdnsDomainNameTest, CopyAndMove) {
  TestCopyAndMove(DomainName{"testing", "local"});
}

// TODO: Raw RDATA tests

TEST(MdnsSrvRecordRdataTest, Construct) {
  SrvRecordRdata rdata1;
  EXPECT_EQ(rdata1.max_wire_size(), UINT64_C(9));
  EXPECT_EQ(rdata1.priority(), UINT16_C(0));
  EXPECT_EQ(rdata1.weight(), UINT16_C(0));
  EXPECT_EQ(rdata1.port(), UINT16_C(0));
  EXPECT_EQ(rdata1.target(), DomainName());

  SrvRecordRdata rdata2(1, 2, 3, DomainName{"testing", "local"});
  EXPECT_EQ(rdata2.max_wire_size(), UINT64_C(23));
  EXPECT_EQ(rdata2.priority(), UINT16_C(1));
  EXPECT_EQ(rdata2.weight(), UINT16_C(2));
  EXPECT_EQ(rdata2.port(), UINT16_C(3));
  EXPECT_EQ(rdata2.target(), (DomainName{"testing", "local"}));
}

TEST(MdnsSrvRecordRdataTest, Compare) {
  SrvRecordRdata rdata1(1, 2, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata2(1, 2, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata3(4, 2, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata4(1, 5, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata5(1, 2, 6, DomainName{"testing", "local"});
  SrvRecordRdata rdata6(1, 2, 3, DomainName{"device", "local"});

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
  EXPECT_NE(rdata1, rdata4);
  EXPECT_NE(rdata1, rdata5);
  EXPECT_NE(rdata1, rdata6);
}

TEST(MdnsSrvRecordRdataTest, CopyAndMove) {
  TestCopyAndMove(SrvRecordRdata(1, 2, 3, DomainName{"testing", "local"}));
}

TEST(MdnsARecordRdataTest, Construct) {
  ARecordRdata rdata1;
  EXPECT_EQ(rdata1.max_wire_size(), UINT64_C(6));
  EXPECT_EQ(rdata1.ipv4_address(), (IPAddress{0, 0, 0, 0}));

  ARecordRdata rdata2(IPAddress{8, 8, 8, 8});
  EXPECT_EQ(rdata2.max_wire_size(), UINT64_C(6));
  EXPECT_EQ(rdata2.ipv4_address(), (IPAddress{8, 8, 8, 8}));
}

TEST(MdnsARecordRdataTest, Compare) {
  ARecordRdata rdata1(IPAddress{8, 8, 8, 8});
  ARecordRdata rdata2(IPAddress{8, 8, 8, 8});
  ARecordRdata rdata3(IPAddress{1, 2, 3, 4});

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsARecordRdataTest, CopyAndMove) {
  TestCopyAndMove(ARecordRdata(IPAddress{8, 8, 8, 8}));
}

TEST(MdnsAAAARecordRdataTest, Construct) {
  constexpr uint8_t kIPv6AddressBytes1[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  constexpr uint8_t kIPv6AddressBytes2[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };

  IPAddress address1(kIPv6AddressBytes1);
  AAAARecordRdata rdata1;
  EXPECT_EQ(rdata1.max_wire_size(), UINT64_C(18));
  EXPECT_EQ(rdata1.ipv6_address(), address1);

  IPAddress address2(kIPv6AddressBytes2);
  AAAARecordRdata rdata2(address2);
  EXPECT_EQ(rdata2.max_wire_size(), UINT64_C(18));
  EXPECT_EQ(rdata2.ipv6_address(), address2);
}

TEST(MdnsAAAARecordRdataTest, Compare) {
  constexpr uint8_t kIPv6AddressBytes1[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  };
  constexpr uint8_t kIPv6AddressBytes2[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };

  IPAddress address1(kIPv6AddressBytes1);
  IPAddress address2(kIPv6AddressBytes2);
  AAAARecordRdata rdata1(address1);
  AAAARecordRdata rdata2(address1);
  AAAARecordRdata rdata3(address2);

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsAAAARecordRdataTest, CopyAndMove) {
  constexpr uint8_t kIPv6AddressBytes[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  TestCopyAndMove(AAAARecordRdata(IPAddress(kIPv6AddressBytes)));
}

TEST(MdnsPtrRecordRdataTest, Construct) {
  PtrRecordRdata rdata1;
  EXPECT_EQ(rdata1.max_wire_size(), UINT64_C(3));
  EXPECT_EQ(rdata1.ptr_domain(), DomainName());

  PtrRecordRdata rdata2(DomainName{"testing", "local"});
  EXPECT_EQ(rdata2.max_wire_size(), UINT64_C(17));
  EXPECT_EQ(rdata2.ptr_domain(), (DomainName{"testing", "local"}));
}

TEST(MdnsPtrRecordRdataTest, Compare) {
  PtrRecordRdata rdata1(DomainName{"testing", "local"});
  PtrRecordRdata rdata2(DomainName{"testing", "local"});
  PtrRecordRdata rdata3(DomainName{"device", "local"});

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsPtrRecordRdataTest, CopyAndMove) {
  TestCopyAndMove(PtrRecordRdata(DomainName{"testing", "local"}));
}

TEST(MdnsTxtRecordRdataTest, Construct) {
  TxtRecordRdata rdata1;
  EXPECT_EQ(rdata1.max_wire_size(), UINT64_C(3));
  EXPECT_EQ(rdata1.texts(), std::vector<std::string>());

  TxtRecordRdata rdata2{"foo=1", "bar=2"};
  EXPECT_EQ(rdata2.max_wire_size(), UINT64_C(14));
  EXPECT_EQ(rdata2.texts(), (std::vector<std::string>{"foo=1", "bar=2"}));

  std::vector<absl::string_view> texts{"E=mc^2", "F=ma"};
  TxtRecordRdata rdata3(texts);
  EXPECT_EQ(rdata3.max_wire_size(), UINT64_C(14));
  EXPECT_EQ(rdata3.texts(), (std::vector<std::string>{"E=mc^2", "F=ma"}));
}

TEST(MdnsTxtRecordRdataTest, Compare) {
  TxtRecordRdata rdata1{"foo=1", "bar=2"};
  TxtRecordRdata rdata2{"foo=1", "bar=2"};
  TxtRecordRdata rdata3{"foo=1"};
  TxtRecordRdata rdata4{"E=mc^2", "F=ma"};

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
  EXPECT_NE(rdata1, rdata4);
}

TEST(MdnsTxtRecordRdataTest, CopyAndMove) {
  TestCopyAndMove(TxtRecordRdata{"foo=1", "bar=2"});
}

TEST(MdnsRecordTest, Construct) {
  MdnsRecord record1;
  EXPECT_EQ(record1.max_wire_size(), 11);
  EXPECT_EQ(record1.name(), DomainName());
  EXPECT_EQ(record1.type(), UINT16_C(0));
  EXPECT_EQ(record1.record_class(), UINT16_C(0));
  EXPECT_EQ(record1.ttl(), UINT32_C(255));  // 255 is kDefaultRecordTTL
  EXPECT_EQ(record1.rdata(), Rdata(RawRecordRdata()));

  MdnsRecord record2(DomainName{"hostname", "local"}, kTypePTR,
                     kClassIN | kCacheFlushBit, 120,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  EXPECT_EQ(record2.max_wire_size(), UINT64_C(41));
  EXPECT_EQ(record2.name(), (DomainName{"hostname", "local"}));
  EXPECT_EQ(record2.type(), kTypePTR);
  EXPECT_EQ(record2.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record2.ttl(), 120);
  EXPECT_EQ(record2.rdata(),
            Rdata(PtrRecordRdata(DomainName{"testing", "local"})));
}

TEST(MdnsRecordTest, Compare) {
  MdnsRecord record1(DomainName{"hostname", "local"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record2(DomainName{"hostname", "local"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record3(DomainName{"othername", "local"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record4(DomainName{"hostname", "local"}, kTypeA, kClassIN, 120,
                     ARecordRdata(IPAddress{8, 8, 8, 8}));
  MdnsRecord record5(DomainName{"hostname", "local"}, kTypePTR,
                     kClassIN | kCacheFlushBit, 120,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record6(DomainName{"hostname", "local"}, kTypePTR, kClassIN, 200,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record7(DomainName{"hostname", "local"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"device", "local"}));

  EXPECT_EQ(record1, record2);
  EXPECT_NE(record1, record3);
  EXPECT_NE(record1, record4);
  EXPECT_NE(record1, record5);
  EXPECT_NE(record1, record6);
  EXPECT_NE(record1, record7);
}

TEST(MdnsRecordTest, CopyAndMove) {
  MdnsRecord record(DomainName{"hostname", "local"}, kTypePTR,
                    kClassIN | kCacheFlushBit, 120,
                    PtrRecordRdata(DomainName{"testing", "local"}));
  TestCopyAndMove(record);
}

TEST(MdnsQuestionTest, Construct) {
  MdnsQuestion question1;
  EXPECT_EQ(question1.max_wire_size(), 5);
  EXPECT_EQ(question1.name(), DomainName());
  EXPECT_EQ(question1.type(), UINT16_C(0));
  EXPECT_EQ(question1.record_class(), UINT16_C(0));

  MdnsQuestion question2(DomainName{"testing", "local"}, kTypePTR,
                         kClassIN | kUnicastResponseBit);
  EXPECT_EQ(question2.max_wire_size(), UINT64_C(19));
  EXPECT_EQ(question2.name(), (DomainName{"testing", "local"}));
  EXPECT_EQ(question2.type(), kTypePTR);
  EXPECT_EQ(question2.record_class(), kClassIN | kCacheFlushBit);
}

TEST(MdnsQuestionTest, Compare) {
  MdnsQuestion question1(DomainName{"testing", "local"}, kTypePTR, kClassIN);
  MdnsQuestion question2(DomainName{"testing", "local"}, kTypePTR, kClassIN);
  MdnsQuestion question3(DomainName{"hostname", "local"}, kTypePTR, kClassIN);
  MdnsQuestion question4(DomainName{"testing", "local"}, kTypeA, kClassIN);
  MdnsQuestion question5(DomainName{"hostname", "local"}, kTypePTR,
                         kClassIN | kUnicastResponseBit);

  EXPECT_EQ(question1, question2);
  EXPECT_NE(question1, question3);
  EXPECT_NE(question1, question4);
  EXPECT_NE(question1, question5);
}

TEST(MdnsQuestionTest, CopyAndMove) {
  MdnsQuestion question(DomainName{"testing", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  TestCopyAndMove(question);
}

TEST(MdnsMessageTest, Construct) {
  MdnsMessage message1;
  EXPECT_EQ(message1.max_wire_size(), UINT64_C(12));
  EXPECT_EQ(message1.id(), UINT16_C(0));
  EXPECT_EQ(message1.flags(), UINT16_C(0));
  EXPECT_EQ(message1.questions().size(), UINT64_C(0));
  EXPECT_EQ(message1.answers().size(), UINT64_C(0));
  EXPECT_EQ(message1.authority_records().size(), UINT64_C(0));
  EXPECT_EQ(message1.additional_records().size(), UINT64_C(0));

  MdnsQuestion question(DomainName{"testing", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  MdnsRecord record1(DomainName{"record1"}, kTypeA, kClassIN, 120,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsRecord record2(DomainName{"record2"}, kTypeTXT, kClassIN, 120,
                     TxtRecordRdata{"foo=1", "bar=2"});
  MdnsRecord record3(DomainName{"record3"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"device", "local"}));

  MdnsMessage message2(123, 0x8400);
  EXPECT_EQ(message2.max_wire_size(), UINT64_C(12));
  EXPECT_EQ(message2.id(), UINT16_C(123));
  EXPECT_EQ(message2.flags(), UINT16_C(0x8400));
  EXPECT_EQ(message2.questions().size(), UINT64_C(0));
  EXPECT_EQ(message2.answers().size(), UINT64_C(0));
  EXPECT_EQ(message2.authority_records().size(), UINT64_C(0));
  EXPECT_EQ(message2.additional_records().size(), UINT64_C(0));

  message2.AddQuestion(question);
  message2.AddAnswer(record1);
  message2.AddAuthorityRecord(record2);
  message2.AddAdditionalRecord(record3);

  EXPECT_EQ(message2.max_wire_size(), UINT64_C(118));
  ASSERT_EQ(message2.questions().size(), UINT64_C(1));
  ASSERT_EQ(message2.answers().size(), UINT64_C(1));
  ASSERT_EQ(message2.authority_records().size(), UINT64_C(1));
  ASSERT_EQ(message2.additional_records().size(), UINT64_C(1));

  EXPECT_EQ(message2.questions()[0], question);
  EXPECT_EQ(message2.answers()[0], record1);
  EXPECT_EQ(message2.authority_records()[0], record2);
  EXPECT_EQ(message2.additional_records()[0], record3);

  MdnsMessage message3(123, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});

  EXPECT_EQ(message3.max_wire_size(), UINT64_C(118));
  ASSERT_EQ(message3.questions().size(), UINT64_C(1));
  ASSERT_EQ(message3.answers().size(), UINT64_C(1));
  ASSERT_EQ(message3.authority_records().size(), UINT64_C(1));
  ASSERT_EQ(message3.additional_records().size(), UINT64_C(1));

  EXPECT_EQ(message3.questions()[0], question);
  EXPECT_EQ(message3.answers()[0], record1);
  EXPECT_EQ(message3.authority_records()[0], record2);
  EXPECT_EQ(message3.additional_records()[0], record3);
}

TEST(MdnsMessageTest, Compare) {
  MdnsQuestion question(DomainName{"testing", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  MdnsRecord record1(DomainName{"record1"}, kTypeA, kClassIN, 120,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsRecord record2(DomainName{"record2"}, kTypeTXT, kClassIN, 120,
                     TxtRecordRdata{"foo=1", "bar=2"});
  MdnsRecord record3(DomainName{"record3"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"device", "local"}));

  MdnsMessage message1(123, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message2(123, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message3(456, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message4(123, 0x400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message5(123, 0x8400, std::vector<MdnsQuestion>{},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message6(123, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message7(123, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message8(123, 0x8400, std::vector<MdnsQuestion>{question},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{});

  EXPECT_EQ(message1, message2);
  EXPECT_NE(message1, message3);
  EXPECT_NE(message1, message4);
  EXPECT_NE(message1, message5);
  EXPECT_NE(message1, message6);
  EXPECT_NE(message1, message7);
  EXPECT_NE(message1, message8);
}

TEST(MdnsMessageTest, CopyAndMove) {
  MdnsQuestion question(DomainName{"testing", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  MdnsRecord record1(DomainName{"record1"}, kTypeA, kClassIN, 120,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsRecord record2(DomainName{"record2"}, kTypeTXT, kClassIN, 120,
                     TxtRecordRdata{"foo=1", "bar=2"});
  MdnsRecord record3(DomainName{"record3"}, kTypePTR, kClassIN, 120,
                     PtrRecordRdata(DomainName{"device", "local"}));
  MdnsMessage message(123, 0x8400, std::vector<MdnsQuestion>{question},
                      std::vector<MdnsRecord>{record1},
                      std::vector<MdnsRecord>{record2},
                      std::vector<MdnsRecord>{record3});
  TestCopyAndMove(message);
}

TEST(MdnsRecordTest, ReadARecord) {
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
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_TRUE(reader.Read(&record));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(record.name().ToString(), "testing.local");
  EXPECT_EQ(record.type(), kTypeA);
  EXPECT_EQ(record.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  ARecordRdata a_rdata(IPAddress{8, 8, 8, 8});
  EXPECT_EQ(record.rdata(), Rdata(a_rdata));
}

TEST(MdnsRecordTest, ReadUnknownRecordType) {
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
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_TRUE(reader.Read(&record));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(record.name().ToString(), "testing.local");
  EXPECT_EQ(record.type(), kTypeCNAME);
  EXPECT_EQ(record.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  RawRecordRdata raw_rdata(kCnameRdata, sizeof(kCnameRdata));
  EXPECT_EQ(record.rdata(), Rdata(raw_rdata));
}

TEST(MdnsRecordTest, ReadCompressedNames) {
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

  EXPECT_EQ(record.name().ToString(), "testing.local");
  EXPECT_EQ(record.type(), kTypePTR);
  EXPECT_EQ(record.record_class(), kClassIN);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  PtrRecordRdata ptr_rdata(DomainName{"ptr", "testing", "local"});
  EXPECT_EQ(record.rdata(), Rdata(ptr_rdata));

  EXPECT_TRUE(reader.Read(&record));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(record.name().ToString(), "one.two.testing.local");
  EXPECT_EQ(record.type(), kTypeA);
  EXPECT_EQ(record.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  ARecordRdata a_rdata(IPAddress{8, 8, 8, 8});
  EXPECT_EQ(record.rdata(), Rdata(a_rdata));
}

TEST(MdnsRecordTest, FailToReadMissingRdata) {
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

TEST(MdnsRecordTest, FailToReadInvalidHostName) {
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

TEST(MdnsRecordTest, WriteARecord) {
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
  MdnsRecord record(DomainName{"testing", "local"}, kTypeA,
                    kClassIN | kCacheFlushBit, 120,
                    ARecordRdata(IPAddress{172, 0, 0, 1}));

  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsRecordTest, WritePtrRecord) {
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
  MdnsRecord record(DomainName{"_service", "testing", "local"}, kTypePTR,
                    kClassIN, 120,
                    PtrRecordRdata(DomainName{"testing", "local"}));

  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsQuestionTest, Read) {
  // clang-format off
  const uint8_t kTestQuestion[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
  };
  // clang-format on
  MdnsReader reader(kTestQuestion, sizeof(kTestQuestion));
  MdnsQuestion question;
  EXPECT_TRUE(reader.Read(&question));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(question.name().ToString(), "testing.local");
  EXPECT_EQ(question.type(), kTypeA);
  EXPECT_EQ(question.record_class(), kClassIN | kUnicastResponseBit);
}

TEST(MdnsQuestionTest, ReadCompressedNames) {
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

  EXPECT_EQ(question.name().ToString(), "first.local");
  EXPECT_EQ(question.type(), kTypeA);
  EXPECT_EQ(question.record_class(), kClassIN | kUnicastResponseBit);

  EXPECT_TRUE(reader.Read(&question));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(question.name().ToString(), "second.local");
  EXPECT_EQ(question.type(), kTypePTR);
  EXPECT_EQ(question.record_class(), kClassIN);
}

TEST(MdnsQuestionTest, FailToReadInvalidHostName) {
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

TEST(MdnsQuestionTest, Write) {
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
  MdnsQuestion question(DomainName{"wire", "format", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(question));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsMessageTest, Read) {
  // clang-format off
  const uint8_t kTestMessage[] = {
      // Header
      0x00, 0x01,  // ID = 1
      0x84, 0x00,  // FLAGS = AA | RESPONSE
      0x00, 0x00,  // Questions = 0
      0x00, 0x01,  // Answers = 1
      0x00, 0x00,  // Authority = 0
      0x00, 0x01,  // Additional = 1
      // Record 1
      0x07, 'r', 'e', 'c', 'o', 'r', 'd', '1',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x0f,              // RDLENGTH = 15 bytes
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      // Record 2
      0x07, 'r', 'e', 'c', 'o', 'r', 'd', '2',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0xac, 0x00, 0x00, 0x01,  // 172.0.0.1
  };
  // clang-format on
  MdnsReader reader(kTestMessage, sizeof(kTestMessage));
  MdnsMessage message;
  EXPECT_TRUE(reader.Read(&message));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(message.id(), UINT16_C(1));
  EXPECT_EQ(message.flags(), UINT16_C(0x8400));

  EXPECT_EQ(message.questions().size(), UINT64_C(0));

  ASSERT_EQ(message.answers().size(), UINT64_C(1));
  const MdnsRecord& answer = message.answers().at(0);
  EXPECT_EQ(answer.name().ToString(), "record1");
  EXPECT_EQ(answer.type(), kTypePTR);
  EXPECT_EQ(answer.record_class(), kClassIN);
  EXPECT_EQ(answer.ttl(), UINT32_C(120));
  PtrRecordRdata ptr_rdata(DomainName{"testing", "local"});
  EXPECT_EQ(answer.rdata(), Rdata(ptr_rdata));

  EXPECT_EQ(message.authority_records().size(), UINT64_C(0));

  ASSERT_EQ(message.additional_records().size(), UINT64_C(1));
  const MdnsRecord& additional = message.additional_records().at(0);
  EXPECT_EQ(additional.name().ToString(), "record2");
  EXPECT_EQ(additional.type(), kTypeA);
  EXPECT_EQ(additional.record_class(), kClassIN);
  EXPECT_EQ(additional.ttl(), UINT32_C(120));
  ARecordRdata a_rdata(IPAddress{172, 0, 0, 1});
  EXPECT_EQ(additional.rdata(), Rdata(a_rdata));
}

TEST(MdnsMessageTest, FailToReadInvalidRecordCounts) {
  // clang-format off
  const uint8_t kInvalidMessage1[] = {
      0x00, 0x00,  // ID = 0
      0x00, 0x00,  // FLAGS = 0
      0x00, 0x01,  // Questions = 1
      0x00, 0x01,  // Answers = 1
      0x00, 0x00,  // Authority = 0
      0x00, 0x00,  // Additional = 0
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,  // TYPE = PTR (12)
      0x00, 0x01,  // CLASS = IN (1)
      // NOTE: Missing answer record
  };
  const uint8_t kInvalidMessage2[] = {
      0x00, 0x00,  // ID = 0
      0x00, 0x00,  // FLAGS = 0
      0x00, 0x00,  // Questions = 0
      0x00, 0x00,  // Answers = 0
      0x00, 0x00,  // Authority = 0
      0x00, 0x02,  // Additional = 2
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x00,              // RDLENGTH = 0
      // NOTE: Only 1 answer record is given.
  };
  // clang-format on
  MdnsMessage message;
  MdnsReader reader1(kInvalidMessage1, sizeof(kInvalidMessage1));
  EXPECT_FALSE(reader1.Read(&message));
  MdnsReader reader2(kInvalidMessage2, sizeof(kInvalidMessage2));
  EXPECT_FALSE(reader2.Read(&message));
}

TEST(MdnsMessageTest, Write) {
  // clang-format off
  const uint8_t kExpectedMessage[] = {
      0x00, 0x01,  // ID = 1
      0x04, 0x00,  // FLAGS = AA
      0x00, 0x01,  // Question count
      0x00, 0x00,  // Answer count
      0x00, 0x01,  // Authority count
      0x00, 0x00,  // Additional count
      // Question
      0x08, 'q', 'u', 'e', 's', 't', 'i', 'o', 'n',
      0x00,
      0x00, 0x0c,  // TYPE = PTR (12)
      0x00, 0x01,  // CLASS = IN (1)
      // Authority Record
      0x04, 'a', 'u', 't', 'h',
      0x00,
      0x00, 0x10,              // TYPE = TXT (16)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x0c,              // RDLENGTH = 12 bytes
      0x05, 'f', 'o', 'o', '=', '1',
      0x05, 'b', 'a', 'r', '=', '2',
  };
  // clang-format on
  MdnsQuestion question(DomainName{"question"}, kTypePTR, kClassIN);

  MdnsRecord auth_record(DomainName{"auth"}, kTypeTXT, kClassIN, 120,
                         TxtRecordRdata{"foo=1", "bar=2"});

  MdnsMessage message(1, 0x0400);
  message.AddQuestion(question);
  message.AddAuthorityRecord(auth_record);

  std::vector<uint8_t> buffer(sizeof(kExpectedMessage));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(message));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedMessage));
}

}  // namespace mdns
}  // namespace cast
