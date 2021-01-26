// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_TESTING_SIMPLE_ENVIRONMENT_SUBSCRIBER_H_
#define CAST_STREAMING_TESTING_SIMPLE_ENVIRONMENT_SUBSCRIBER_H_

#include "cast/streaming/environment.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {

class SimpleSubscriber : public Environment::Subscriber {
  void OnEnvironmentReady() {}
  void OnEnvironmentInvalidated(Error error) {
    ASSERT_TRUE(error.ok()) << error;
  }
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_TESTING_SIMPLE_ENVIRONMENT_SUBSCRIBER_H_
