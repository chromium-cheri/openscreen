// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_LOOP_H_
#define PLATFORM_BASE_NETWORK_LOOP_H_

namespace openscreen {
namespace platform {

class TaskRunner;
class UdpSocket;

class SocketReadCallback {
 public:
  virtual ~SocketReadCallback() = 0;

  virtual void OnReadable(UdpSocket* socket) = 0;
};

class SocketWriteCallback {
 public:
  virtual ~SocketWriteCallback() = 0;

  virtual void OnWritable(UdpSocket* socket) = 0;
};

class NetworkLoop {
 public:
  virtual ~NetworkLoop() = 0;

  // All |callback| arguments will be called on the last TaskRunner set by this
  // method.
  virtual void SetTaskRunner(TaskRunner* task_runner) = 0;

  virtual void WaitForReadable(UdpSocket* socket,
                               SocketReadCallback* callback) = 0;
  virtual void CancelReadWait(UdpSocket* socket) = 0;

  virtual void WaitForWritable(UdpSocket* socket,
                               SocketWriteCallback* callback) = 0;
  virtual void CancelWriteWait(UdpSocket* socket) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_LOOP_H_
