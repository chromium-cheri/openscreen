// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_CONTROLLER_H_
#define API_PUBLIC_PRESENTATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "api/public/presentation_common.h"

namespace openscreen {

class PresentationRequestDelegate {
 public:
  virtual ~PresentationRequestDelegate() = default;

  virtual void OnConnection(
      std::unique_ptr<PresentationConnection> connection) = 0;
  virtual void OnError(const PresentationError& error) = 0;
};

class PresentationScreenObserver {
 public:
  virtual ~PresentationScreenObserver() = default;

  // Called when screens compatible with |presentation_url| are known to be
  // available.
  virtual void OnScreensAvailable(const std::string& presentation_url,
                                  const std::string& screen_id) = 0;
};

class PresentationController {
 public:
  // Returns the single instance.
  static PresentationController* Get();

  // Requests screens compatible with |url| and registers |observer| for
  // availability changes.  The screens will be a subset of the screen list
  // maintained by the ScreenListener.  Returns a positive integer id that
  // tracks the registration. If |url| is already being watched for screens,
  // then the id of the previous registration is returned and |observer|
  // replaces the previous registration.
  uint64_t RegisterScreenWatch(const std::string& url,
                               PresentationScreenObserver* observer);

  // Returns the registered observer if |watch_id| corresponds to an active
  // request to watch compatible screens, or nullptr otherwise.
  PresentationScreenObserver* GetScreenWatch(uint64_t watch_id);

  // Cancels compatible screen monitoring for the given watch_id and
  // returns true.  If watch_id does not correspond to an active registration,
  // false is returned.
  bool CancelScreenWatch(uint64_t watch_id);

  // Requests that a new presentation be created on |screen_id| using
  // |presentation_url, with the result passed to |delegate|.
  // |connection_delegate| is passed to the resulting connection.
  void StartPresentation(const std::string& url,
                         const std::string& screen_id,
                         PresentationRequestDelegate* delegate,
                         PresentationConnection::Delegate* conn_delegate);

  // Requests reconnection to the presentation with the given id and URL
  // running on the screen with |screen_id|, with the result passed to
  // |delegate|.  |connection_delegate| is passed to the resulting connection.
  void ReconnectPresentation(const std::string& presentation_id,
                             const std::string& screen_id,
                             PresentationRequestDelegate* delegate,
                             PresentationConnection::Delegate* conn_delegate);

  // Called by the embedder to report that a presentation has been terminated.
  void ReportPresentationTerminated(const std::string& presentation_id,
                                    PresentationTerminationReason reason);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_CONTROLLER_H_
