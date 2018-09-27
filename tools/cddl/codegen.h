// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_CODEGEN_H_
#define TOOLS_CDDL_CODEGEN_H_

#include "tools/cddl/sema.h"

bool DumpDefs(int fd, CppSymbolTable* table);
bool DumpEncoders(int fd, CppSymbolTable* table);
bool DumpDecoders(int fd, CppSymbolTable* table);

#endif  // TOOLS_CDDL_CODEGEN_H_
