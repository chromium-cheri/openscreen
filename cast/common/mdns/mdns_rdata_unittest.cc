// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_rdata.h"

#include "platform/api/network_interface.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsRdataTest, SrvRecordRdata) {
  const uint8_t kExpectedRdata[] = {
      0x00, 0x05,  // PRIORITY = 5
      0x00, 0x06,  // WEIGHT = 6
      0x1f, 0x49,  // PORT = 8009
      0x07, 't',  'e', 's', 't', 'i', 'n',  'g',
      0x05, 'l',  'o', 'c', 'a', 'l', 0x00,
  };
  SrvRecordRdata rdata;
  rdata.set_priority(5);
  rdata.set_weight(6);
  rdata.set_port(8009);
  DomainName name;
  name.PushLabel("testing");
  name.PushLabel("local");
  rdata.set_target(name);
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.MaxWireSize());

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  SrvRecordRdata rdata_read;
  EXPECT_TRUE(rdata_read.ReadFrom(&reader, sizeof(kExpectedRdata)));
  EXPECT_TRUE(rdata_read.IsEqual(&rdata));
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  uint16_t rdlength;
  EXPECT_TRUE(rdata.WriteTo(&writer, &rdlength));
  EXPECT_EQ(sizeof(kExpectedRdata), rdlength);
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, ARecordRdata) {
  const uint8_t kExpectedRdata[] = {
      0x08,
      0x08,
      0x08,
      0x08,
  };
  auto parse_result = IPAddress::Parse("8.8.8.8");
  ASSERT_TRUE(parse_result);
  IPAddress address(parse_result.MoveValue());
  ARecordRdata rdata;
  rdata.set_address(address);
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.MaxWireSize());

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  ARecordRdata rdata_read;
  EXPECT_TRUE(rdata_read.ReadFrom(&reader, sizeof(kExpectedRdata)));
  EXPECT_TRUE(rdata_read.address().IsV4());
  EXPECT_TRUE(rdata_read.IsEqual(&rdata));
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  uint16_t rdlength;
  EXPECT_TRUE(rdata.WriteTo(&writer, &rdlength));
  EXPECT_EQ(sizeof(kExpectedRdata), rdlength);
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, AAAARecordRdata) {
  const uint8_t kExpectedRdata[] = {
      // FE80:0000:0000:0000:0202:B3FF:FE1E:8329
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  auto parse_result = IPAddress::Parse("FE80:0000:0000:0000:0202:B3FF:FE1E:8329");
  ASSERT_TRUE(parse_result);
  IPAddress address(parse_result.MoveValue());
  AAAARecordRdata rdata;
  rdata.set_address(address);
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.MaxWireSize());

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  AAAARecordRdata rdata_read;
  EXPECT_TRUE(rdata_read.ReadFrom(&reader, sizeof(kExpectedRdata)));
  EXPECT_TRUE(rdata_read.address().IsV6());
  EXPECT_TRUE(rdata_read.IsEqual(&rdata));
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  uint16_t rdlength;
  EXPECT_TRUE(rdata.WriteTo(&writer, &rdlength));
  EXPECT_EQ(sizeof(kExpectedRdata), rdlength);
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, PtrRecordRdata) {
  const uint8_t kExpectedRdata[] = {
      0x08, 'm', 'y', 'd', 'e', 'v',  'i', 'c', 'e', 0x07, 't', 'e',
      's',  't', 'i', 'n', 'g', 0x05, 'l', 'o', 'c', 'a',  'l', 0x00,
  };
  DomainName name;
  name.PushLabel("mydevice");
  name.PushLabel("testing");
  name.PushLabel("local");
  PtrRecordRdata rdata;
  rdata.set_ptr_domain(name);
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.MaxWireSize());

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  PtrRecordRdata rdata_read;
  EXPECT_TRUE(rdata_read.ReadFrom(&reader, sizeof(kExpectedRdata)));
  EXPECT_TRUE(rdata_read.IsEqual(&rdata));
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  uint16_t rdlength;
  EXPECT_TRUE(rdata.WriteTo(&writer, &rdlength));
  EXPECT_EQ(sizeof(kExpectedRdata), rdlength);
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, TxtRecordRdata) {
  const uint8_t kExpectedRdata[] = {
      0x05, 'f', 'o', 'o', '=', '1', 0x05, 'b', 'a', 'r', '=', '2', 0x0e, 'n',
      'a',  'm', 'e', '=', 'w', 'e', '.',  '.', 'i', 'r', 'd', '/', '/',
  };
  std::vector<std::string> texts;
  texts.push_back("foo=1");
  texts.push_back("bar=2");
  texts.push_back("name=we..ird//");
  TxtRecordRdata rdata;
  rdata.set_texts(texts);
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.MaxWireSize());

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  TxtRecordRdata rdata_read;
  EXPECT_TRUE(rdata_read.ReadFrom(&reader, sizeof(kExpectedRdata)));
  EXPECT_TRUE(rdata_read.IsEqual(&rdata));
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  uint16_t rdlength;
  EXPECT_TRUE(rdata.WriteTo(&writer, &rdlength));
  EXPECT_EQ(sizeof(kExpectedRdata), rdlength);
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, TxtRecordRdata_Empty) {
  const uint8_t kExpectedRdata[] = {0x00};
  std::vector<std::string> texts;
  TxtRecordRdata rdata;
  rdata.set_texts(texts);
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.MaxWireSize());

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  TxtRecordRdata rdata_read;
  EXPECT_TRUE(rdata_read.ReadFrom(&reader, sizeof(kExpectedRdata)));
  EXPECT_TRUE(rdata_read.IsEqual(&rdata));
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  uint16_t rdlength;
  EXPECT_TRUE(rdata.WriteTo(&writer, &rdlength));
  EXPECT_EQ(sizeof(kExpectedRdata), rdlength);
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, IsEqual) {
  auto parse_result = IPAddress::Parse("8.8.8.8");
  ASSERT_TRUE(parse_result);
  IPAddress address(parse_result.MoveValue());
  ARecordRdata a_rdata;
  a_rdata.set_address(address);
  ARecordRdata a_rdata_2;
  a_rdata_2.set_address(address);
  EXPECT_TRUE(a_rdata.IsEqual(&a_rdata_2));

  std::vector<std::string> texts;
  texts.push_back("foo=1");
  texts.push_back("bar=2");
  texts.push_back("name=we..ird//");
  TxtRecordRdata txt_rdata;
  txt_rdata.set_texts(texts);
  EXPECT_FALSE(a_rdata.IsEqual(&txt_rdata));
  EXPECT_FALSE(txt_rdata.IsEqual(&a_rdata_2));

  RawRecordRdata raw_rdata(mdns::kTypeCNAME);
  raw_rdata.set_rdata("testing.local");
  EXPECT_FALSE(raw_rdata.IsEqual(&txt_rdata));
  EXPECT_FALSE(a_rdata.IsEqual(&raw_rdata));
}

}  // namespace mdns
}  // namespace cast
