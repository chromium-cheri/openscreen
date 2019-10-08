// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RESPONDER_H_
#define CAST_COMMON_MDNS_MDNS_RESPONDER_H_

#include "cast/common/mdns/mdns_querier.h"
#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace cast {
namespace mdns {

using TaskRunner = openscreen::platform::TaskRunner;

class MdnsResponder {
 public:
  MdnsResponder(MdnsSender* sender,
                MdnsReceiver* receiver,
                MdnsQuerier* querier,
                TaskRunner* task_runner,
                ClockNowFunctionPtr now_function,
                MdnsRandom* random_delay);
  MdnsResponder(const MdnsResponder& other) = delete;
  MdnsResponder(MdnsResponder&& other) noexcept = delete;
  ~MdnsResponder();

  MdnsResponder& operator=(const MdnsResponder& other) = delete;
  MdnsResponder& operator=(MdnsResponder&& other) noexcept = delete;

 private:
  void OnMessageReceived(const MdnsMessage& message);

  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  MdnsQuerier* const querier_;
  TaskRunner* task_runner_;
  const ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RESPONDER_H_
