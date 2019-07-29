// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_QUERIER_H_
#define CAST_COMMON_MDNS_MDNS_QUERIER_H_

#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_sender.h"

namespace cast {
namespace mdns {

class MdnsQuerier : MdnsReceiver::Delegate {
 public:
  MdnsQuerier(MdnsSender* sender, MdnsReceiver* receiver);
  MdnsQuerier(const MdnsQuerier& other) = delete;
  MdnsQuerier(MdnsQuerier&& other) noexcept = delete;
  ~MdnsQuerier() override = default;

  MdnsQuerier& operator=(const MdnsQuerier& other) = delete;
  MdnsQuerier& operator=(MdnsQuerier&& other) noexcept = delete;

  void OnMessageReceived(const MdnsMessage& message,
                         const IPEndpoint& sender) override;

 private:
  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_QUERIER_H_
