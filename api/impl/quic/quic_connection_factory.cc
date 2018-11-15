// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_connection_factory.h"

#include "api/impl/quic/quic_connection_factory_impl.h"
#include "platform/api/logging.h"

namespace openscreen {

// static
QuicConnectionFactory* QuicConnectionFactory::factory_ = nullptr;

// static
QuicConnectionFactory* QuicConnectionFactory::Get() {
  return factory_;
}

// static
void QuicConnectionFactory::Set(QuicConnectionFactory* factory) {
  OSP_DCHECK(!factory_ || !factory);
  factory_ = factory;
}

}  // namespace openscreen
