// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_agent.h"

#include <utility>

#include "util/logging.h"

namespace openscreen {
namespace cast {

CastAgent::CastAgent(TaskRunner* task_runner, InterfaceInfo interface)
    : task_runner_(task_runner),
      interface_(std::move(interface)),
      publisher_(task_runner_, interface_) {}

CastAgent::~CastAgent() = default;

Error CastAgent::Start() {
  publisher_.Publish();
  wake_lock_ = ScopedWakeLock::Create();
  return Error::None();
}

Error CastAgent::Stop() {
  // TODO(jophba): implement stopping.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

void CastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
}

void CastAgent::OnMessage(CastSocket* socket,
                          ::cast::channel::CastMessage message) {}

}  // namespace cast
}  // namespace openscreen
