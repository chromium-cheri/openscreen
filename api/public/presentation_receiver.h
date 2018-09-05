// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_RECEIVER_H_
#define API_PUBLIC_PRESENTATION_RECEIVER_H_

#include <memory>
#include <string>

#include "api/public/presentation_common.h"

namespace openscreen {

enum class PresentationResponseResult {
  kSuccess = 0,
  kInvalidUrl,
  kRequestTimedOut,
  kRequestFailedTransient,
  kRequestFailedPermanent,
  kHttpError,
  kUnknown,
};

class PresentationReceiverObserver {
 public:
  virtual ~PresentationReceiverObserver() = default;

  // Called when a new presentation is requested by a controller.
  virtual bool OnPresentationRequested(const PresentationConnection::Info& info,
                                       const std::string& http_headers) = 0;

  // Called when a presentation is requested to be terminated by a controller.
  virtual void OnPresentationTerminateRequested(
      const PresentationConnection::Info& info,
      PresentationTerminationSource source) = 0;

  // Called when a new connection is being requested of the receiver.  The
  // observer should return a suitable delegate object for the new connection if
  // it accepts the connection and nullptr if it does not.  If it returns a
  // valid pointer, OnConnection will then be called with the new
  // PresentationConnection object.
  virtual PresentationConnection::Delegate* OnConnectionAvailable(
      const PresentationConnection::Info& info) = 0;

  // Called when a new connection to the receiver is created, where the delegate
  // came from OnConnectionAvailable.
  virtual void OnConnection(
      std::unique_ptr<PresentationConnection> connection) = 0;
};

class PresentationConnectionObserver {
 public:
  virtual ~PresentationConnectionObserver() = default;

  virtual void OnConnection(
      std::unique_ptr<PresentationConnection> connection) = 0;
};

class PresentationReceiver {
 public:
  // Returns the single instance.
  static PresentationReceiver* Get();

  // Sets the object to call when a new receiver connection is available.
  void SetReceiverObserver(PresentationReceiverObserver* observer);

  // Called by the embedder to report its response to OnPresentationRequested.
  void ReportPresentationResponseResult(const std::string& presentation_id,
                                        PresentationResponseResult result);

  // Called by the embedder to report that a presentation has been terminated.
  void ReportPresentationTerminated(const std::string& presentation_id,
                                    PresentationTerminationReason reason);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_RECEIVER_H_
