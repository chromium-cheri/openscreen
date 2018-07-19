// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_MDNS_SCREEN_LISTENER_H_
#define API_MDNS_SCREEN_LISTENER_H_

namespace openscreen {

struct MdnsScreenListenerConfig {
  // TODO(mfoltz): Populate with actual parameters.

  bool dummy_value_for_test = true;
}

class MdnsScreenListener : public ScreenListener {
 public:
  MdnsScreenListener(MdnsScreenListenerConfig config);

 private:
  const MdnsScreenListenerConfig config_;
}

}

}  // namespace openscreen

#endif  // API_MDNS_SCREEN_LISTENER_H_
