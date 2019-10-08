// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_responder.h"

namespace cast {
namespace mdns {

using Clock = openscreen::platform::Clock;
using ClockNowFunctionPtr = openscreen::platform::ClockNowFunctionPtr;
using Alarm = openscreen::Alarm;

MdnsResponder::MdnsResponder(MdnsSender* sender,
                             MdnsReceiver* receiver,
                             MdnsQuerier* querier,
                             TaskRunner* task_runner,
                             ClockNowFunctionPtr now_function,
                             MdnsRandom* random_delay)
    : sender_(sender),
      receiver_(receiver),
      querier_(querier),
      task_runner_(task_runner),
      now_function_(now_function),
      random_delay_(random_delay) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(querier_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(now_function_);
  OSP_DCHECK(random_delay_);

  receiver_->SetQueryCallback(
      [this](const MdnsMessage& message) { OnMessageReceived(message); });
}

MdnsResponder::~MdnsResponder() {
  receiver_->SetQueryCallback(nullptr);
}

void MdnsResponder::OnMessageReceived(const MdnsMessage& message) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Query);

  // TODO: implement responding to the query
}

}  // namespace mdns
}  // namespace cast
