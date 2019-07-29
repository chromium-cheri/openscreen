// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

namespace cast {
namespace mdns {

MdnsQuerier::MdnsQuerier(MdnsSender* sender, MdnsReceiver* receiver)
    : sender_(sender), receiver_(receiver) {
  OSP_DCHECK(sender_ != nullptr);
  OSP_DCHECK(receiver_ != nullptr);
  receiver_->SetResponseDelegate(this);
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) {
  OSP_DCHECK(message.type == MessageType::Response);
}

}  // namespace mdns
}  // namespace cast
