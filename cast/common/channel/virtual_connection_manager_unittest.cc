// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_manager.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;

class MockObserver : public VirtualConnectionManager::Observer {
 public:
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnConnectionAdded,
              (const VirtualConnection& vconn,
               const VirtualConnection::AssociatedData& associated_data),
              (override));
  MOCK_METHOD(void,
              OnConnectionRemoved,
              (const VirtualConnection& vconn,
               const VirtualConnection::AssociatedData& associated_data,
               VirtualConnection::CloseReason reason),
              (override));
};

class VirtualConnectionManagerTest : public ::testing::Test {
 public:
  void SetUp() override { manager_.set_observer(&observer_); }

 protected:
  ::testing::NiceMock<MockObserver> observer_;
  VirtualConnectionManager manager_;

  VirtualConnection vc1_{"local1", "peer1", 75};
  VirtualConnection vc2_{"local2", "peer2", 76};
  VirtualConnection vc3_{"local1", "peer3", 75};
};

}  // namespace

TEST_F(VirtualConnectionManagerTest, NoConnections) {
  EXPECT_FALSE(manager_.HasConnection(vc1_));
  EXPECT_FALSE(manager_.HasConnection(vc2_));
  EXPECT_FALSE(manager_.HasConnection(vc3_));
}

TEST_F(VirtualConnectionManagerTest, AddConnections) {
  VirtualConnection::AssociatedData data1 = {};

  manager_.AddConnection(vc1_, std::move(data1));
  EXPECT_TRUE(manager_.HasConnection(vc1_));
  EXPECT_FALSE(manager_.HasConnection(vc2_));
  EXPECT_FALSE(manager_.HasConnection(vc3_));

  VirtualConnection::AssociatedData data2 = {};
  manager_.AddConnection(vc2_, std::move(data2));
  EXPECT_TRUE(manager_.HasConnection(vc1_));
  EXPECT_TRUE(manager_.HasConnection(vc2_));
  EXPECT_FALSE(manager_.HasConnection(vc3_));

  VirtualConnection::AssociatedData data3 = {};
  manager_.AddConnection(vc3_, std::move(data3));
  EXPECT_TRUE(manager_.HasConnection(vc1_));
  EXPECT_TRUE(manager_.HasConnection(vc2_));
  EXPECT_TRUE(manager_.HasConnection(vc3_));
}

TEST_F(VirtualConnectionManagerTest, RemoveConnections) {
  VirtualConnection::AssociatedData data1 = {};
  VirtualConnection::AssociatedData data2 = {};
  VirtualConnection::AssociatedData data3 = {};

  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_TRUE(manager_.RemoveConnection(
      vc1_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.HasConnection(vc1_));
  EXPECT_TRUE(manager_.HasConnection(vc2_));
  EXPECT_TRUE(manager_.HasConnection(vc3_));

  EXPECT_TRUE(manager_.RemoveConnection(
      vc2_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.HasConnection(vc1_));
  EXPECT_FALSE(manager_.HasConnection(vc2_));
  EXPECT_TRUE(manager_.HasConnection(vc3_));

  EXPECT_TRUE(manager_.RemoveConnection(
      vc3_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.HasConnection(vc1_));
  EXPECT_FALSE(manager_.HasConnection(vc2_));
  EXPECT_FALSE(manager_.HasConnection(vc3_));

  EXPECT_FALSE(manager_.RemoveConnection(
      vc1_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.RemoveConnection(
      vc2_, VirtualConnection::CloseReason::kClosedBySelf));
  EXPECT_FALSE(manager_.RemoveConnection(
      vc3_, VirtualConnection::CloseReason::kClosedBySelf));
}

TEST_F(VirtualConnectionManagerTest, RemoveConnectionsByIds) {
  VirtualConnection::AssociatedData data1 = {};
  VirtualConnection::AssociatedData data2 = {};
  VirtualConnection::AssociatedData data3 = {};

  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_EQ(manager_.RemoveConnectionByLocalId(
                "local1", VirtualConnection::CloseReason::kClosedBySelf),
            2u);
  EXPECT_FALSE(manager_.HasConnection(vc1_));
  EXPECT_TRUE(manager_.HasConnection(vc2_));
  EXPECT_FALSE(manager_.HasConnection(vc3_));

  data1 = {};
  data2 = {};
  data3 = {};
  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));
  EXPECT_EQ(manager_.RemoveConnectionBySocketId(
                76, VirtualConnection::CloseReason::kClosedBySelf),
            1u);
  EXPECT_TRUE(manager_.HasConnection(vc1_));
  EXPECT_FALSE(manager_.HasConnection(vc2_));
  EXPECT_TRUE(manager_.HasConnection(vc3_));

  EXPECT_EQ(manager_.RemoveConnectionBySocketId(
                75, VirtualConnection::CloseReason::kClosedBySelf),
            2u);
  EXPECT_FALSE(manager_.HasConnection(vc1_));
  EXPECT_FALSE(manager_.HasConnection(vc2_));
  EXPECT_FALSE(manager_.HasConnection(vc3_));
}

TEST_F(VirtualConnectionManagerTest, Observer) {
  VirtualConnection::AssociatedData data1 = {};
  VirtualConnection::AssociatedData data2 = {};
  VirtualConnection::AssociatedData data3 = {};

  EXPECT_CALL(observer_, OnConnectionAdded(_, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data) {
            EXPECT_EQ(vc1_, vconn);
          }));
  manager_.AddConnection(vc1_, std::move(data1));
  EXPECT_CALL(observer_, OnConnectionAdded(_, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data) {
            EXPECT_EQ(vc2_, vconn);
          }));
  manager_.AddConnection(vc2_, std::move(data2));
  EXPECT_CALL(observer_, OnConnectionAdded(_, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data) {
            EXPECT_EQ(vc3_, vconn);
          }));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_CALL(observer_, OnConnectionAdded(_, _)).Times(0);
  data1 = {};
  data2 = {};
  data3 = {};
  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc2_, std::move(data2));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_CALL(observer_, OnConnectionRemoved(_, _, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data,
                 VirtualConnection::CloseReason reason) {
            EXPECT_EQ(vc1_, vconn);
            EXPECT_EQ(VirtualConnection::CloseReason::kClosedBySelf, reason);
            EXPECT_FALSE(manager_.HasConnection(vc1_));
          }))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data,
                 VirtualConnection::CloseReason reason) {
            EXPECT_EQ(vc3_, vconn);
            EXPECT_EQ(VirtualConnection::CloseReason::kClosedBySelf, reason);
            EXPECT_FALSE(manager_.HasConnection(vc3_));
          }));
  EXPECT_EQ(manager_.RemoveConnectionByLocalId(
                "local1", VirtualConnection::CloseReason::kClosedBySelf),
            2u);

  EXPECT_CALL(observer_, OnConnectionAdded(_, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data) {
            EXPECT_EQ(vc1_, vconn);
          }))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data) {
            EXPECT_EQ(vc3_, vconn);
          }));
  data1 = {};
  data3 = {};
  manager_.AddConnection(vc1_, std::move(data1));
  manager_.AddConnection(vc3_, std::move(data3));

  EXPECT_CALL(observer_, OnConnectionRemoved(_, _, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data,
                 VirtualConnection::CloseReason reason) {
            EXPECT_EQ(vc2_, vconn);
            EXPECT_EQ(VirtualConnection::CloseReason::kClosedByPeer, reason);
            EXPECT_FALSE(manager_.HasConnection(vc2_));
          }));
  EXPECT_EQ(manager_.RemoveConnectionBySocketId(
                76, VirtualConnection::CloseReason::kClosedByPeer),
            1u);

  EXPECT_CALL(observer_, OnConnectionRemoved(_, _, _))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data,
                 VirtualConnection::CloseReason reason) {
            EXPECT_EQ(vc1_, vconn);
            EXPECT_EQ(VirtualConnection::CloseReason::kClosedBySelf, reason);
            EXPECT_FALSE(manager_.HasConnection(vc1_));
          }))
      .WillOnce(Invoke(
          [this](const VirtualConnection& vconn,
                 const VirtualConnection::AssociatedData& associated_data,
                 VirtualConnection::CloseReason reason) {
            EXPECT_EQ(vc3_, vconn);
            EXPECT_EQ(VirtualConnection::CloseReason::kClosedBySelf, reason);
            EXPECT_FALSE(manager_.HasConnection(vc3_));
          }));
  EXPECT_EQ(manager_.RemoveConnectionBySocketId(
                75, VirtualConnection::CloseReason::kClosedBySelf),
            2u);
}

}  // namespace channel
}  // namespace cast
