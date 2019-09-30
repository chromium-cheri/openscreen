// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_RUNTIME_CONTEXT_H_
#define PLATFORM_API_RUNTIME_CONTEXT_H_

#include <memory>

namespace openscreen {
namespace platform {

class TaskRunner;
class UdpSocket;
class TlsConnectionFactory;
class TlsConnection;

class RuntimeContext {
 public:
  virtual ~RuntimeContext() = default;

  static std::unique_ptr<RuntimeContext> Create();

  virtual TaskRunner* task_runner() = 0;

  virtual void OnCreate(UdpSocket* socket) = 0;

  virtual void OnCreate(TlsConnection* socket) = 0;

  virtual void OnCreate(TlsConnectionFactory* socket) = 0;

  virtual void OnDestroy(UdpSocket* socket) = 0;

  virtual void OnDestroy(TlsConnection* socket) = 0;

  virtual void OnDestroy(TlsConnectionFactory* socket) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_RUNTIME_CONTEXT_H_
