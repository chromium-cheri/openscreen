// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/receiver_listener.h"

namespace openscreen {

ReceiverListenerError::ReceiverListenerError() = default;
ReceiverListenerError::ReceiverListenerError(Code error,
                                             const std::string& message)
    : error(error), message(message) {}
ReceiverListenerError::ReceiverListenerError(
    const ReceiverListenerError& other) = default;
ReceiverListenerError::~ReceiverListenerError() = default;

ReceiverListenerError& ReceiverListenerError::operator=(
    const ReceiverListenerError& other) = default;

ReceiverListener::Metrics::Metrics() = default;
ReceiverListener::Metrics::~Metrics() = default;

ReceiverListener::ReceiverListener(Observer* observer) : observer_(observer) {}
ReceiverListener::~ReceiverListener() = default;

}  // namespace openscreen
