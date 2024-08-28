// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_PRESENTATION_PRESENTATION_COMMON_H_
#define OSP_PUBLIC_PRESENTATION_PRESENTATION_COMMON_H_

#include "osp/public/message_demuxer.h"

namespace openscreen::osp {

enum class TerminationSource {
  kController = 0,
  kReceiver,
};

enum class TerminationReason {
  kApplicationTerminated = 0,
  kUserTerminated,
  kReceiverPresentationReplaced,
  kReceiverIdleTooLong,
  kReceiverPresentationUnloaded,
  kReceiverShuttingDown,
  kReceiverError,
};

enum class ResponseResult {
  kSuccess = 0,
  kInvalidUrl,
  kRequestTimedOut,
  kRequestFailedTransient,
  kRequestFailedPermanent,
  kHttpError,
  kUnknown,
};

// These methods retrieve the server and client demuxers, respectively. These
// are retrieved from the protocol connection server and client. The lifetime of
// the demuxers themselves are not well defined: currently they are created in
// the demo component for the ListenerDemo and PublisherDemo methods.
MessageDemuxer& GetServerDemuxer();
MessageDemuxer& GetClientDemuxer();

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_PRESENTATION_PRESENTATION_COMMON_H_
