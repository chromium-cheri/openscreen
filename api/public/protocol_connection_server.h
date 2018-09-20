// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_

#include <string>
#include <vector>

#include "api/public/protocol_connection.h"
#include "base/ip_address.h"
#include "base/macros.h"
#include "base/time.h"

namespace openscreen {

// Used to report an error from a ProtocolConnectionServer implementation.
struct ProtocolConnectionServerError {
 public:
  // TODO(mfoltz): Add additional error types, as implementations progress.
  enum class Code {
    kNone = 0,
  };

  ProtocolConnectionServerError();
  ProtocolConnectionServerError(Code error, const std::string& message);
  ProtocolConnectionServerError(const ProtocolConnectionServerError& other);
  ~ProtocolConnectionServerError();

  ProtocolConnectionServerError& operator=(
      const ProtocolConnectionServerError& other);

  Code error = Code::kNone;
  std::string message;
};

class ProtocolConnectionServer {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
    kSuspended,
  };

  // For any embedder-specific configuration of the ProtocolConnectionServer.
  struct Config {
    Config();
    ~Config();

    // The connection endpoints that are advertised for LAN connections to this
    // server.  NOTE: This duplicates information in ProtocolPublisher::Config.
    // Is that OK?
    std::vector<IPEndpoint> connection_endpoints;
  };

  // Holds a set of metrics, captured over a specific range of time, about the
  // behavior of a ProtocolConnectionServer instance.
  struct Metrics {
    Metrics();
    ~Metrics();

    // The range of time over which the metrics were collected; end_timestamp >
    // start_timestamp
    timestamp_t start_timestamp = 0;
    timestamp_t end_timestamp = 0;

    // The number of packets and bytes sent over the timestamp range.
    uint64_t num_packets_sent = 0;
    uint64_t num_bytes_sent = 0;

    // The number of packets and bytes received over the timestamp range.
    uint64_t num_packets_received = 0;
    uint64_t num_bytes_received = 0;

    // The maximum number of connections over the timestamp range.  The
    // latter two fields break this down by connections to ipv4 and ipv6
    // endpoints.
    size_t num_connections = 0;
    size_t num_ipv4_connections = 0;
    size_t num_ipv6_connections = 0;
  };

  class Observer : public ProtocolConnectionObserverBase {
   public:
    virtual ~Observer() = default;

    // Called when the state becomes kSuspended.
    virtual void OnSuspended() = 0;

    // Reports an error.
    virtual void OnError(ProtocolConnectionServerError) = 0;

    // Reports metrics.
    virtual void OnMetrics(Metrics) = 0;
  };

  virtual ~ProtocolConnectionServer();

  // Starts the server, listening for new connections on the endpoints in the
  // config object.  Returns true if state() == kStopped and the service will be
  // started, false otherwise.
  virtual bool Start() = 0;

  // Stops the server and frees any resources associated with the server
  // instance.  Returns true if state() != (kStopped|kStopping).
  virtual bool Stop() = 0;

  // NOTE: We need to decide if suspend/resume semantics for QUIC connections
  // are well defined, and if we can resume the server and existing connections
  // in a consistent and useful state.

  // Temporarily stops accepting new connections and sending/receiving data on
  // existing connections.  Any resources associated with existing connections
  // are not freed.
  virtual bool Suspend() = 0;

  // Resumes exchange of data on existing connections and acceptance of new
  // connections.
  virtual bool Resume() = 0;

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this client.
  const ProtocolConnectionServerError& last_error() const { return last_error_; }

 protected:
  explicit ProtocolConnectionServer(const Config& config, Observer* observer);

  Config config_;
  State state_ = State::kStopped;
  ProtocolConnectionServerError last_error_;
  Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolConnectionServer);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_SERVER_H_
