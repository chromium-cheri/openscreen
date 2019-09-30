// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_RUNTIME_CONTEXT_POSIX_H_
#define PLATFORM_IMPL_RUNTIME_CONTEXT_POSIX_H_

#include <thread>

#include "platform/api/runtime_context.h"
#include "platform/api/time.h"
#include "platform/impl/socket_handle_waiter_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/tls_data_router_posix.h"
#include "platform/impl/udp_socket_reader_posix.h"

namespace openscreen {
namespace platform {

class RuntimeContextPosix {
 public:
  RuntimeContext(ClockNowFunctionPtr clock_func);
  ~RuntimeContextPosix() override;

  static std::unique_ptr<RuntimeContext> Create();

  TaskRunner* task_runner() = 0;

  void OnCreate(UdpSocket* socket) override {
    udp_socket_reader_.OnCreate(static_cast<UdpSocketPosix*>(socket));
  }

  void OnCreate(TlsConnection* connection) override {
    tls_data_router_posix_.RegisterConnection(
        static_cast<TlsConenctionPosix*>(connection));
  }

  virtual void OnCreate(TlsConnectionFactory* factory) {
    static_cast<TlsConnectionFactoryPosix*>(factory)
        .SetStreamSocketNetworkWatcher(&tls_data_router_posix_);
  }

  void OnDestroy(UdpSocket* socket) override {
    udp_socket_reader_.OnDestroy(static_cast<UdpSocketPosix*>(socket));
  }

  void OnDestroy(TlsConnection* connection) override {
    tls_data_router_posix_.OnConnectionDestroyed(
        static_cast<TlsConenctionPosix*>(connection));
  }

  virtual void OnDestroy(TlsConnectionFactory* factory) {}

  OSP_DISALLOW_COPY_AND_ASSIGN(RuntimeContextPosix);

 private:
  // Singletons.
  SocketHandleWaiterPosix socket_handle_waiter_;
  TlsDataRouterPosix tls_data_router_posix_;
  UdpSocketReaderPosix udp_socket_reader_;
  TaskRunnerImpl task_runner_;

  // Threads to run Networking Thread and Task Runner.
  std::thread task_runner_thread_;
  std::thread network_loop_thread_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_RUNTIME_CONTEXT_POSIX_H_
