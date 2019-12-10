// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_DISCOVERY_SERVICE_INFO_H_
#define CAST_COMMON_DISCOVERY_SERVICE_INFO_H_

#include <memory>
#include <string>

#include "platform/base/ip_address.h"

namespace openscreen {
namespace cast {

// This represents the ‘st’ flag in the CastV2 TXT record.
enum ReceiverStatus {
  // The receiver is idle and does not need to be connected now.
  kIdle = 0,

  // The receiver is hosting an activity and invites the sender to join.  The
  // receiver should connect to the running activity using the channel
  // establishment protocol, and then query the activity to determine the next
  // step, such as showing a description of the activity and prompting the user
  // to launch the corresponding app.
  kBusy = 1,
  kJoin = kBusy
};

// This represents the ‘ca’ field in the CastV2 spec.
struct ReceiverCapabilities {
  bool has_video_output;
  bool has_video_input;
  bool has_audio_output;
  bool has_audio_input;
  bool is_dev_mode_enabled;
};

// This is the top-level service info class for CastV2. It describes a specific
// service instance.
struct ServiceInfo {
  // Endpoints for the service. Present if an endpoint of this address type
  // exists and empty otherwise.
  std::unique_ptr<IPEndpoint> v4_address;
  std::unique_ptr<IPEndpoint> v6_address;

  // A UUID for the Cast receiver.
  std::string unique_id;

  // Cast protocol version supported. Begins at 2 and is incremented by 1 with
  // each version.
  uint8_t version;

  // Capabilities supported by this service instance.
  ReceiverCapabilities capabilities;

  // Status of the service instance.
  ReceiverStatus status;

  // The model name of the device, e.g. “Eureka v1”, “Mollie”.
  std::string model_name;

  // The friendly name of the device, e.g. “Living Room TV".
  std::string friendly_name;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_DISCOVERY_SERVICE_INFO_H_
