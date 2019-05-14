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
      os << "Success";
      break;
    case Error::Code::kAgain:
      os << "Transient Failure";
      break;
    case Error::Code::kCborParsing:
      os << "Failure: CborParsing";
      break;
    case Error::Code::kCborEncoding:
      os << "Failure: CborEncoding";
      break;
    case Error::Code::kCborIncompleteMessage:
      os << "Failure: CborIncompleteMessage";
      break;
    case Error::Code::kCborInvalidMessage:
      os << "Failure: CborInvalidMessage";
      break;
    case Error::Code::kCborInvalidResponseId:
      os << "Failure: CborInvalidResponseId";
      break;
    case Error::Code::kNoAvailableReceivers:
      os << "Failure: NoAvailableReceivers";
      break;
    case Error::Code::kRequestCancelled:
      os << "Failure: RequestCancelled";
      break;
    case Error::Code::kNoPresentationFound:
      os << "Failure: NoPresentationFound";
      break;
    case Error::Code::kPreviousStartInProgress:
      os << "Failure: PreviousStartInProgress";
      break;
    case Error::Code::kUnknownStartError:
      os << "Failure: UnknownStartError";
      break;
    case Error::Code::kUnknownRequestId:
      os << "Failure: UnknownRequestId";
      break;
    case Error::Code::kAddressInUse:
      os << "Failure: AddressInUse";
      break;
    case Error::Code::kAlreadyListening:
      os << "Failure: AlreadyListening";
      break;
    case Error::Code::kDomainNameTooLong:
      os << "Failure: DomainNameTooLong";
      break;
    case Error::Code::kDomainNameLabelTooLong:
      os << "Failure: DomainNameLabelTooLong";
      break;
    case Error::Code::kGenericPlatformError:
      os << "Failure: GenericPlatformError";
      break;
    case Error::Code::kIOFailure:
      os << "Failure: IOFailure";
      break;
    case Error::Code::kInitializationFailure:
      os << "Failure: InitializationFailure";
      break;
    case Error::Code::kInvalidIPV4Address:
      os << "Failure: InvalidIPV4Address";
      break;
    case Error::Code::kInvalidIPV6Address:
      os << "Failure: InvalidIPV6Address";
      break;
    case Error::Code::kConnectionFailed:
      os << "Failure: ConnectionFailed";
      break;
    case Error::Code::kSocketOptionSettingFailure:
      os << "Failure: SocketOptionSettingFailure";
      break;
    case Error::Code::kSocketBindFailure:
      os << "Failure: SocketBindFailure";
      break;
    case Error::Code::kSocketClosedFailure:
      os << "Failure: SocketClosedFailure";
      break;
    case Error::Code::kSocketReadFailure:
      os << "Failure: SocketReadFailure";
      break;
    case Error::Code::kSocketSendFailure:
      os << "Failure: SocketSendFailure";
      break;
    case Error::Code::kMdnsRegisterFailure:
      os << "Failure: dnsRegisterFailure";
      break;
    case Error::Code::kNoItemFound:
      os << "Failure: NoItemFound";
      break;
    case Error::Code::kNotImplemented:
      os << "Failure: NotImplemented";
      break;
    case Error::Code::kNotRunning:
      os << "Failure: NotRunning";
      break;
    case Error::Code::kParseError:
      os << "Failure: ParseError";
      break;
    case Error::Code::kUnknownMessageType:
      os << "Failure: UnknownMessageType";
      break;
    case Error::Code::kNoActiveConnection:
      os << "Failure: NoActiveConnection";
      break;
    case Error::Code::kAlreadyClosed:
      os << "Failure: AlreadyClosed";
      break;
    case Error::Code::kNoStartedPresentation:
      os << "Failure: NoStartedPresentation";
      break;
    case Error::Code::kPresentationAlreadyStarted:
      os << "Failure: PresentationAlreadyStarted";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& out, const Error& error);

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
