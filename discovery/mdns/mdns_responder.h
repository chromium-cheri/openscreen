// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_H_

#include "platform/api/time.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TaskRunner;

}  // namespace platform
namespace discovery {

class MdnsMessage;
class MdnsRandom;
class MdnsReceiver;
class MdnsRecord;
class MdnsRecordChangedCallback;
class MdnsSender;
class MdnsQuerier;

class MdnsResponder {
 public:
  MdnsResponder(MdnsSender* sender,
                MdnsReceiver* receiver,
                MdnsQuerier* querier,
                platform::TaskRunner* task_runner,
                platform::ClockNowFunctionPtr now_function,
                MdnsRandom* random_delay);
  ~MdnsResponder();

  void RegisterRecord(const MdnsRecord& record);

  void UpdateRegisteredRecord(const MdnsRecord& record);

  void DeregisterRecord(const MdnsRecord& record);

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsResponder);

 private:
  void OnMessageReceived(const MdnsMessage& message);

  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  MdnsQuerier* const querier_;
  platform::TaskRunner* const task_runner_;
  const platform::ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_RESPONDER_H_
