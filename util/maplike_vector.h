// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_MAPLIKE_VECTOR_H_
#define UTIL_MAPLIKE_VECTOR_H_

#include <initializer_list>
#include <map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "util/osp_logging.h"

namespace openscreen {

// For small numbers of elements, a vector is much more efficient than a
// map or unordered_map due to not needing hashing. MaplikeVector allows for
// using map-like syntax but is backed by a std::vector, combining all the
// performance of a vector with the convenience of a map.
template <class Key, class Value>
class MaplikeVector final : public std::vector<std::pair<Key, Value>> {
 public:
  MaplikeVector() = default;
  MaplikeVector(std::initializer_list<std::pair<Key, Value>> init_list)
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

  // Whether or not the vector contains value.
  bool ContainsValue(const Value& value) const {
    for (const auto& pair : *this) {
      if (pair.second == value) {
        return true;
      }
    }
    return false;
  }

  // Accessor that assumes you already checked that the value in is the vector
  // by calling ContainsKey. Will assert if key is not in the vector.
  Value& operator[](const Key& key) {
    for (auto& pair : *this) {
      if (pair.first == key) {
        return pair.second;
      }
    }
    OSP_NOTREACHED() << "Key not in maplike vector";
    return default_;
  }

  const Value& operator[](const Key& key) const {
    for (const auto& pair : *this) {
      if (pair.first == key) {
        return pair.second;
      }
    }
    OSP_NOTREACHED() << "Key not in maplike vector";
    return default_;
  }

  // Accessor that allows access without asserting if it is missing. This
  // does not allow for changing in place.
  absl::optional<Value> Get(const Key& key) const {
    for (const auto& pair : *this) {
      if (pair.first == key) {
        return pair.second;
      }
    }
    return absl::nullopt;
  }

  // Remove an entry from the map. Returns true if an entry was actually
  // removed.
  bool Remove(const Key& key) {
    auto it = this->begin();
    for (; it != this->end(); ++it) {
      if (it->first == key) {
        break;
      }
    }
    if (it != this->end()) {
      this->erase(it);
      return true;
    }
    return false;
  }

 private:
  Value default_{};
};

}  // namespace openscreen

#endif  // UTIL_MAPLIKE_VECTOR_H_
