// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_COMMON_H_
#define API_PUBLIC_PRESENTATION_COMMON_H_

#include <string>

namespace openscreen {

enum class PresentationTerminationSource {
  kController = 0,
  kReceiver,
};

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
    kNavigated,
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
    virtual void OnClosed() = 0;

    // Closed because the connection object was discarded.
    virtual void OnDiscarded() = 0;

    // Closed because of an error.
    virtual void OnError(const std::string& message) = 0;

    // Terminated through a different connection.
    virtual void OnTerminated(PresentationTerminationSource source) = 0;

    // TODO: Investigate APIs to minimize the number of memory allocations, esp.
    // for large blobs.

    // A string message was received.
    virtual void OnStringMessage(const std::string& message) = 0;

    // A binary message was received.
    virtual void OnBinaryMessage(const void* data, size_t data_length) = 0;
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
  std::string id() const { return id_; }
  std::string url() const { return url_; }
  State state() const { return state_; }

  // Sends a string message.
  void SendString(const std::string& message);

  // Sends a binary message.
  void SendBinary(const void* data, size_t data_length);

  // Closes the connection based on an explicit request from the embedder.
  void RequestClose();

  // Closes a connection discarded by the embedder (page navigated, object
  // GCed).
  void Discard();

  // Terminates the presentation associated with this presentation.
  // TODO: Should this co-exist with the Controller/Receiver terminate methods?
  // That mapping of QUIC streams and connections onto Presentation API concepts
  // may help inform this choice.
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
