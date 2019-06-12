// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_LOOP_H_
#define PLATFORM_BASE_NETWORK_LOOP_H_

#include <map>
#include <mutex>  // NOLINT

#include "platform/api/event_waiter.h"
#include "platform/api/network_runner.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

// A wrapper around network operations for the service.
class NetworkLoop {
 public:
  static std::unique_ptr<NetworkLoop> Create();

  // Virtual destructor needed to support delete called from unique_ptr.
  virtual ~NetworkLoop();

  // Sets the TaskRunner used by this NetworkLoop.
  void SetTaskRunner(TaskRunner* task_runner);

  // Waits for |socket| to be readable and then posts a task to the currently
  // set TaskRunner to run the provided |callback|.
  // NOTE: Only one read callback can be registered per socket.
  Error ReadRepeatedly(
      UdpSocket* socket,
      std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> callback);

  // Cancels any pending wait on reading |socket|.
  Error CancelRead(UdpSocket* socket);

  // Waits for |socket| to be writable and then posts a task to the currently
  // set TaskRunner to run the provided |callback|.
  // NOTE: Only one write callback can be registered per socket.
  Error WriteAll(UdpSocket* socket, std::function<void()> callback);

  // Cancels any pending wait on writing to |socket|.
  Error CancelWriteAll(UdpSocket* socket);

  // Runs the Wait function in a loop until the below RequestStopSoon function
  // is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

 protected:
  NetworkLoop(std::unique_ptr<EventWaiter> waiter,
              std::unique_ptr<SocketHandler> read_handler,
              std::unique_ptr<SocketHandler> write_handler);

  // Method to read data from a socket. This will be overridden in UTs.
  virtual std::unique_ptr<UdpReadCallback::Packet> ReadData(UdpSocket* socket);

  // Waits for any writes to occur or for timeout to pass, whichever is sooner.
  // NOTE: Must be protected rather than private for UT purposes.
  Error Wait(Clock::duration timeout);

  // Associations between sockets and callbacks, plus the platform-level
  // EventWaiter. Note that the EventWaiter has not been rolled into this class
  // and the callbacks have not been pushed to the socket layer in order to
  // keep the platform-specific code as simple as possible and maximize
  // code reusability.
  std::map<UdpSocket*,
           std::function<void(std::unique_ptr<UdpReadCallback::Packet>)>>
      read_callbacks_;
  std::map<UdpSocket*, std::function<void()>> write_callbacks_;

 private:
  // Abstractions around socket handling to ensure platform independence.
  std::unique_ptr<EventWaiter> waiter_;
  std::unique_ptr<SocketHandler> read_handler_;
  std::unique_ptr<SocketHandler> write_handler_;

  // The task runner on which all callbacks should be run
  TaskRunner* task_runner_;

  // Mutex to protect against concurrent modification of socket info.
  std::mutex mutex_;

  // Specifies whether the RunNetworkLoop method should continue.
  bool continue_network_processing_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_LOOP_H_
