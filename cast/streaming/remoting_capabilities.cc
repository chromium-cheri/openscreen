// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/remoting_capabilities.h"

namespace openscreen::cast {

RemotingCapabilities::RemotingCapabilities() = default;
RemotingCapabilities::RemotingCapabilities(const RemotingCapabilities&) =
    default;
RemotingCapabilities::RemotingCapabilities(RemotingCapabilities&&) noexcept =
    default;
RemotingCapabilities& RemotingCapabilities::operator=(
    const RemotingCapabilities&) = default;
RemotingCapabilities& RemotingCapabilities::operator=(RemotingCapabilities&&) =
    default;
RemotingCapabilities::~RemotingCapabilities() = default;

}  // namespace openscreen::cast
