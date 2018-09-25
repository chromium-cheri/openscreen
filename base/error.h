// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ERROR_H_
#define BASE_ERROR_H_

#include <memory>
#include <ostream>
#include <string>

namespace openscreen {

// Represents an error returned by an OSP library operation.  An error has a
// code and an optional message.
class Error {
 public:
  enum class Code {
    // No error occurred.
    kNone = 0,
    // CBOR parsing error.
    kCborParsing = 1,
  };

  Error() = default;
  Error(const Error& error) = default;
  Error(Error&& error) = default;
  Error(Code code);
  Error(Code code, const std::string& message);
  Error(Code code, std::string&& message);

  Error& operator=(const Error& other) = default;
  Error& operator=(Error&& other) = default;
  bool operator==(const Error& other) const;

  Code code() const { return code_; }
  const std::string& message() const { return message_; }

  static const std::string& CodeToString(Code code);

 private:
  Code code_ = kNone;
  std::string message_;
};

std::ostream& operator<<(std::ostream& out, const Error& error);


// A convenience function to return a single value from a function that can
// return a value or an error.  For normal results, construct with a Value*
// (ErrorOr takes ownership) and the Error will be kNone with an empty message.
// For Error results, construct with an error code and value.
//
// Example:
//
// ErrorOr<Bar> Foo:DoSomething() {
//   if (success) {
//     return new Bar();
//   } else {
//     return Error(kBadThingHappened, "No can do");
//   }
// }
//
// TODO:
// - Add support for type conversions
// - Better support for primitive (non-movable) values
template <typename Value>
class ErrorOr<Value> {
 public:
  ErrorOr(ErrorOr&& error_or) = default;
  ErrorOr(std::unique_ptr<Value> value);
  ErrorOr(Value* value);
  ErrorOr(Error&& error);
  ErrorOr(const Error& error);
  ErrorOr(Error::Code code);
  ErrorOr(Error::Code code, std::string message);
  ErrorOr(Error::Code code, const std::string& message);
  ~ErrorOr();

  ErrorOr& operator=(ErrorOr&& other) = default;

  // Move-only type.
  ErrorOr(const ErrorOr&) = delete;
  ErrorOr& operator=(const ErrorOr&) = delete;

  boolean is_error() const { return error_.code() != Code::kNone; };
  boolean is_value() const { return value_.get(); };
  const Error& error() const { return error_; }
  Error&& error();
  const Value& value() const { return *value_; }
  std::unique_ptr<Value>&& value();

 private:
  Error error_;
  std::unique_ptr<Value> value_;
};

}  // namespace openscreen

#endif  // BASE_ERROR_H_
