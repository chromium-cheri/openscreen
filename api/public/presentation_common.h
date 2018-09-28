// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_COMMON_H_
#define API_PUBLIC_PRESENTATION_COMMON_H_

#include <cstdint>
#include <string>
#include <vector>

namespace openscreen {

enum class PresentationTerminationSource {
  kController = 0,
  kReceiver,
};

// TODO(btolsch): Can any of these enums be included from code generated from
// CDDL?
enum class PresentationTerminationReason {
  kReceiverTerminateCalled = 0,
  kReceiverUserTerminated,
  kControllerTerminateCalled,
  kControllerUserTermianted,
  kPresentationReplaced,
  kIdleTooLong,
  kNavigationAttempted,
  kReceiverShuttingDown,
  kReceiverError,
  kUnknown,
};

enum class PresentationErrorCode {
  kNoAvailableScreens,
  kRequestCancelled,
  kNoPresentationFound,
  kPreviousStartInProgress,
  kUnknown,
};

struct PresentationError {
  PresentationError();
  PresentationError(PresentationErrorCode error, const std::string& message);
  PresentationError(const PresentationError& other);
  ~PresentationError();

  PresentationError& operator=(const PresentationError& other);

  PresentationErrorCode error;
  std::string message;
};

class PresentationConnection {
 public:
  enum class CloseReason {
    kClosed = 0,
    kDiscarded,
    kError,
  };

  enum class State {
    // The library is currently attempting to connect to the presentation.
    kConnecting,
    // The connection to the presentation is open and communication is possible.
    kConnected,
    // The connection is closed or could not be opened.  No communication is
    // possible but it may be possible to reopen the connection via
    // ReconnectPresentation.
    kClosed,
    // The connection is closed and the receiver has been terminated.
    kTerminated,
  };

  // An object to receive callbacks related to a single PresentationConnection.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // State changes.
    virtual void OnConnected() = 0;

    // Explicit close by other endpoint.
    virtual void OnRemotelyClosed() = 0;

    // Closed because the connection object was discarded.
    virtual void OnDiscarded() = 0;

    // Closed because of an error.
    virtual void OnError(const std::string& message) = 0;

    // Terminated through a different connection.
    virtual void OnTerminated(PresentationTerminationSource source) = 0;

    // TODO(btolsch): Use a string view class to minimize copies when we have a
    // string view or span implementation.

    // A UTF-8 string message was received.
    virtual void OnStringMessage(std::string message) = 0;

    // A binary message was received.
    virtual void OnBinaryMessage(std::vector<uint8_t> data) = 0;
  };

  struct Info {
    std::string id;
    std::string url;
  };

  // Constructs a new connection using |delegate| for callbacks.
  PresentationConnection(const std::string& id,
                         const std::string& url,
                         Delegate* delegate);

  // Returns the ID and URL of this presentation.
  const std::string& id() const { return id_; }
  const std::string& url() const { return url_; }
  State state() const { return state_; }

  // Sends a UTF-8 string message.
  void SendString(const std::string& message);

  // Sends a binary message.
  void SendBinary(const std::vector<uint8_t>& data);

  // Closes the connection.  This can be based on an explicit request from the
  // embedder or because the connection object is being discarded (page
  // navigated, object GC'd, etc.).
  void Close(CloseReason reason);

  // Terminates the presentation associated with this connection.
  // TODO(btolsch): Should this co-exist with the Controller/Receiver terminate
  // methods?
  void Terminate(PresentationTerminationSource source,
                 PresentationTerminationReason reason);

 private:
  std::string id_;
  std::string url_;
  State state_;
  Delegate* delegate_;
};

}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_COMMON_H_
