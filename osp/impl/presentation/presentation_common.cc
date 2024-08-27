// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/presentation/presentation_common.h"

#include <utility>

#include "osp/public/network_service_manager.h"
#include "util/string_util.h"

namespace openscreen::osp {

msgs::PresentationTerminationSource ConvertTerminationSource(
    TerminationSource source) {
  switch (source) {
    case TerminationSource::kController:
      return msgs::PresentationTerminationSource::kController;
    case TerminationSource::kReceiver:
      return msgs::PresentationTerminationSource::kReceiver;
    default:
      return msgs::PresentationTerminationSource::kUnknown;
  }
}

msgs::PresentationTerminationReason ConvertTerminationReason(
    TerminationReason reason) {
  switch (reason) {
    case TerminationReason::kApplicationTerminated:
      return msgs::PresentationTerminationReason::kApplicationRequest;
    case TerminationReason::kUserTerminated:
      return msgs::PresentationTerminationReason::kUserRequest;
    case TerminationReason::kReceiverPresentationReplaced:
      return msgs::PresentationTerminationReason::kReceiverReplacedPresentation;
    case TerminationReason::kReceiverIdleTooLong:
      return msgs::PresentationTerminationReason::kReceiverIdleTooLong;
    case TerminationReason::kReceiverPresentationUnloaded:
      return msgs::PresentationTerminationReason::kReceiverAttemptedToNavigate;
    case TerminationReason::kReceiverShuttingDown:
      return msgs::PresentationTerminationReason::kReceiverPoweringDown;
    case TerminationReason::kReceiverError:
      return msgs::PresentationTerminationReason::kReceiverError;
    default:
      return msgs::PresentationTerminationReason::kUnknown;
  }
}

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

PresentationID::PresentationID(std::string presentation_id)
    : id_(Error::Code::kParseError) {
  // The spec dictates that the presentation ID must be composed
  // of at least 16 ASCII characters.
  bool is_valid = presentation_id.length() >= 16;
  for (const char& c : presentation_id) {
    is_valid &= string_util::ascii_isprint(c);
  }

  if (is_valid) {
    id_ = std::move(presentation_id);
  }
}

}  // namespace openscreen::osp
