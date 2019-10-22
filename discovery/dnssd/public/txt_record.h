// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_TXT_RECORD_H_
#define DISCOVERY_DNSSD_PUBLIC_TXT_RECORD_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

class DnsSdTxtRecord {
 public:
  // Sets the value currently stored in this DNS-SD TXT record. Returns error
  // if the provided key is already set or if either the key or value is
  // invalid, and Error::None() otherwise. Case of the key is ignored. Setting
  // a value or flag which is already present will overwrite the previous one.
  Error SetValue(const absl::string_view& key,
                 const absl::Span<uint8_t>& value);
  Error SetFlag(const absl::string_view& key, bool value);

  // Reads the value associated with the provided key, or an error if the key
  // is mapped to the opposite type or the query is othewise invalid. Case of
  // the key is ignored.
  // is invalid or not present. Case of the key is ignored.
  ErrorOr<absl::Span<uint8_t>> GetValue(const absl::string_view& key) const;
  ErrorOr<bool> GetFlag(const absl::string_view& key) const;

  // Clears an existing TxtRecord value associated with the given key.
  Error ClearValue(const absl::string_view& key);
  Error ClearFlag(const absl::string_view& key);

  bool IsEmpty() const { return txt_.empty(); }

 private:
  using TxtValue = absl::optional<std::vector<uint8_t>>;

  struct CaseInsensitiveStringHash {
    size_t operator()(const std::string& key) const;
  };

  struct CaseInsensitiveStringEquals {
    bool operator()(const std::string& Left, const std::string& Right) const;
  };

  // Validations for keys and (key, value) pairs.
  bool IsKeyValid(const std::string& key);
  bool IsKeyValuePairValid(const std::string& key,
                           const absl::Span<uint8_t>& value);

  // Map from VALID key to TxtValue, where absl::nullopt represents the presence
  // of a boolean value (meaning it is set to true) and a uint8_t array in other
  // cases.
  mutable std::unordered_map<std::string,
                             TxtValue,
                             CaseInsensitiveStringHash,
                             CaseInsensitiveStringEquals>
      txt_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_TXT_RECORD_H_
