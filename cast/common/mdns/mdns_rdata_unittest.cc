// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_rdata.h"

#include "cast/common/mdns/mdns_reader.h"
#include "cast/common/mdns/mdns_writer.h"
#include "platform/api/network_interface.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsDomainNameTest, BasicDomainNames) {
  DomainName name;
  EXPECT_TRUE(name.PushLabel("MyDevice"));
  EXPECT_TRUE(name.PushLabel("_mYSERvice"));
  EXPECT_TRUE(name.PushLabel("local"));
  ASSERT_EQ(3U, name.label_count());
  EXPECT_EQ("MyDevice", name.Label(0));
  EXPECT_EQ("_mYSERvice", name.Label(1));
  EXPECT_EQ("local", name.Label(2));
  EXPECT_EQ("MyDevice._mYSERvice.local", name.ToString());

  DomainName other_name;
  EXPECT_TRUE(other_name.PushLabel("OtherDevice"));
  EXPECT_TRUE(other_name.PushLabel("_MYservice"));
  EXPECT_TRUE(other_name.PushLabel("LOcal"));
  ASSERT_EQ(3U, other_name.label_count());
  EXPECT_EQ("OtherDevice", other_name.Label(0));
  EXPECT_EQ("_MYservice", other_name.Label(1));
  EXPECT_EQ("LOcal", other_name.Label(2));
  EXPECT_EQ("OtherDevice._MYservice.LOcal", other_name.ToString());
}

TEST(MdnsDomainNameTest, CopyAndAssignAndClear) {
  DomainName name;
  name.PushLabel("testing");
  name.PushLabel("local");
  EXPECT_EQ(15u, name.max_wire_size());

  DomainName name_copy(name);
  EXPECT_EQ(name_copy, name);
  EXPECT_EQ(15u, name_copy.max_wire_size());

  DomainName name_assign = name;
  EXPECT_EQ(name_assign, name);
  EXPECT_EQ(15u, name_assign.max_wire_size());

  name.Clear();
  EXPECT_EQ(1u, name.max_wire_size());
  EXPECT_NE(name_copy, name);
  EXPECT_NE(name_assign, name);
  EXPECT_EQ(name_copy, name_assign);
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

  EXPECT_EQ(first, second);
  EXPECT_EQ(fourth, fifth);

  EXPECT_NE(first, third);
  EXPECT_NE(first, fourth);
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

}  // namespace mdns
}  // namespace cast
