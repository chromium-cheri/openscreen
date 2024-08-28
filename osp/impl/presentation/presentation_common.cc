// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/presentation/presentation_common.h"

#include "osp/public/network_service_manager.h"

namespace openscreen::osp {

MessageDemuxer& GetServerDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->GetMessageDemuxer();
}

MessageDemuxer& GetClientDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionClient()
      ->GetMessageDemuxer();
}

}  // namespace openscreen::osp
