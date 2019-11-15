// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_SERVICE_H_
#define DISCOVERY_DNSSD_PUBLIC_SERVICE_H_

#include <memory>

namespace openscreen {

struct IPEndpoint;
class TaskRunner;

namespace discovery {

class DnsSdPublisher;
class DnsSdQuerier;

// This class provides a wrapper around DnsSdQuerier and DnsSdPublisher to
// allow for an embedder-overridable factory method below.
class DnsSdService {
 public:
  virtual ~DnsSdService() = default;

  // Creates a new DnsSdService instance, to be owned by the caller. On failure,
  // the resulting object will point to nullptr.
  static std::unique_ptr<DnsSdService> Create(TaskRunner* task_runner);

  // Returns the querier instance associated with this service instance. The
  // returned instance is owned by the DnsSdService instance on which this was
  // called. If discovery is not supported, the result of this call will be
  // nullptr.
  virtual DnsSdQuerier* Querier() = 0;

  // Returns the publisher instance associated with this service instance. The
  // returned instance is owned by the DnsSdService instance on which this was
  // called. If publishing is not supported, the result of this call will be
  // nullptr.
  virtual DnsSdPublisher* Publisher() = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_SERVICE_H_
