// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/error.h"

namespace openscreen {

Error::Error(Code code) : code_(code) {}

Error::Error(Code code, const std::string& message) :
  code_(code), message_(message) {}

Error::Error(Code code, std::string&& message) :
  code_(code), message_(std::move(message)) {}

const std::string&& CodeToString(Code code) {
  switch (code) {
  case kNone:
    return "None";
  default:
    return "Unknown";
  }
}

std::ostream& operator<<(std::ostream& out, const Error& error) {
  out << Error::CodeToString(error.code()) << ": " << error.message();
}

ErrorOr::ErrorOr(std::unique_ptr<Value> value) : value_(std::move(value)) {}

ErrorOr::ErrorOr(Value* value) : value_(value) {}

ErrorOr::ErrorOr(Error&& error) : error_(std::move(error)) {}

ErrorOr::ErrorOr(const Error& error) : error_(error) {}

ErrorOr::ErrorOr(Error::Code code) : error_(code) {}

ErrorOr::ErrorOr(Error::Code code, std::string message) : error_(code, std::move(message)) {}

ErrorOr::ErrorOr(Error::Code code, const std::string& message) : error_(code, message) {}

ErrorOr::ErrorOr(Error::Code code, const std::string& message) : error_(code, message) {}

Error&& ErrorOr::error() {
  return std::move(error_);
}

std::unique_ptr<Value>&& ErrorOr::value() {
  return std::move(value_);
}

}  // namespace openscreen
