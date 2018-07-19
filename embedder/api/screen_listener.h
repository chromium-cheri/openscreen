// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_SCREEN_LISTENER_H_
#define EMBEDDER_SCREEN_LISTENER_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "embedder/screen_info.h"

namespace openscreen {

enum class ScreenListenerState {
  STOPPED = 0,
  STARTING,
  RUNNING,
  STOPPING,
  SEARCHING,
  SUSPENDED,
};

// For now, an opaque number and human readable message.
// TODO: Get more specific once we have a handle on what we should report.
struct ScreenListenerError {
  uint32_t errno;
  std::string message;
};

struct ScreenListenerMetrics {
  // Microseconds after the Unix epoch the metric snapshot was taken.
  uint64_t timestamp;

  // The number of packets and bytes sent since the service started.
  uint64_t num_packets_sent;
  uint64_t num_bytes_sent;

  // The number of packets and bytes received since the service started.
  uint64_t num_packets_received;
  uint64_t num_bytes_received;

  // TODO: Add other useful metrics required for discovery benchmarking
};

class ScreenListenerObserver {
 public:
  virtual ~ScreenListenerObserver() = default;

  // Called on state changes.
  virtual void OnRunning() = 0;
  virtual void OnStopped() = 0;
  virtual void OnSuspended() = 0;
  virtual void OnSearching() = 0;

  // Notifications to changes to the listener's screen list.
  virtual void OnScreenAdded(const ScreenInfo& info) = 0;
  virtual void OnScreenChanged(const ScreenInfo& info) = 0;
  virtual void OnScreenRemoved(const ScreenInfo& info) = 0;
  // Called if all screens have been removed, e.g. all network interfaces
  // have been disabled.
  virtual void OnAllScreensRemoved() = 0;

  // Reports an error.
  virtual void OnError(ScreenListenerError) = 0;

  // Reports metrics.
  virtual void OnMetrics(ScreenListenerMetrics) = 0;
};

class ScreenListener {
 public:
  virtual ~ScreenListener() = default;
  // TODO: Update state transition documentation.

  // Starts listening for screens using the config object.
  // Returns true if state() == STOPPED and the service will be started,
  // false otherwise.
  virtual bool Start() = 0;

  // Starts the listener in SUSPENDED mode.  This could be used to enable
  // immediate search via SearchNow() in the future.
  // Returns true if state() == STOPPED and the service will be started,
  // false otherwise.
  virtual bool StartAndSuspend() = 0;

  // Stops listening and cancels any search in progress.
  // Returns true if state() != STOPPED.
  virtual bool Stop() = 0;

  // Suspends background listening. For example, the tab wanting screen
  // availability might go in the background, meaning we can suspend listening
  // to save power.
  // Returns true if state() == (RUNNING|SEARCHING|STARTING), meaning the
  // suspension will take effect.
  virtual bool Suspend() = 0;

  // Resumes listening.  Returns true if state() == SUSPENDED.
  virtual bool Resume() = 0;

  // Asks the listener to search for screens now, even if the listener is
  // is currently suspended.  If a background search is already in
  // progress, this has no effect.  Returns true if state() ==
  // (RUNNING|SUSPENDED).
  virtual bool SearchNow() = 0;

  // Returns the current state of the listener.
  virtual ScreenListenerState state() const = 0;

  // Returns the last error reported by this listener.
  virtual ScreenListenerError GetLastError() const = 0;

  // Must be called with a valid observer before the listener is started.
  virtual void SetObserver(ScreenListenerObserver* observer) = 0;

  // Returns the current list of screens known to the ScreenListener.
  virtual const std::vector<ScreenInfo>& GetScreens() const = 0;
};

std::ostream& operator<<(std::ostream& os, ScreenListenerState state);

}  // namespace openscreen

#endif
