// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_CONNECTION_H_
#define PLATFORM_API_TLS_CONNECTION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "platform/api/network_interface.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TlsConnection {
 public:
  class Client {
   public:
    // Called when |connection| writing is blocked or unblocked. Note
    // that we will do best effort to buffer packets even in blocked state,
    // and will call OnError if we actually deplete the buffer.
    virtual void OnWriteBlocked(TlsConnection* connection, bool blocked) = 0;

    // Called when |connection| experiences an error, such as a read error.
    virtual void OnError(TlsConnection* socket, Error error) = 0;

    // Called when a |packet| arrives on |socket|.
    virtual void OnRead(TlsConnection* socket,
                        std::vector<uint8_t> message) = 0;

   protected:
    virtual ~Client() = default;
  };

  // Sets the client for this instance.
  void SetClient(Client* client) { client_ = client; }

  // Sends a message.
  virtual void Write(const std::vector<uint8_t>& message) = 0;

  // Get the local address.
  virtual const absl::optional<IPEndpoint>& local_address() const = 0;

  // Get the connected remote address.
  virtual const absl::optional<IPEndpoint>& remote_address() const = 0;

 protected:
  explicit TlsConnection(TaskRunner* task_runner) : task_runner_(task_runner) {}

  Client* client() const { return client_; }
  TaskRunner* task_runner() const { return task_runner_; }

 private:
  Client* client_;
  TaskRunner* const task_runner_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsConnection);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CONNECTION_H_
