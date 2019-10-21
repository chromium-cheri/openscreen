// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_manager.h"

#include <type_traits>

namespace cast {
namespace channel {

VirtualConnectionManager::VirtualConnectionManager() = default;

VirtualConnectionManager::~VirtualConnectionManager() = default;

void VirtualConnectionManager::AddConnection(
    VirtualConnection vconn,
    VirtualConnection::AssociatedData&& associated_data) {
  auto& socket_map = connections_[vconn.socket_id];
  auto local_entries = socket_map.equal_range(vconn.local_id);
  auto it = std::find_if(local_entries.first, local_entries.second,
                         [&vconn](const std::pair<std::string, VCTail>& entry) {
                           return entry.second.peer_id == vconn.peer_id;
                         });
  if (it == socket_map.end()) {
    if (observer_) {
      VirtualConnection vc = vconn;
      VirtualConnection::AssociatedData data = associated_data;
      socket_map.emplace(
          std::move(vconn.local_id),
          VCTail{std::move(vconn.peer_id), std::move(associated_data)});
      observer_->OnConnectionAdded(vc, data);
    } else {
      socket_map.emplace(
          std::move(vconn.local_id),
          VCTail{std::move(vconn.peer_id), std::move(associated_data)});
    }
  }
}

bool VirtualConnectionManager::RemoveConnection(
    const VirtualConnection& vconn,
    VirtualConnection::CloseReason reason) {
  auto socket_entry = connections_.find(vconn.socket_id);
  if (socket_entry == connections_.end()) {
    return false;
  }

  auto& socket_map = socket_entry->second;
  auto local_entries = socket_map.equal_range(vconn.local_id);
  if (local_entries.first == socket_map.end()) {
    return false;
  }
  for (auto it = local_entries.first; it != local_entries.second; ++it) {
    if (it->second.peer_id == vconn.peer_id) {
      VirtualConnection::AssociatedData data = std::move(it->second.data);
      socket_map.erase(it);
      if (socket_map.empty()) {
        connections_.erase(socket_entry);
      }
      if (observer_) {
        observer_->OnConnectionRemoved(vconn, data, reason);
      }
      return true;
    }
  }
  return false;
}

uint32_t VirtualConnectionManager::RemoveConnectionByLocalId(
    const std::string& local_id,
    VirtualConnection::CloseReason reason) {
  uint32_t removed_count = 0;
  for (auto socket_entry = connections_.begin();
       socket_entry != connections_.end(); ++socket_entry) {
    auto& socket_map = socket_entry->second;
    auto local_entries = socket_map.equal_range(local_id);
    if (local_entries.first != socket_map.end()) {
      uint32_t current_count =
          std::distance(local_entries.first, local_entries.second);
      removed_count += current_count;
      if (observer_) {
        std::vector<std::decay_t<decltype(socket_map)>::value_type> temp;
        temp.reserve(current_count);
        for (auto it = local_entries.first; it != local_entries.second; ++it) {
          temp.emplace_back(std::move(*it));
        }
        socket_map.erase(local_entries.first, local_entries.second);

        for (const auto& it : temp) {
          observer_->OnConnectionRemoved(
              VirtualConnection{local_id, it.second.peer_id,
                                socket_entry->first},
              it.second.data, reason);
        }
      } else {
        socket_map.erase(local_entries.first, local_entries.second);
      }
      if (socket_map.empty()) {
        connections_.erase(socket_entry);
      }
    }
  }
  return removed_count;
}

uint32_t VirtualConnectionManager::RemoveConnectionBySocketId(
    uint32_t socket_id,
    VirtualConnection::CloseReason reason) {
  auto entry = connections_.find(socket_id);
  if (entry == connections_.end()) {
    return 0;
  }

  uint32_t removed_count = entry->second.size();
  if (observer_) {
    std::decay_t<decltype(connections_)>::mapped_type temp =
        std::move(entry->second);
    connections_.erase(entry);

    for (const auto& local_entry : temp) {
      observer_->OnConnectionRemoved(
          VirtualConnection{local_entry.first, local_entry.second.peer_id,
                            socket_id},
          local_entry.second.data, reason);
    }
  } else {
    connections_.erase(entry);
  }

  return removed_count;
}

bool VirtualConnectionManager::HasConnection(
    const VirtualConnection& vconn) const {
  auto socket_entry = connections_.find(vconn.socket_id);
  if (socket_entry == connections_.end()) {
    return false;
  }

  auto& socket_map = socket_entry->second;
  auto local_entries = socket_map.equal_range(vconn.local_id);
  if (local_entries.first == socket_map.end()) {
    return false;
  }
  for (auto it = local_entries.first; it != local_entries.second; ++it) {
    if (it->second.peer_id == vconn.peer_id) {
      return true;
    }
  }
  return false;
}

}  // namespace channel
}  // namespace cast
