// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_OFFER_H_
#define CAST_STREAMING_OFFER_H_

#include "platform/base/error.h"

namespace cast {
namespace streaming {

class Offer {
 public:
  enum CastMode {
    kRemoting,
    kMirroring
  };

  openscreen::ErrorOr<Offer> ParseOfferMessage(absl::string_view message);
  CastMode cast_mode() { return cast_mode_; }

 private:
  Offer();

  CastMode cast_mode_
};

}
}

#endif
