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
#include "platform/api/tls_write_buffer.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TlsConnection : TlsWriteBuffer::Observer {
 public:
  // Client callbacks are ran on the provided TaskRunner.
  class Client {
   public:
    // Called when |connection| writing is blocked and unblocked, respectively.
    // Note that implementations should do best effort to buffer packets even in
    // blocked state, and should call OnError if we actually overflow the
    // buffer.
    virtual void OnWriteBlocked(TlsConnection* connection) = 0;
    virtual void OnWriteUnblocked(TlsConnection* connection) = 0;

    // Called when |connection| experiences an error, such as a read error.
    virtual void OnError(TlsConnection* socket, Error error) = 0;

    // Called when a |packet| arrives on |socket|.
    virtual void OnRead(TlsConnection* socket,
                        std::vector<uint8_t> message) = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~TlsConnection();

  // Sends a message.
  void Write(const void* data, size_t len);

  // Get the local address.
  virtual const IPEndpoint& local_address() const = 0;

  // Get the connected remote address.
  virtual const IPEndpoint& remote_address() const = 0;

  // Sets the client for this instance.
  void set_client(Client* client) { client_ = client; }

  // TlsWriteBuffer::Observer methods.
  void OnWriteBlocked() override;
  void OnWriteUnblocked() override;
  void OnTooMuchDataWritten() override;

 protected:
  explicit TlsConnection(TaskRunner* task_runner);

  // Called when |connection| experiences an error, such as a read error. This
  // call will be proxied to the Client set for this TlsConnection.
  void OnError(Error error);

  // Called when a |packet| arrives on |socket|. This call will be proxied to
  // the Client set for this TlsConnection.
  void OnRead(std::vector<uint8_t> message);

 private:
  Client* client_;
  TaskRunner* const task_runner_;

  std::unique_ptr<TlsWriteBuffer> write_buffer_;

  friend class TlsWriteBuffer;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsConnection);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CONNECTION_H_
