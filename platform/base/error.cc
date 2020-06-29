// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/error.h"

#include <cstring>
#include <sstream>

#define ENUM_CASE(os, code) \
  case code:                \
    return WriteCodeName<code>(os)
#define HANDLE_ENUM_CASES(index)

namespace openscreen {
namespace {

constexpr int kMaxCharSize = 255;
constexpr int kMaxCodeIndex = static_cast<int>(Error::Code::kLastDoNotUse) + 1;
constexpr int kNamePrefixSize = sizeof(
    "uint8_t openscreen::(anonymous namespace)::WriteCodeName(char *) [TCode = "
    "openscreen::Error::Code::");
constexpr int kNameSuffixSize = sizeof("]");

template <Error::Code TCode>
uint8_t WriteCodeName(char* dest) {
  int size = static_cast<int>(sizeof(__PRETTY_FUNCTION__) - kNamePrefixSize -
                              kNameSuffixSize);
  assert(size <= kMaxCharSize && size > 0);

  memcpy(dest, &__PRETTY_FUNCTION__[kNamePrefixSize], size);
  return static_cast<uint8_t>(size);
}

class ErrorNameHandler {
 public:
  ErrorNameHandler() { Initialize<Error::Code::kLastDoNotUse>(names_); }

  std::ostream& PrintName(std::ostream& os, Error::Code code) {
    char* current = names_[static_cast<int>(code)];
    const uint8_t size = static_cast<uint8_t>(current[0]);
    for (int i = 1; static_cast<int>(i) <= size; i++) {
      os << current[i];
    }
    return os;
  }

 private:
  char names_[kMaxCodeIndex][kMaxCharSize + 1];

  template <Error::Code TCode>
  void Initialize(char names[kMaxCodeIndex][kMaxCharSize + 1]) {
    char* current = names_[static_cast<int8_t>(TCode)];
    uint8_t size = WriteCodeName<TCode>(&current[1]);
    current[0] = static_cast<char>(size);
    Initialize<static_cast<Error::Code>(static_cast<int8_t>(TCode) - 1)>(names);
  }

  template <>
  void Initialize<static_cast<Error::Code>(-1)>(
      char names[kMaxCodeIndex][kMaxCharSize + 1]) {}
};

static ErrorNameHandler kErrorNames;

}  // namespace

Error::Error() = default;

Error::Error(const Error& error) = default;

Error::Error(Error&& error) noexcept = default;

Error::Error(Code code) : code_(code) {}

Error::Error(Code code, const std::string& message)
    : code_(code), message_(message) {}

Error::Error(Code code, std::string&& message)
    : code_(code), message_(std::move(message)) {}

Error::~Error() = default;

Error& Error::operator=(const Error& other) = default;

Error& Error::operator=(Error&& other) = default;

bool Error::operator==(const Error& other) const {
  return code_ == other.code_ && message_ == other.message_;
}

bool Error::operator!=(const Error& other) const {
  return !(*this == other);
}

bool Error::operator==(Code code) const {
  return code_ == code;
}

bool Error::operator!=(Code code) const {
  return !(*this == code);
}

std::ostream& operator<<(std::ostream& os, const Error::Code& code) {
  if (code == Error::Code::kNone) {
    return os << "Success";
  }
  if (code == Error::Code::kAgain) {
    return os << "Failure: Transient";
  }

  assert(static_cast<int8_t>(code) >= 0);
  assert(static_cast<int8_t>(code) <
         static_cast<int8_t>(Error::Code::kLastDoNotUse));
  os << "Failure: ";
  return kErrorNames.PrintName(os, code);
}

std::string Error::ToString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

std::ostream& operator<<(std::ostream& out, const Error& error) {
  out << error.code() << " = \"" << error.message() << "\"";
  return out;
}

// static
const Error& Error::None() {
  static Error& error = *new Error(Code::kNone);
  return error;
}

#undef ENUM_CASE

}  // namespace openscreen
