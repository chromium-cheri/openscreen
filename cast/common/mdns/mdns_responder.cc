// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_responder.h"

namespace cast {
namespace mdns {

MdnsResponder::MdnsResponder(MdnsSender* sender, MdnsReceiver* receiver)
    : sender_(sender), receiver_(receiver) {
  OSP_DCHECK(sender_ != nullptr);
  OSP_DCHECK(receiver_ != nullptr);
  receiver_->SetQueryDelegate(this);
}

void MdnsResponder::OnMessageReceived(const MdnsMessage& message,
                                      const IPEndpoint& sender) {
  OSP_DCHECK(message.type == MessageType::Query);
}

}  // namespace mdns
}  // namespace cast
