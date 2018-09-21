// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/parse.h"
#include "tools/cddl/sema.h"

#include <fstream>
#include <string>
#include <vector>

std::vector<char> ReadEntireFile(const std::string& filename) {
  std::ifstream input(filename);
  if (!input) {
    return {};
  }

  input.seekg(0, std::ios_base::end);
  size_t length = input.tellg();
  std::vector<char> input_data(length + 1);

  input.seekg(0, std::ios_base::beg);
  input.read(input_data.data(), length);
  input_data[length] = 0;

  return input_data;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  std::vector<char> data = ReadEntireFile(argv[1]);
  if (data.empty()) {
    return 1;
  }

  Parser p{data.data()};
  AstNode* rules = HandleCddl(&p);
  if (!rules) {
    return 1;
  }
  DumpAst(rules);

  if (!BuildSymbolTable(rules) || !BuildCppTypes() || !DumpStructDefs() ||
      !DumpStructDefsAlt() || !DumpEncoders()) {
    return 1;
  }

  return 0;
}
