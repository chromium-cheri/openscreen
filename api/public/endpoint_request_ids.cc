// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/endpoint_request_ids.h"

namespace openscreen {

EndpointRequestIds::EndpointRequestIds() = default;
EndpointRequestIds::~EndpointRequestIds() = default;

uint64_t EndpointRequestIds::GetNextRequestId(uint64_t endpoint_id) {
  return ++request_ids_by_endpoint_id_[endpoint_id];
}

void EndpointRequestIds::ResetRequestId(uint64_t endpoint_id) {
  request_ids_by_endpoint_id_.erase(endpoint_id);
}

void EndpointRequestIds::Reset() {
  request_ids_by_endpoint_id_.clear();
}

}  // namespace openscreen
