// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_H_

#include <functional>

#include "discovery/mdns/mdns_records.h"
#include "platform/base/macros.h"

namespace openscreen {

struct IPEndpoint;

namespace platform {

class TaskRunner;

}  // namespace platform
namespace discovery {

class MdnsMessage;
class MdnsPublisher;
class MdnsRandom;
class MdnsReceiver;
class MdnsRecordChangedCallback;
class MdnsSender;
class MdnsQuerier;

class MdnsResponder {
 public:
  MdnsResponder(MdnsPublisher* publisher,
                MdnsSender* sender,
                MdnsReceiver* receiver,
                platform::TaskRunner* task_runner,
                MdnsRandom* random_delay);
  ~MdnsResponder();

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsResponder);

 private:
  void OnMessageReceived(const MdnsMessage& message, const IPEndpoint& src);

  void SendResponse(MdnsQuestion question,
                    std::function<void(const MdnsMessage&)> send_response,
                    bool is_exclusive_owner);

  MdnsPublisher* const publisher_;
  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  platform::TaskRunner* const task_runner_;
  MdnsRandom* const random_delay_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_RESPONDER_H_
