// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_SERVICE_H_
#define DISCOVERY_DNSSD_PUBLIC_SERVICE_H_

#include <memory>

#include "discovery/dnssd/publisher.h"
#include "discovery/dnssd/querier.h"

namespace openscreen {

template <typename T>
class ErrorOr;
struct IPEndpoint;
class TaskRunner;

namespace discovery {

// This class provides a wrapper around DnsSdQuerier and DnsSdPublisher to
// allow for an embedder-overridable factory method below.
class DnsSdService {
 public:
  virtual ~DnsSdService() = default;

  // Returns a new std::unique_ptr<DnsSdService> on success and an error code
  // on failure, where the failure conditions are defined by the embedder.
  static ErrorOr<std::unique_ptr<DnsSdService>> Create(
      TaskRunner* task_runner,
      const IPEndpoint& endpoint);

  // Returns the querier instance associated with this service instance.
  virtual DnsSdQuerier* Querier() = 0;

  // Returns the publisher instance assocaited with this service instance.
  virtual DnsSdPublisher* Publisher() = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_SERVICE_H_
