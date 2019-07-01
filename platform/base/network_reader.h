// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_READER_H_
#define PLATFORM_BASE_NETWORK_READER_H_

#include <map>
#include <mutex>  // NOLINT
#include <unordered_set>

#include "platform/api/network_runner.h"
#include "platform/api/network_waiter.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

// This is the class responsible for watching sockets for readable data, then
// calling the function associated with these sockets once that data is read.
// NOTE: This class will only function as intended while its RunUntilStopped
// method is running.
class NetworkReader {
 public:
  // Create a type for readability
  using Callback =
      std::function<void(std::unique_ptr<UdpReadCallback::Packet>)>;

  // Creates a new instance of this object.
  // NOTE: The provided TaskRunner must be running and must live for the
  // duration of this instance's life.
  NetworkReader(TaskRunner* task_runner);
  virtual ~NetworkReader();

  // Waits for |socket| to be readable and then posts a task to the currently
  // set TaskRunner to run the provided |callback|.
  // NOTE: Only one read callback can be registered per socket. If
  // ReadRepeatedly is called on a socket already being watched, the new call
  // will be ignored and an error will be returned.
  // NOTE: The first read on any newly watched socket may be delayed up to 50
  // ms.
  Error ReadRepeatedly(UdpSocket* socket, Callback callback);

  // Cancels any pending wait on reading |socket|. Following this call, any
  // already pending reads will proceed but their associated callbacks will not
  // fire. This function returns false only if the socket was not yet being
  // watched, and true if the operation is successful  and the socket is no
  // longer watched.
  bool CancelRead(UdpSocket* socket);

  // Returns whether the given socket is currently being read from or not.
  bool IsRead(UdpSocket* socket);

  // Runs the Wait function in a loop until the below RequestStopSoon function
  // is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

 protected:
  // Creates a new instance of this object.
  // NOTE: The provided TaskRunner must be running and must live for the
  // duration of this instance's life.
  NetworkReader(TaskRunner* task_runner, std::unique_ptr<NetworkWaiter> waiter);

  // Method to read data from a socket. This method will not block, but is only
  // expected to be called by WaitAndRead when it detects that a socket has
  // data waiting to be read.
  virtual ErrorOr<std::unique_ptr<UdpReadCallback::Packet>> ReadFromSocket(
      UdpSocket* socket);

  // Waits for any reads to occur or for timeout to pass, whichever is sooner.
  // If an error occurs when calling WaitAndRead, then no callbacks will have
  // been called during the method's execution, but it is still safe to
  // immediately call again.
  // NOTE: Must be protected rather than private for UT purposes.
  // NOTE: If a socket callback is removed in the middle of a wait call, data
  // may be read on this socket and but the callback may not be called. If a
  // socket callback is added in the middle of a wait call, the new socket may
  // not be watched until after this wait call ends.
  Error WaitAndRead(Clock::duration timeout);

  // Associations between sockets and callbacks, plus the platform-level
  // EventWaiter.
  std::map<UdpSocket*, Callback> read_callbacks_;

  // Sockets which will be un-watched at the end of the current wait iteration.
  std::unordered_set<UdpSocket*> sockets_to_unwatch_;

 private:
  // Abstractions around socket handling to ensure platform independence.
  std::unique_ptr<NetworkWaiter> waiter_;

  // The task runner on which all callbacks should be run
  TaskRunner* task_runner_;

  // Mutex to protect against concurrent modification of socket info.
  std::mutex mutex_;

  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReader);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_READER_H_
