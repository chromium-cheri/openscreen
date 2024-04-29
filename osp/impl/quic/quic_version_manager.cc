// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_version_manager.h"

#include <string>

namespace openscreen::osp {

QuicVersionManager::QuicVersionManager(
    quic::ParsedQuicVersionVector supported_versions)
    : quic::QuicVersionManager(supported_versions) {}

QuicVersionManager::~QuicVersionManager() = default;

void QuicVersionManager::RefilterSupportedVersions() {
  quic::QuicVersionManager::RefilterSupportedVersions();
  AddCustomAlpn("osp");
}

}  // namespace openscreen::osp
