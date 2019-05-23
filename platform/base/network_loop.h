// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_LOOP_H_
#define PLATFORM_API_NETWORK_LOOP_H_

#include <map>
#include <mutex>  // NOLINT

#include "platform/api/event_waiter.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

class NetworkLoop;

static constexpr int kUdpMaxPacketSize = 1 << 16;

class UdpReadCallback {
 public:
  struct Packet {
    Packet() = default;
    ~Packet() = default;

    IPEndpoint source;
    IPEndpoint original_destination;
    std::array<uint8_t, kUdpMaxPacketSize> bytes;
    ssize_t length;
    UdpSocket* socket;
  };

  virtual ~UdpReadCallback() = default;

  virtual void OnRead(Packet* data, NetworkLoop* network_runner) = 0;
};

class UdpWriteCallback {
 public:
  virtual ~UdpWriteCallback() = default;

  virtual void OnWrite(NetworkLoop* network_runner) = 0;
};

class NetworkLoop {
 public:
  // Constructor used in production code.
  explicit NetworkLoop(EventWaiter* waiter)
      : NetworkLoop(waiter, std::bind(&EventWaiter::GetWakeUpHandler, waiter)) {
  }
  ~NetworkLoop();

  // Waits for |socket| to be readable and then posts a task to the currently
  // set TaskRunner to run the provided |callback|.
  void ReadAll(UdpSocket* socket, UdpReadCallback* callback);

  // Cancels any pending wait on reading |socket|.
  void CancelReadAll(UdpSocket* socket);

  // Waits for |socket| to be writable and then posts a task to the currently
  // set TaskRunner to run the provided |callback|.
  void WriteAll(UdpSocket* socket, UdpWriteCallback* callback);

  // Cancels any pending wait on writing to |socket|.
  void CancelWriteAll(UdpSocket* socket);

  // Sets the TaskRunner used by this NetworkLoop.
  void SetTaskRunner(TaskRunner* task_runner);

  // Waits for any writes to occur or for timeout to pass, whichever is sooner.
  Error Wait(Clock::duration timeout);

  // Wakes the waiting thread.
  inline void WakeUp() { this->wake_up_handler->Set(); }

 protected:
  // Constructor needed to allow dependency injection for UTs.
  NetworkLoop(EventWaiter* waiter,
              std::function<WakeUpHandler*()> handler_factory);

  // Method to read data from a socket. This will be overridden in UTs.
  virtual UdpReadCallback::Packet* ReadData(UdpSocket* socket);

  // Associations between sockets and callbacks, plus the platform-level
  // EventWaiter. Note that the EventWaiter has not been rolled into this class
  // and the callbacks have not been pushed to the socket layer in order to
  // keep the platform-specific code as simple as possible and maximize
  // code reusability.
  std::map<UdpSocket*, UdpReadCallback*> read_callbacks_;
  std::map<UdpSocket*, UdpWriteCallback*> write_callbacks_;
  EventWaiter* waiter_;

  // Handler to allow waking the Network Handler when running
  WakeUpHandler* wake_up_handler;

  // The task runner on which all callbacks should be run
  TaskRunner* task_runner_;

  // Mutexes for to protect against concurrent modification of socket info.
  std::mutex read_mutex_;
  std::mutex write_mutex_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_LOOP_H_
