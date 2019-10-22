// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/constants.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

TEST(DnsSdConstantsTest, TestSrvKeyEquals) {
  SrvKey key1;
  key1.service_id = "service";
  key1.domain_id = "domain";
  key1.instance_id = "instance";
  SrvKey key2;
  key2.service_id = "service";
  key2.domain_id = "domain";
  key2.instance_id = "instance";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.service_id = "service2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.service_id = "service2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.domain_id = "domain2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.domain_id = "domain2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.instance_id = "instance2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.instance_id = "instance2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);
}

TEST(DnsSdConstantsTest, TestPtrKeyEquals) {
  PtrKey key1;
  key1.service_id = "service";
  key1.domain_id = "domain";
  PtrKey key2;
  key2.service_id = "service";
  key2.domain_id = "domain";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.service_id = "service2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.service_id = "service2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1.domain_id = "domain2";
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2.domain_id = "domain2";
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);
}

TEST(DnsSdConstantsTest, TestIsInstanceOf) {
  PtrKey ptr;
  ptr.service_id = "service";
  ptr.domain_id = "domain";
  SrvKey svc;
  svc.service_id = "service";
  svc.domain_id = "domain";
  svc.instance_id = "instance";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));

  svc.instance_id = "other id";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));

  svc.domain_id = "domain2";
  EXPECT_FALSE(IsInstanceOf(ptr, svc));
  ptr.domain_id = "domain2";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));

  svc.service_id = "service2";
  EXPECT_FALSE(IsInstanceOf(ptr, svc));
  ptr.service_id = "service2";
  EXPECT_TRUE(IsInstanceOf(ptr, svc));
}

}  // namespace discovery
}  // namespace openscreen
