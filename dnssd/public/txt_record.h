// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DNSSD_PUBLIC_TXT_RECORD_H_
#define DNSSD_PUBLIC_TXT_RECORD_H_

#include "absl/strings/string_view.h"
#include "platform/base/error.h"

namespace dnssd {

class TxtRecord {
 public:
  using Error = openscreen::Error;
  template <typename T>
  using ErrorOr = openscreen::ErrorOr<T>;

  virtual ~TxtRecord() = default;

  // Sets the value currently stored in this DNS-SD TXT record. Returns error
  // if the provided key is already set or if either the key or value is
  // invalid, and Error::None() otherwise. Case of the key is ignored.
  Error SetValue(const absl::string_view& key, uint8_t value);
  Error SetFlag(const absl::string_view& key, bool value);

  // Reads the value associated with the provided key, or an error if the key
  // is invalid or not present. Case of the key is ignored.
  ErrorOr<uint8_t> GetValue(const absl::string_view& key) const;
  ErrorOr<bool> GetFlag(const absl::string_view& key) const;
};

}  // namespace dnssd

#endif  // DNSSD_PUBLIC_TXT_RECORD_H_
