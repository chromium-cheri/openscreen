// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_reader.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  openscreen::discovery::MdnsReader reader(data, size);
  openscreen::discovery::MdnsMessage message;
  reader.Read(&message);
  return 0;
}

#if defined(NEEDS_MAIN_TO_CALL_FUZZER_DRIVER)

// Forward declarations of Clang's built-in libFuzzer driver.
namespace fuzzer {
using TestOneInputCallback = int (*)(const uint8_t* data, size_t size);
int FuzzerDriver(int* argc, char*** argv, TestOneInputCallback callback);
}  // namespace fuzzer

int main(int argc, char* argv[]) {
  return fuzzer::FuzzerDriver(&argc, &argv, LLVMFuzzerTestOneInput);
}

#endif  // defined(NEEDS_MAIN_TO_CALL_FUZZER_DRIVER)
