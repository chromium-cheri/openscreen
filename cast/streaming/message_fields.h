// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_FIELDS_H_
#define CAST_STREAMING_MESSAGE_FIELDS_H_

#include <string>
#include <vector>

namespace openscreen {
namespace cast {

/// NOTE: Constants here are all taken from the Cast V2: Mirroring Control
/// Protocol specification: http://goto.google.com/mirroring-control-protocol
// JSON message field values specific to the Sender Session.
static constexpr char kMessageKeyType[] = "type";
static constexpr char kMessageTypeOffer[] = "OFFER";
static constexpr char kMessageTypeAnswer[] = "ANSWER";

// List of OFFER message fields.
static constexpr char kOfferMessageBody[] = "offer";
static constexpr char kKeyType[] = "type";
static constexpr char kSequenceNumber[] = "seqNum";

/// ANSWER message fields.
static constexpr char kAnswerMessageBody[] = "answer";
static constexpr char kResult[] = "result";
static constexpr char kResultOk[] = "ok";
static constexpr char kResultError[] = "error";
static constexpr char kErrorMessageBody[] = "error";
static constexpr char kErrorCode[] = "code";
static constexpr char kErrorDescription[] = "description";

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_FIELDS_H_
