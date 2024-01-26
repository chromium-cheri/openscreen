// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/connection_settings.h"

#include <utility>

namespace openscreen::cast {

ConnectionSettings::ConnectionSettings(IPEndpoint receiver_endpoint,
                                       std::string path_to_file,
                                       int max_bitrate,
                                       bool should_include_video,
                                       bool use_android_rtp_hack,
                                       bool use_remoting,
                                       bool should_loop_video,
                                       VideoCodec codec)
    : receiver_endpoint(std::move(receiver_endpoint)),
      path_to_file(std::move(path_to_file)),
      max_bitrate(max_bitrate),
      should_include_video(should_include_video),
      use_android_rtp_hack(use_android_rtp_hack),
      use_remoting(use_remoting),
      should_loop_video(should_loop_video),
      codec(codec) {}

ConnectionSettings::ConnectionSettings() = default;
ConnectionSettings::ConnectionSettings(const ConnectionSettings&) = default;
ConnectionSettings::ConnectionSettings(ConnectionSettings&&) noexcept = default;
ConnectionSettings& ConnectionSettings::operator=(const ConnectionSettings&) =
    default;
ConnectionSettings& ConnectionSettings::operator=(ConnectionSettings&&) =
    default;
ConnectionSettings::~ConnectionSettings() = default;

}  // namespace openscreen::cast
