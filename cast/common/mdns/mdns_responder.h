// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RESPONDER_H_
#define CAST_COMMON_MDNS_MDNS_RESPONDER_H_

#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_sender.h"

namespace cast {
namespace mdns {

class MdnsResponder : MdnsReceiver::Delegate {
 public:
  MdnsResponder(MdnsSender* sender, MdnsReceiver* receiver);
  MdnsResponder(const MdnsResponder& other) = delete;
  MdnsResponder(MdnsResponder&& other) noexcept = delete;
  ~MdnsResponder() override = default;

  MdnsResponder& operator=(const MdnsResponder& other) = delete;
  MdnsResponder& operator=(MdnsResponder&& other) noexcept = delete;

  void OnMessageReceived(const MdnsMessage& message,
                         const IPEndpoint& sender) override;

 private:
  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RESPONDER_H_
