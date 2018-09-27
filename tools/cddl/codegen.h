// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_CODEGEN_H_
#define TOOLS_CDDL_CODEGEN_H_

#include "tools/cddl/sema.h"

bool DumpDefs(int fd, CppSymbolTable* table);
bool DumpEncoders(int fd, CppSymbolTable* table);
bool DumpDecoders(int fd, CppSymbolTable* table);
bool DumpHeaderPrologue(int fd, const std::string& header_filename);
bool DumpHeaderEpilogue(int fd, const std::string& header_filename);
bool DumpSourcePrologue(int fd, const std::string& header_filename);
bool DumpSourceEpilogue(int fd);

#endif  // TOOLS_CDDL_CODEGEN_H_
