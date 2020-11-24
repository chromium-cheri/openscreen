// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_FLAT_MAP_H_
#define UTIL_FLAT_MAP_H_

#include <initializer_list>
#include <map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "util/osp_logging.h"

namespace openscreen {

// For small numbers of elements, a vector is much more efficient than a
// map or unordered_map due to not needing hashing. FlatMap allows for
// using map-like syntax but is backed by a std::vector, combining all the
// performance of a vector with the convenience of a map.
template <class Key, class Value>
class FlatMap final : public std::vector<std::pair<Key, Value>> {
 public:
  FlatMap() = default;
  ~FlatMap() = default;
  FlatMap(std::initializer_list<std::pair<Key, Value>> init_list)
      : std::vector<std::pair<Key, Value>>(init_list) {}

  // Whether or not the vector contains key.
  bool Contains(const Key& key) const {
    for (const auto& pair : *this) {
      if (pair.first == key) {
        return true;
      }
    }
    return false;
  }

  // Accessors that wrap std::find_if, and return an iterator to the key value
  // pair.
  decltype(auto) Find(const Key& key) {
    return std::find_if(
        this->begin(), this->end(),
        [key](const std::pair<Key, Value>& pair) { return key == pair.first; });
  }

  decltype(auto) Find(const Key& key) const {
    return const_cast<FlatMap<Key, Value>*>(this)->Find(key);
  }

  // Remove an entry from the map. Returns true if an entry was actually
  // removed.
  bool Erase(const Key& key) {
    auto it = Find(key);
    if (it != this->end()) {
      this->erase(it);
      return true;
    }
    return false;
  }
};

}  // namespace openscreen

#endif  // UTIL_FLAT_MAP_H_
