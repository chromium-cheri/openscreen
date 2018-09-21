// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_SEMA_H_
#define TOOLS_CDDL_SEMA_H_

#include "tools/cddl/parse.h"

bool BuildSymbolTable(AstNode* rules);
bool BuildCppTypes();
bool DumpStructDefs();
bool DumpStructDefsAlt();
bool DumpEncoders();

#endif  // TOOLS_CDDL_SEMA_H_
