// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_
#define CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_

#include <cstdint>
#include <map>
#include <string>

#include "cast/common/channel/virtual_connection.h"

namespace cast {
namespace channel {

// Maintains a collection of open VirtualConnections and provides an observer
// interface for Add/Remove events.
class VirtualConnectionManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnConnectionAdded(
        const VirtualConnection& vconn,
        const VirtualConnection::AssociatedData& associated_data) = 0;
    virtual void OnConnectionRemoved(
        const VirtualConnection& vconn,
        const VirtualConnection::AssociatedData& associated_data,
        VirtualConnection::CloseReason reason) = 0;
  };

  VirtualConnectionManager();
  ~VirtualConnectionManager();

  void set_observer(Observer* observer) { observer_ = observer; }

  void AddConnection(VirtualConnection vconn,
                     VirtualConnection::AssociatedData&& associated_data);
  bool RemoveConnection(const VirtualConnection& vconn,
                        VirtualConnection::CloseReason reason);
  uint32_t RemoveConnectionByLocalId(const std::string& local_id,
                                     VirtualConnection::CloseReason reason);
  uint32_t RemoveConnectionBySocketId(uint32_t socket_id,
                                      VirtualConnection::CloseReason reason);

  bool HasConnection(const VirtualConnection& vconn) const;

 private:
  struct VCTail {
    std::string peer_id;
    VirtualConnection::AssociatedData data;
  };

  Observer* observer_ = nullptr;
  std::map<uint32_t /* socket_id */,
           std::multimap<std::string /* local_id */, VCTail>>
      connections_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_
