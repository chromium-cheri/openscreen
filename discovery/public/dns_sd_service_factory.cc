// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/public/dns_sd_service_factory.h"

namespace openscreen {
namespace discovery {

SerialDeletePtr<DnsSdService> CreateDnsSdService(
    TaskRunner* task_runner,
    ReportingClient* reporting_client,
    const Config& config) {
  return DnsSdService::Create(task_runner, reporting_client, config);
}

}  // namespace discovery
}  // namespace openscreen
