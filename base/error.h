// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ERROR_H_
#define BASE_ERROR_H_

#include <ostream>
#include <string>
#include <utility>

#include "base/macros.h"

namespace openscreen {

enum class GlobalErrorCode {
  // No error occurred.
  kNone = 0,
  // CBOR parsing error.
  kCborParsing = 1,
};

#define ERROR_STRING_CASE(enm, value) \
  case enm::k##value:                 \
    return #value

std::string ErrorCodeToString(GlobalErrorCode code);

// Represents an error returned by an OSP library operation.  An error has a
// code and an optional message.
template <typename Code>
class Error {
 public:
  Error() = default;
  Error(const Error& error) = default;
  Error(Error&& error) noexcept = default;
  explicit Error(Code code) : code_(code) {}
  Error(Code code, std::string message)
      : code_(code), message_(std::move(message)) {}

  Error& operator=(const Error& other) = default;
  Error& operator=(Error&& other) = default;
  bool operator==(const Error& other) const {
    return code_ == other.code_ && message_ == other.message_;
  }

  Code code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  Code code_ = Code::kNone;
  std::string message_;
};

template <typename Code>
std::ostream& operator<<(std::ostream& out, const Error<Code>& error) {
  return out << ErrorCodeToString(error.code()) << ": " << error.message();
}

// A convenience function to return a single value from a function that can
// return a value or an error.  For normal results, construct with a Value*
// (ErrorOr takes ownership) and the Error will be kNone with an empty message.
// For Error results, construct with an error code and value.
//
// Example:
//
// ErrorOr<Bar> Foo::DoSomething() {
//   if (success) {
//     return Bar();
//   } else {
//     return Error(kBadThingHappened, "No can do");
//   }
// }
//
// TODO(mfoltz):
// - Add support for type conversions
// - Better support for primitive (non-movable) values
template <typename Value, typename ErrorCode = GlobalErrorCode>
class ErrorOr {
 public:
  ErrorOr(ErrorOr&& error_or) = default;
  ErrorOr(Value&& value) noexcept
      : is_error_(false), value_(std::move(value)) {}
  ErrorOr(Error<ErrorCode> error) : is_error_(true), error_(std::move(error)) {}
  ErrorOr(ErrorCode code) : is_error_(true), error_(code) {}
  ErrorOr(ErrorCode code, std::string message)
      : is_error_(true), error_(code, std::move(message)) {}
  ~ErrorOr() = default;

  ErrorOr& operator=(ErrorOr&& other) = default;

  bool is_error() const { return is_error_; }
  bool is_value() const { return !is_error(); }
  const Error<ErrorCode>& error() const { return error_; }

  Error<ErrorCode>&& MoveError() { return std::move(error_); }

  const Value& value() const { return value_; }

  Value&& MoveValue() { return std::move(value_); }

 private:
  bool is_error_;
  Error<ErrorCode> error_;
  Value value_;

  DISALLOW_COPY_AND_ASSIGN(ErrorOr);
};

}  // namespace openscreen

#endif  // BASE_ERROR_H_
