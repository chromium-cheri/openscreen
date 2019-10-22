// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/txt_record.h"

#include "absl/strings/ascii.h"

namespace openscreen {
namespace discovery {

Error DnsSdTxtRecord::SetValue(const absl::string_view& key,
                               const absl::Span<uint8_t>& value) {
  std::string key_string(key.data(), key.size());

  if (!IsKeyValid(key_string) || !IsKeyValuePairValid(key_string, value)) {
    return Error::Code::kParameterInvalid;
  }

  txt_[key_string] = std::vector<uint8_t>(value.begin(), value.end());
  return Error::None();
}

Error DnsSdTxtRecord::SetFlag(const absl::string_view& key, bool value) {
  std::string key_string(key.data(), key.size());

  if (!IsKeyValid(key_string)) {
    return Error::Code::kParameterInvalid;
  }

  if (value) {
    txt_[key_string] = absl::nullopt;
  } else {
    ClearFlag(key_string);
  }
  return Error::None();
}

ErrorOr<absl::Span<uint8_t>> DnsSdTxtRecord::GetValue(
    const absl::string_view& key) const {
  std::string key_string(key.data(), key.size());

  auto it = txt_.find(key_string);
  if (it == txt_.end()) {
    return Error::Code::kItemNotFound;
  } else if (it->second == absl::nullopt) {
    return Error::Code::kOperationInvalid;
  }

  return absl::Span<uint8_t>(it->second.value().data(),
                             it->second.value().size());
}

ErrorOr<bool> DnsSdTxtRecord::GetFlag(const absl::string_view& key) const {
  std::string key_string(key.data(), key.size());

  auto it = txt_.find(key_string);
  if (it == txt_.end()) {
    return false;
  } else if (it->second == absl::nullopt) {
    return true;
  } else {
    return Error::Code::kOperationInvalid;
  }
}

Error DnsSdTxtRecord::ClearValue(const absl::string_view& key) {
  std::string key_string(key.data(), key.size());

  auto it = txt_.find(key_string);
  if (it != txt_.end()) {
    if (it->second == absl::nullopt) {
      return Error::Code::kOperationInvalid;
    } else {
      txt_.erase(it);
    }
  }
  return Error::None();
}

Error DnsSdTxtRecord::ClearFlag(const absl::string_view& key) {
  std::string key_string(key.data(), key.size());

  auto it = txt_.find(key_string);
  if (it != txt_.end()) {
    if (it->second != absl::nullopt) {
      return Error::Code::kOperationInvalid;
    } else {
      txt_.erase(it);
    }
  }
  return Error::None();
}

bool DnsSdTxtRecord::IsKeyValid(const std::string& key) {
  // The max length of any individual TXT record is 255 bytes.
  if (key.size() > 255) {
    return false;
  }

  // The length of a key must be at least 1.
  if (key.size() < 1) {
    return false;
  }

  // Keys must contain only valid characters.
  for (char key_char : key) {
    if (key_char < char{0x20} || key_char > char{0x7E} || key_char == '=') {
      return false;
    }
  }

  return true;
}

bool DnsSdTxtRecord::IsKeyValuePairValid(const std::string& key,
                                         const absl::Span<uint8_t>& value) {
  // The max length of any individual TXT record is 255 bytes.
  if (key.size() + value.size() + 1 /* for equals */ > 255) {
    return false;
  }

  return true;
}

size_t DnsSdTxtRecord::CaseInsensitiveStringHash::operator()(
    const std::string& key) const {
  return std::hash<std::string>()(absl::AsciiStrToUpper(key));
}

bool DnsSdTxtRecord::CaseInsensitiveStringEquals::operator()(
    const std::string& lhs,
    const std::string& rhs) const {
  return std::equal_to<std::string>()(absl::AsciiStrToUpper(lhs),
                                      absl::AsciiStrToUpper(rhs));
}

}  // namespace discovery
}  // namespace openscreen
