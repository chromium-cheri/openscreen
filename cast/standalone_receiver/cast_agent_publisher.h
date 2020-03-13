// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_CAST_AGENT_PUBLISHER_H_
#define CAST_STANDALONE_RECEIVER_CAST_AGENT_PUBLISHER_H_

#include <memory>

#include "discovery/common/reporting_client.h"
#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "util/serial_delete_ptr.h"

namespace openscreen {
namespace cast {

class CastAgentPublisher : discovery::ReportingClient,
                           discovery::DnsSdPublisher::Client {
 public:
  CastAgentPublisher(TaskRunner* task_runner, InterfaceInfo interface);
  ~CastAgentPublisher();

  Error Publish();
  Error Unpublish();

  // discovery::ReportingClient overrides.
  void OnFatalError(Error error) override;
  void OnRecoverableError(Error error) override;

  // discovery::DnsSdPublisher::Client overrides.
  void OnInstanceClaimed(
      const discovery::DnsSdInstanceRecord& requested_record,
      const discovery::DnsSdInstanceRecord& claimed_record) override;

 private:
  TaskRunner* const task_runner_;
  const InterfaceInfo interface_;

  // The record is updated whenever the receiver status changed, so must be
  // mutable and stored as a property.
  discovery::DnsSdInstanceRecord record_;
  SerialDeletePtr<discovery::DnsSdService> dns_sd_service_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_CAST_AGENT_PUBLISHER_H_
