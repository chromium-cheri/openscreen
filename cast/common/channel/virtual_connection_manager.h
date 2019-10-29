// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_
#define CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>

#include "absl/types/optional.h"
#include "cast/common/channel/virtual_connection.h"

namespace cast {
namespace channel {

// Maintains a collection of open VirtualConnections and associated data.
class VirtualConnectionManager {
 public:
  VirtualConnectionManager();
  ~VirtualConnectionManager();

  void AddConnection(VirtualConnection virtual_connection,
                     VirtualConnection::AssociatedData&& associated_data);

  // Returns true if a connection matching |virtual_connection| was found and
  // removed.
  bool RemoveConnection(const VirtualConnection& virtual_connection,
                        VirtualConnection::CloseReason reason);

  // Returns the number of connections removed.
  uint32_t RemoveConnectionsByLocalId(const std::string& local_id,
                                      VirtualConnection::CloseReason reason);
  uint32_t RemoveConnectionsBySocketId(uint32_t socket_id,
                                       VirtualConnection::CloseReason reason);

  absl::optional<
      std::reference_wrapper<const VirtualConnection::AssociatedData>>
  HasConnection(const VirtualConnection& virtual_connection) const;

 private:
  struct VCTail {
    std::string peer_id;
    VirtualConnection::AssociatedData data;
  };

  std::map<uint32_t /* socket_id */,
           std::multimap<std::string /* local_id */, VCTail>>
      connections_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_VIRTUAL_CONNECTION_MANAGER_H_
