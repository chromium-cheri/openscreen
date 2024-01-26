// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/common/config.h"

namespace openscreen::discovery {

Config::Config(std::vector<InterfaceInfo> network_info)
  : network_info (std::move(network_info)) {}
Config::Config() = default;
Config::Config(Config&&) noexcept = default;
Config::Config(const Config&) = default;
Config& Config::operator=(Config&&) = default;
Config& Config::operator=(const Config&) = default;
Config::~Config() = default;

}  // namespace openscreen::discovery
