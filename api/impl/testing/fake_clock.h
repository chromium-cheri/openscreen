// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_TESTING_FAKE_CLOCK_H_
#define API_IMPL_TESTING_FAKE_CLOCK_H_

#include "api/public/clock.h"

namespace openscreen {

class FakeClock final : public Clock {
public:
  explicit FakeClock(platform::TimeDelta now);
  ~FakeClock() override;

  platform::TimeDelta Now() override;

  platform::TimeDelta now;
};

}  // namespace openscreen

#endif  // API_IMPL_TESTING_FAKE_CLOCK_H_
