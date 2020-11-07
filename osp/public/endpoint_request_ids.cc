// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/endpoint_request_ids.h"

#include <utility>

namespace openscreen {
namespace osp {

EndpointRequestIds::EndpointRequestIds(Role role) : role_(role) {}
EndpointRequestIds::~EndpointRequestIds() = default;

uint64_t EndpointRequestIds::GetNextRequestId(uint64_t endpoint_id) {
  auto& next_request_id = request_ids_by_endpoint_id_[endpoint_id];
  if (next_request_id == 0UL && role_ == Role::kServer) {
    next_request_id = 1UL;
  }
  uint64_t result = next_request_id;
  next_request_id += 2;
  return result;
}

void EndpointRequestIds::ResetRequestId(uint64_t endpoint_id) {
  // TODO(crbug.com/openscreen/42): Consider using a timeout to drop the request
  // id counter, and/or possibly set the initial value as part of the handshake.
  request_ids_by_endpoint_id_.erase(endpoint_id);
}

void EndpointRequestIds::Reset() {
  request_ids_by_endpoint_id_.clear();
}

}  // namespace osp
}  // namespace openscreen
