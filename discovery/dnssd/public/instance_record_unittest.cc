// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/instance_record.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

TEST(DnsSdInstanceRecordTests, InstanceLength) {
  EXPECT_TRUE(DnsSdInstanceRecord::IsInstanceValid("instance"));
  EXPECT_TRUE(DnsSdInstanceRecord::IsInstanceValid("name"));
  EXPECT_TRUE(DnsSdInstanceRecord::IsInstanceValid(""));
  EXPECT_TRUE(DnsSdInstanceRecord::IsInstanceValid(
      "Something63CharsLongabcdefghijklmnopqrstuvwxyz1234567890ABCDEFG"));

  EXPECT_FALSE(DnsSdInstanceRecord::IsInstanceValid(
      "Something63CharsLongabcdefghijklmnopqrstuvwxyz1234567890ABCDEFGH"));
}

TEST(DnsSdInstanceRecordTests, InstanceCharacters) {
  EXPECT_TRUE(DnsSdInstanceRecord::IsInstanceValid(
      "IncludingSpecialCharacters.+ =*&<<+`~\\/"));
  EXPECT_TRUE(DnsSdInstanceRecord::IsInstanceValid(".+ =*&<<+`~\\/ "));

  EXPECT_FALSE(
      DnsSdInstanceRecord::IsInstanceValid(std::string(1, uint8_t{0x7F})));
  EXPECT_FALSE(DnsSdInstanceRecord::IsInstanceValid(
      std::string("name with ") + std::string(1, uint8_t{0x7F}) +
      " in the middle"));
  for (uint8_t bad_char = 0x0; bad_char <= 0x1F; bad_char++) {
    EXPECT_FALSE(
        DnsSdInstanceRecord::IsInstanceValid(std::string(1, bad_char)));
    EXPECT_FALSE(DnsSdInstanceRecord::IsInstanceValid(
        std::string("name with ") + std::string(1, bad_char) +
        " in the middle"));
  }
}

TEST(DnsSdInstanceRecordTests, ServiceLength) {
  // Too short.
  EXPECT_TRUE(DnsSdInstanceRecord::IsServiceValid("_a._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_._udp"));

  // Too long.
  EXPECT_TRUE(DnsSdInstanceRecord::IsServiceValid("_abcdefghijklmno._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_abcdefghijklmnop._udp"));
}

TEST(DnsSdInstanceRecordTests, ServiceNonProtocolNameFormatting) {
  EXPECT_TRUE(DnsSdInstanceRecord::IsServiceValid("_abcd._udp"));

  // Unexprected protocol string,
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_abcd._ssl"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_abcd._tls"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_abcd._ucp"));

  // Extra characters before
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid(" _abcd._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("a_abcd._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("-_abcd._udp"));

  // Extra characters after.
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("a_abcd._udp "));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("a_abcd._udp-"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("a_abcd._udpp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("a_abcd._tcp_udp"));
}

TEST(DnsSdInstanceRecordTests, ServiceProtocolNameFormatting) {
  EXPECT_TRUE(DnsSdInstanceRecord::IsServiceValid("_abcd._udp"));

  // Disallowed Characters
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_ab`d._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_a\\cd._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_ab.d._udp"));

  // Contains no letters
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_123._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_1-3._udp"));

  // Improperly placed hyphen.
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_-abcd._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_abcd-._udp"));

  // Adjacent hyphens.
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_abc--d._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_a--bcd._tcp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_0a1b--c02d._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_0a--1._udp"));
  EXPECT_FALSE(DnsSdInstanceRecord::IsServiceValid("_a--b._udp"));
}

}  // namespace discovery
}  // namespace openscreen
