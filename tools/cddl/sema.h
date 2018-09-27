// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_SEMA_H_
#define TOOLS_CDDL_SEMA_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/cddl/parse.h"

struct CddlGroup;

struct CddlType {
  enum class Which {
    kDirectChoice,
    kValue,
    kId,
    kMap,
    kArray,
    kGroupChoice,
    kGroupnameChoice,
    kTaggedType,
  };
  struct TaggedType {
    std::string tag_value;
    CddlType* type;
  };
  CddlType();
  ~CddlType();

  Which which;
  union {
    std::vector<CddlType*> direct_choice;
    std::string value_value;
    std::string id;
    CddlGroup* map;
    CddlGroup* array;
    CddlGroup* group_choice;
    TaggedType tagged_type;
  };
};

// TODO(btolsch): group choices
struct CddlGroup {
  struct Entry {
    enum class Which {
      kUninitialized = 0,
      kType,
      kGroup,
    };
    struct EntryType {
      std::string opt_key;
      CddlType* value;
    };
    Entry();
    ~Entry();

    std::string opt_occurrence;
    Which which = Which::kUninitialized;
    union {
      EntryType type;
      CddlGroup* group;
    };
  };

  std::vector<std::unique_ptr<Entry>> entries;
};

struct CddlSymbolTable {
  std::vector<std::unique_ptr<CddlType>> types;
  std::vector<std::unique_ptr<CddlGroup>> groups;
  std::map<std::string, CddlType*> type_map;
  std::map<std::string, CddlGroup*> group_map;
};

struct CppType {
  enum class Which {
    kUninitialized = 0,
    kUint64,
    kString,
    kBytes,
    kVector,
    kEnum,
    kStruct,
    kOptional,
    kDiscriminatedUnion,
    kTaggedType,
  };

  struct Vector {
    CppType* element_type;
  };

  struct Enum {
    std::string name;
    std::vector<CppType*> sub_members;
    std::vector<std::pair<std::string, uint64_t>> members;
  };

  struct Struct {
    enum class KeyType {
      kMap,
      kArray,
      kPlainGroup,
    };
    std::vector<std::pair<std::string, CppType*>> members;
    KeyType key_type;
  };

  struct DiscriminatedUnion {
    std::vector<CppType*> members;
  };

  struct TaggedType {
    uint64_t tag;
    CppType* real_type;
  };

  CppType();
  ~CppType();

  void InitVector();
  void InitEnum();
  void InitStruct();
  void InitDiscriminatedUnion();

  Which which = Which::kUninitialized;
  std::string name;
  union {
    Vector vector_type;
    Enum enum_type;
    Struct struct_type;
    CppType* optional_type;
    DiscriminatedUnion discriminated_union;
    TaggedType tagged_type;
  };
};

struct CppSymbolTable {
  std::vector<std::unique_ptr<CppType>> cpp_types;
  std::map<std::string, CppType*> cpp_type_map;
};

std::pair<bool, CddlSymbolTable> BuildSymbolTable(AstNode* rules);
std::pair<bool, CppSymbolTable> BuildCppTypes(CddlSymbolTable* cddl_table);

#endif  // TOOLS_CDDL_SEMA_H_
