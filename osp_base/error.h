// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_BASE_ERROR_H_
#define OSP_BASE_ERROR_H_

#include <ostream>
#include <string>
#include <utility>

#include "osp_base/macros.h"

namespace openscreen {

// Represents an error returned by an OSP library operation.  An error has a
// code and an optional message.
class Error {
 public:
  enum class Code : int8_t {
    // No error occurred.
    kNone = 0,

    // A transient condition prevented the operation from proceeding (e.g.,
    // cannot send on a non-blocking socket without blocking). This indicates
    // the caller should try again later.
    kAgain = -1,

    // CBOR errors.
    kCborParsing = 1,
    kCborEncoding,
    kCborIncompleteMessage,
    kCborInvalidResponseId,
    kCborInvalidMessage,

    // Presentation start errors.
    kNoAvailableReceivers,
    kRequestCancelled,
    kNoPresentationFound,
    kPreviousStartInProgress,
    kUnknownStartError,
    kUnknownRequestId,

    kAddressInUse,
    kAlreadyListening,
    kDomainNameTooLong,
    kDomainNameLabelTooLong,

    kGenericPlatformError,

    kIOFailure,
    kInitializationFailure,
    kInvalidIPV4Address,
    kInvalidIPV6Address,
    kConnectionFailed,

    kSocketOptionSettingFailure,
    kSocketBindFailure,
    kSocketClosedFailure,
    kSocketReadFailure,
    kSocketSendFailure,

    kMdnsRegisterFailure,

    kNoItemFound,
    kNotImplemented,
    kNotRunning,

    kParseError,
    kUnknownMessageType,

    kNoActiveConnection,
    kAlreadyClosed,
    kNoStartedPresentation,
    kPresentationAlreadyStarted,
  };

  Error();
  Error(const Error& error);
  Error(Error&& error) noexcept;

  Error(Code code);
  Error(Code code, const std::string& message);
  Error(Code code, std::string&& message);
  ~Error();

  Error& operator=(const Error& other);
  Error& operator=(Error&& other);
  bool operator==(const Error& other) const;
  bool ok() const { return code_ == Code::kNone; }

  operator std::string() { return CodeToString(code_) + ": " + message_; }

  Code code() const { return code_; }
  const std::string& message() const { return message_; }

  static std::string CodeToString(Error::Code code);
  static const Error& None();

 private:
  Code code_ = Code::kNone;
  std::string message_;
};

inline std::ostream& operator<<(std::ostream& os, const Error::Code& code) {
  switch (code) {
    case Error::Code::kNone:
      return os << "Success";
    case Error::Code::kAgain:
      return os << "Transient Failure";
    case Error::Code::kCborParsing:
      return os << "Failure: CborParsing";
    case Error::Code::kCborEncoding:
      return os << "Failure: CborEncoding";
    case Error::Code::kCborIncompleteMessage:
      return os << "Failure: CborIncompleteMessage";
    case Error::Code::kCborInvalidMessage:
      return os << "Failure: CborInvalidMessage";
    case Error::Code::kCborInvalidResponseId:
      return os << "Failure: CborInvalidResponseId";
    case Error::Code::kNoAvailableReceivers:
      return os << "Failure: NoAvailableReceivers";
    case Error::Code::kRequestCancelled:
      return os << "Failure: RequestCancelled";
    case Error::Code::kNoPresentationFound:
      return os << "Failure: NoPresentationFound";
    case Error::Code::kPreviousStartInProgress:
      return os << "Failure: PreviousStartInProgress";
    case Error::Code::kUnknownStartError:
      return os << "Failure: UnknownStartError";
    case Error::Code::kUnknownRequestId:
      return os << "Failure: UnknownRequestId";
    case Error::Code::kAddressInUse:
      return os << "Failure: AddressInUse";
    case Error::Code::kAlreadyListening:
      return os << "Failure: AlreadyListening";
    case Error::Code::kDomainNameTooLong:
      return os << "Failure: DomainNameTooLong";
    case Error::Code::kDomainNameLabelTooLong:
      return os << "Failure: DomainNameLabelTooLong";
    case Error::Code::kGenericPlatformError:
      return os << "Failure: GenericPlatformError";
    case Error::Code::kIOFailure:
      return os << "Failure: IOFailure";
    case Error::Code::kInitializationFailure:
      return os << "Failure: InitializationFailure";
    case Error::Code::kInvalidIPV4Address:
      return os << "Failure: InvalidIPV4Address";
    case Error::Code::kInvalidIPV6Address:
      return os << "Failure: InvalidIPV6Address";
    case Error::Code::kConnectionFailed:
      return os << "Failure: ConnectionFailed";
    case Error::Code::kSocketOptionSettingFailure:
      return os << "Failure: SocketOptionSettingFailure";
    case Error::Code::kSocketBindFailure:
      return os << "Failure: SocketBindFailure";
    case Error::Code::kSocketClosedFailure:
      return os << "Failure: SocketClosedFailure";
    case Error::Code::kSocketReadFailure:
      return os << "Failure: SocketReadFailure";
    case Error::Code::kSocketSendFailure:
      return os << "Failure: SocketSendFailure";
    case Error::Code::kMdnsRegisterFailure:
      return os << "Failure: MdnsRegisterFailure";
    case Error::Code::kNoItemFound:
      return os << "Failure: NoItemFound";
    case Error::Code::kNotImplemented:
      return os << "Failure: NotImplemented";
    case Error::Code::kNotRunning:
      return os << "Failure: NotRunning";
    case Error::Code::kParseError:
      return os << "Failure: ParseError";
    case Error::Code::kUnknownMessageType:
      return os << "Failure: UnknownMessageType";
    case Error::Code::kNoActiveConnection:
      return os << "Failure: NoActiveConnection";
    case Error::Code::kAlreadyClosed:
      return os << "Failure: AlreadyClosed";
    case Error::Code::kNoStartedPresentation:
      return os << "Failure: NoStartedPresentation";
    case Error::Code::kPresentationAlreadyStarted:
      return os << "Failure: PresentationAlreadyStarted";
  }
}

inline std::ostream& operator<<(std::ostream& out, const Error& error) {
  out << error.code() << ": " << error.message();
  return out;
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
template <typename Value>
class ErrorOr {
 public:
  static ErrorOr<Value> None() {
    static ErrorOr<Value> error(Error::Code::kNone);
    return error;
  }

  ErrorOr(ErrorOr&& error_or) = default;
  ErrorOr(const Value& value) : value_(value) {}
  ErrorOr(Value&& value) noexcept : value_(std::move(value)) {}
  ErrorOr(Error error) : error_(std::move(error)) {}
  ErrorOr(Error::Code code) : error_(code) {}
  ErrorOr(Error::Code code, std::string message)
      : error_(code, std::move(message)) {}
  ~ErrorOr() = default;

  ErrorOr& operator=(ErrorOr&& other) = default;

  bool is_error() const { return error_.code() != Error::Code::kNone; }
  bool is_value() const { return !is_error(); }

  // Unlike Error, we CAN provide an operator bool here, since it is
  // more obvious to callers that ErrorOr<Foo> will be true if it's Foo.
  operator bool() const { return is_value(); }

  const Error& error() const { return error_; }

  Error&& MoveError() { return std::move(error_); }

  const Value& value() const { return value_; }

  Value&& MoveValue() { return std::move(value_); }

 private:
  Error error_;
  Value value_;

  OSP_DISALLOW_COPY_AND_ASSIGN(ErrorOr);
};

}  // namespace openscreen

#endif  // OSP_BASE_ERROR_H_
