// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/sema.h"

#include <string.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
  CddlType() : map(nullptr) {}
  ~CddlType() {}

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

// TODO: group choices
struct CddlGroup {
  struct Entry {
    enum class Which {
      kType,
      kGroup,
    };
    struct EntryType {
      std::string opt_key;
      CddlType* value;
    };
    Entry() : group(nullptr) {}
    ~Entry() {}

    std::string opt_occurrence;
    Which which;
    union {
      EntryType type;
      CddlGroup* group;
    };
  };

  std::vector<std::unique_ptr<Entry>> entries;
};

struct CppType {
  enum class Which {
    kUint64,
    kString,
    kVector,
    kEnum,
    kStruct,
    kIndirect,
  };

  struct Vector {
    CppType* element_type;
  };

  struct Enum {
    std::string name;
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

  CppType() : vector_type() {}
  ~CppType() {}

  void InitVector() { which = Which::kVector; }

  void InitEnum() {
    which = Which::kEnum;
    vector_type.~Vector();
    new (&enum_type) Enum();
  }

  void InitStruct() {
    which = Which::kStruct;
    vector_type.~Vector();
    new (&struct_type) Struct();
  }

  Which which;
  std::string name;
  union {
    Vector vector_type;
    Enum enum_type;
    Struct struct_type;
    CppType* indirect_type;
  };
};

std::vector<std::unique_ptr<CppType>> g_cpp_types;
std::map<std::string, CppType*> g_cpp_type_map;

CppType* GetCppType(const std::string& name) {
  auto entry = g_cpp_type_map.find(name);
  if (entry != g_cpp_type_map.end()) {
    return entry->second;
  }
  g_cpp_types.emplace_back(new CppType);
  g_cpp_type_map.emplace(name, g_cpp_types.back().get());
  return g_cpp_types.back().get();
}

using CddlTypePtr = std::unique_ptr<CddlType>;
using CddlGroupPtr = std::unique_ptr<CddlGroup>;
std::vector<CddlTypePtr> g_types;
std::vector<CddlGroupPtr> g_groups;
std::map<std::string, CddlType*> g_type_map;
std::map<std::string, CddlGroup*> g_group_map;
std::string g_one_rule_to_ring_them_all;

CddlType* HandleType(AstNode* type);
CddlGroup* HandleGroup(AstNode* group);

CddlType* HandleType2(AstNode* type2) {
  AstNode* node = type2->children;
  if (node->type == AstNode::Type::kNumber ||
      node->type == AstNode::Type::kText ||
      node->type == AstNode::Type::kBytes) {
    g_types.emplace_back(new CddlType);
    CddlType* value = g_types.back().get();
    value->which = CddlType::Which::kValue;
    new (&value->value_value)
        std::string(node->text, node->text + node->text_size);
    return value;
  } else if (node->type == AstNode::Type::kTypename) {
    if (*type2->text == '~') {
      return nullptr;
    }
    g_types.emplace_back(new CddlType);
    CddlType* id = g_types.back().get();
    id->which = CddlType::Which::kId;
    new (&id->id) std::string(node->text, node->text + node->text_size);
    return id;
  } else if (node->type == AstNode::Type::kType) {
    if (*type2->text == '#' && type2->text[1] == '6' && type2->text[2] == '.') {
      g_types.emplace_back(new CddlType);
      CddlType* tagged_type = g_types.back().get();
      tagged_type->which = CddlType::Which::kTaggedType;
      new (&tagged_type->tagged_type) CddlType::TaggedType();
      char* end = type2->text + 3;
      while ('0' <= *end && *end <= '9') {
        ++end;
      }
      tagged_type->tagged_type.tag_value = std::string(type2->text + 3, end);
      tagged_type->tagged_type.type = HandleType(node);
      return tagged_type;
    }
  } else if (node->type == AstNode::Type::kGroup) {
    if (*type2->text == '{') {
      g_types.emplace_back(new CddlType);
      CddlType* map = g_types.back().get();
      map->which = CddlType::Which::kMap;
      map->map = HandleGroup(node);
      return map;
    } else if (*type2->text == '[') {
      g_types.emplace_back(new CddlType);
      CddlType* array = g_types.back().get();
      array->which = CddlType::Which::kArray;
      array->array = HandleGroup(node);
      return array;
    } else if (*type2->text == '&') {
      g_types.emplace_back(new CddlType);
      CddlType* group_choice = g_types.back().get();
      group_choice->which = CddlType::Which::kGroupChoice;
      group_choice->group_choice = HandleGroup(node);
      return group_choice;
    }
  } else if (node->type == AstNode::Type::kGroupname) {
    if (*type2->text == '&') {
      g_types.emplace_back(new CddlType);
      CddlType* group_choice = g_types.back().get();
      group_choice->which = CddlType::Which::kGroupnameChoice;
      new (&group_choice->id)
          std::string(node->text, node->text + node->text_size);
      return group_choice;
    }
  }
  return nullptr;
}

CddlType* HandleType1(AstNode* type1) {
  return HandleType2(type1->children);
}

CddlType* HandleType(AstNode* type) {
  AstNode* type1 = type->children;
  if (type1->sibling) {
    g_types.emplace_back(new CddlType);
    CddlType* type_choice = g_types.back().get();
    type_choice->which = CddlType::Which::kDirectChoice;
    new (&type_choice->direct_choice) std::vector<CddlType*>();
    while (type1) {
      type_choice->direct_choice.push_back(HandleType1(type1));
      type1 = type1->sibling;
    }
    return type_choice;
  } else {
    return HandleType1(type1);
  }
}

bool HandleGroupEntry(AstNode* group_entry, CddlGroup::Entry* entry);

CddlGroup* HandleGroup(AstNode* group) {
  AstNode* node = group->children->children;
  g_groups.emplace_back(new CddlGroup);
  CddlGroup* group_def = g_groups.back().get();
  while (node) {
    group_def->entries.emplace_back(new CddlGroup::Entry);
    HandleGroupEntry(node, group_def->entries.back().get());
    node = node->sibling;
  }
  return group_def;
}

bool HandleGroupEntry(AstNode* group_entry, CddlGroup::Entry* entry) {
  AstNode* node = group_entry->children;
  if (node->type == AstNode::Type::kOccur) {
    entry->opt_occurrence =
        std::string(node->text, node->text + node->text_size);
    node = node->sibling;
  }
  if (node->type == AstNode::Type::kMemberKey) {
    if (node->text[node->text_size - 1] == '>') {
      return false;
    }
    entry->type.opt_key = std::string(
        node->children->text, node->children->text + node->children->text_size);
    node = node->sibling;
  }
  if (node->type == AstNode::Type::kType) {
    entry->which = CddlGroup::Entry::Which::kType;
    entry->type.value = HandleType(node);
  } else if (node->type == AstNode::Type::kGroupname) {
    return false;
  } else if (node->type == AstNode::Type::kGroup) {
    entry->which = CddlGroup::Entry::Which::kGroup;
    entry->group = HandleGroup(node);
  }
  return true;
}

void DumpType(CddlType* type, int indent_level = 0);
void DumpGroup(CddlGroup* group, int indent_level = 0);

void DumpType(CddlType* type, int indent_level) {
  for (int i = 0; i <= indent_level; ++i) {
    printf("--");
  }
  switch (type->which) {
    case CddlType::Which::kDirectChoice:
      printf("kDirectChoice:\n");
      for (auto& option : type->direct_choice) {
        DumpType(option, indent_level + 1);
      }
      break;
    case CddlType::Which::kValue:
      printf("kValue: %s\n", type->value_value.c_str());
      break;
    case CddlType::Which::kId:
      printf("kId: %s\n", type->id.c_str());
      break;
    case CddlType::Which::kMap:
      printf("kMap:\n");
      DumpGroup(type->map, indent_level + 1);
      break;
    case CddlType::Which::kArray:
      printf("kArray:\n");
      DumpGroup(type->array, indent_level + 1);
      break;
    case CddlType::Which::kGroupChoice:
      printf("kGroupChoice:\n");
      DumpGroup(type->group_choice, indent_level + 1);
      break;
    case CddlType::Which::kGroupnameChoice:
      printf("kGroupnameChoice:\n");
      break;
    case CddlType::Which::kTaggedType:
      printf("kTaggedType: %s\n", type->tagged_type.tag_value.c_str());
      DumpType(type->tagged_type.type, indent_level + 1);
      break;
  }
}

void DumpGroup(CddlGroup* group, int indent_level) {
  for (auto& entry : group->entries) {
    for (int i = 0; i <= indent_level; ++i) {
      printf("--");
    }
    switch (entry->which) {
      case CddlGroup::Entry::Which::kType:
        printf("kType:");
        if (!entry->opt_occurrence.empty()) {
          printf(" %s", entry->opt_occurrence.c_str());
        }
        if (!entry->type.opt_key.empty()) {
          printf(" %s =>", entry->type.opt_key.c_str());
        }
        printf("\n");
        DumpType(entry->type.value, indent_level + 1);
        break;
      case CddlGroup::Entry::Which::kGroup:
        printf("kGroup: %s\n", entry->opt_occurrence.c_str());
        DumpGroup(entry->group, indent_level + 1);
        break;
    }
  }
}

void DumpSymbolTable() {
  for (auto& entry : g_type_map) {
    printf("%s\n", entry.first.c_str());
    DumpType(entry.second);
    printf("\n");
  }
  for (auto& entry : g_group_map) {
    printf("%s\n", entry.first.c_str());
    DumpGroup(entry.second);
    printf("\n");
  }
}

bool BuildSymbolTable(AstNode* rules) {
  AstNode* node = rules->children;
  if (node->type != AstNode::Type::kTypename &&
      node->type != AstNode::Type::kGroupname) {
    return false;
  }
  bool is_type = node->type == AstNode::Type::kTypename;
  g_one_rule_to_ring_them_all =
      std::string(node->text, node->text + node->text_size);

  std::cout << g_one_rule_to_ring_them_all << std::endl;

  node = node->sibling;
  if (node->type != AstNode::Type::kAssign) {
    return false;
  }

  node = node->sibling;
  if (is_type) {
    CddlType* type = HandleType(node);
    if (!type) {
      return false;
    }
    g_type_map.emplace(g_one_rule_to_ring_them_all, type);
  } else {
    g_groups.emplace_back(new CddlGroup);
    CddlGroup* group = g_groups.back().get();
    group->entries.emplace_back(new CddlGroup::Entry);
    HandleGroupEntry(node, group->entries.back().get());
    g_group_map.emplace(g_one_rule_to_ring_them_all, group);
  }

  AstNode* rule = rules->sibling;
  while (rule) {
    node = rule->children;
    if (node->type != AstNode::Type::kTypename &&
        node->type != AstNode::Type::kGroupname) {
      return false;
    }
    bool is_type = node->type == AstNode::Type::kTypename;
    auto name = std::string(node->text, node->text + node->text_size);

    node = node->sibling;
    if (node->type != AstNode::Type::kAssign) {
      return false;
    }

    node = node->sibling;
    if (is_type) {
      CddlType* type = HandleType(node);
      if (!type) {
        return false;
      }
      g_type_map.emplace(name, type);
    } else {
      g_groups.emplace_back(new CddlGroup);
      CddlGroup* group = g_groups.back().get();
      group->entries.emplace_back(new CddlGroup::Entry);
      HandleGroupEntry(node, group->entries.back().get());
      g_group_map.emplace(name, group);
    }
    rule = rule->sibling;
  }

  DumpSymbolTable();
  return true;
}

CppType* MakeCppType(CddlType* cddl_type) {
  switch (cddl_type->which) {
    case CddlType::Which::kId: {
      if (cddl_type->id == "uint") {
        g_cpp_types.emplace_back(new CppType);
        CppType* cpp_type = g_cpp_types.back().get();
        cpp_type->which = CppType::Which::kUint64;
        return cpp_type;
      } else if (cddl_type->id == "text") {
        g_cpp_types.emplace_back(new CppType);
        CppType* cpp_type = g_cpp_types.back().get();
        cpp_type->which = CppType::Which::kString;
        return cpp_type;
      } else {
        return GetCppType(cddl_type->id);
      }
    } break;
    case CddlType::Which::kArray: {
      if (cddl_type->array->entries.size() != 1 ||
          cddl_type->array->entries[0]->opt_occurrence.empty()) {
        return nullptr;
      }
      g_cpp_types.emplace_back(new CppType);
      CppType* cpp_type = g_cpp_types.back().get();
      cpp_type->which = CppType::Which::kVector;
      cpp_type->vector_type.element_type =
          GetCppType(cddl_type->array->entries[0]->type.value->id);
      return cpp_type;
    } break;
    default:
      return nullptr;
  }
}

bool AddMembersToStruct(
    CppType* cpp_type,
    const std::vector<std::unique_ptr<CddlGroup::Entry>>& entries) {
  for (const auto& x : entries) {
    if (x->which == CddlGroup::Entry::Which::kType) {
      if (x->type.opt_key.empty()) {
        if (x->type.value->which != CddlType::Which::kId) {
          return false;
        }
        cpp_type->struct_type.members.emplace_back(
            x->type.value->id, GetCppType(x->type.value->id));
      } else {
        CppType* member_type = MakeCppType(x->type.value);
        if (!member_type) {
          return false;
        }
        cpp_type->struct_type.members.emplace_back(x->type.opt_key,
                                                   member_type);
      }
    } else {
      if (!AddMembersToStruct(cpp_type, x->group->entries)) {
        return false;
      }
    }
  }
  return true;
}

bool BuildCppTypes() {
  for (const auto& type_entry : g_type_map) {
    CddlType* type = type_entry.second;
    switch (type->which) {
      case CddlType::Which::kId: {
        if (type->id == "uint") {
          CppType* cpp_type = GetCppType(type_entry.first);
          cpp_type->which = CppType::Which::kUint64;
        } else if (type->id == "text") {
          CppType* cpp_type = GetCppType(type_entry.first);
          cpp_type->which = CppType::Which::kString;
        } else {
          CppType* cpp_type = GetCppType(type_entry.first);
          cpp_type->which = CppType::Which::kIndirect;
          cpp_type->indirect_type = GetCppType(type->id);
        }
      } break;
      case CddlType::Which::kMap: {
        CppType* cpp_type = GetCppType(type_entry.first);
        cpp_type->InitStruct();
        cpp_type->struct_type.key_type = CppType::Struct::KeyType::kMap;
        cpp_type->name = type_entry.first;
        if (!AddMembersToStruct(cpp_type, type->map->entries)) {
          return false;
        }
      } break;
      case CddlType::Which::kArray: {
        CppType* cpp_type = GetCppType(type_entry.first);
        cpp_type->InitStruct();
        cpp_type->struct_type.key_type = CppType::Struct::KeyType::kArray;
        cpp_type->name = type_entry.first;
        if (!AddMembersToStruct(cpp_type, type->map->entries)) {
          return false;
        }
      } break;
      case CddlType::Which::kGroupChoice: {
        CppType* cpp_type = GetCppType(type_entry.first);
        cpp_type->InitEnum();
        cpp_type->name = type_entry.first;
        for (const auto& x : type->group_choice->entries) {
          if (!x->opt_occurrence.empty() ||
              x->which != CddlGroup::Entry::Which::kType ||
              x->type.value->which != CddlType::Which::kValue ||
              x->type.opt_key.empty()) {
            return false;
          }
          cpp_type->enum_type.members.emplace_back(
              x->type.opt_key, atoi(x->type.value->value_value.c_str()));
        }
      } break;
      case CddlType::Which::kDirectChoice: {
      } break;
      default:
        return false;
    }
  }
  for (const auto& group_entry : g_group_map) {
    CppType* cpp_type = GetCppType(group_entry.first);
    cpp_type->InitStruct();
    cpp_type->struct_type.key_type = CppType::Struct::KeyType::kPlainGroup;
    if (!AddMembersToStruct(cpp_type, group_entry.second->entries)) {
      return false;
    }
  }
  return true;
}

std::string ToCId(const std::string& x) {
  std::string result(x);
  for (auto& c : result) {
    if (c == '-') {
      c = '_';
    }
  }
  return result;
}

std::string ToCTypename(const std::string& x) {
  std::string result(x);
  result[0] = (result[0] - 'a') + 'A';
  size_t new_size = 1;
  for (size_t i = 1; i < result.size(); ++i, ++new_size) {
    if (result[i] == '-') {
      ++i;
      result[new_size] = (result[i] - 'a') + 'A';
    } else {
      result[new_size] = result[i];
    }
  }
  result.resize(new_size);
  return result;
}

std::string ToCType(CddlType* type) {
  switch (type->which) {
    case CddlType::Which::kArray:
      if (type->array->entries.size() != 1 ||
          type->array->entries[0]->opt_occurrence != "*") {
        return {};
      }
      return std::string("std::vector<") +
             ToCType(type->array->entries[0]->type.value) + std::string(">");
      break;
    case CddlType::Which::kId:
      if (type->id == "uint") {
        return "uint64_t";
      } else if (type->id == "text") {
        return "std::string";
      } else {
        auto type_entry = g_type_map.find(type->id);
        if (type_entry != g_type_map.end()) {
          std::string x = ToCType(type_entry->second);
          if (x.empty()) {
            return ToCTypename(type->id);
          }
          return x;
        } else {
          return ToCTypename(type->id);
        }
      }
      break;
    default:
      break;
  }
  return {};
}

bool DumpStructMap(CddlGroup* map, int indent_level = 1) {
  for (auto& entry : map->entries) {
    if (!entry->opt_occurrence.empty()) {
      return false;
    }
    if (entry->which == CddlGroup::Entry::Which::kType) {
      if (entry->type.opt_key.empty()) {
        if (entry->type.value->which != CddlType::Which::kId) {
          return false;
        }
        auto type_entry = g_type_map.find(entry->type.value->id);
        if (type_entry != g_type_map.end()) {
        } else {
          auto group_entry = g_group_map.find(entry->type.value->id);
          if (group_entry != g_group_map.end()) {
            if (!DumpStructMap(group_entry->second, indent_level)) {
              return false;
            }
          } else {
            return false;
          }
        }
      } else {
        for (int i = 0; i < indent_level; ++i) {
          printf("  ");
        }
        printf("%s %s;\n", ToCType(entry->type.value).c_str(),
               ToCId(entry->type.opt_key).c_str());
      }
    } else {
      if (!DumpStructMap(entry->group, indent_level)) {
        return false;
      }
    }
  }
  return true;
}

bool DumpStructDefs() {
  printf("\nStructs\n");
  CddlType* root = g_type_map[g_one_rule_to_ring_them_all];
  if (root->which != CddlType::Which::kDirectChoice) {
    return false;
  }
  for (auto* option : root->direct_choice) {
    if (option->which != CddlType::Which::kTaggedType ||
        option->tagged_type.type->which != CddlType::Which::kId) {
      return false;
    }
    auto type_entry = g_type_map.find(option->tagged_type.type->id);
    if (type_entry != g_type_map.end()) {
      switch (type_entry->second->which) {
        case CddlType::Which::kDirectChoice:
          break;
        case CddlType::Which::kValue:
          break;
        case CddlType::Which::kId:
          break;
        case CddlType::Which::kMap:
          printf("struct %s {\n", ToCTypename(type_entry->first).c_str());
          if (!DumpStructMap(type_entry->second->map)) {
            return false;
          }
          printf("};\n\n");
          break;
        case CddlType::Which::kArray:
          break;
        case CddlType::Which::kGroupChoice:
          break;
        case CddlType::Which::kGroupnameChoice:
          break;
        case CddlType::Which::kTaggedType:
          break;
      }
    } else {
      auto group_entry = g_group_map.find(option->tagged_type.type->id);
      if (group_entry != g_group_map.end()) {
      } else {
        return false;
      }
    }
  }

  printf("enum %s_tags {\n", ToCTypename(g_one_rule_to_ring_them_all).c_str());
  for (auto* option : root->direct_choice) {
    printf("  %s,\n", option->tagged_type.tag_value.c_str());
  }
  printf("};\n\n");

  printf("struct %s {\n", ToCTypename(g_one_rule_to_ring_them_all).c_str());
  printf("  %s_tags tag;\n", ToCTypename(g_one_rule_to_ring_them_all).c_str());
  printf("  union {\n");
  for (auto* option : root->direct_choice) {
    printf("    %s %s;\n", ToCTypename(option->tagged_type.type->id).c_str(),
           ToCId(option->tagged_type.type->id).c_str());
  }
  printf("  };\n");
  printf("};\n");
  return true;
}

bool DumpStructMembers(
    const std::vector<std::pair<std::string, CppType*>>& members);

std::string CppTypeToString(CppType* cpp_type) {
  if (cpp_type->which == CppType::Which::kUint64) {
    return "uint64_t";
  } else if (cpp_type->which == CppType::Which::kString) {
    return "std::string";
  } else if (cpp_type->which == CppType::Which::kVector) {
    std::string element_string =
        CppTypeToString(cpp_type->vector_type.element_type);
    if (element_string.empty()) {
      return std::string();
    }
    return std::string("std::vector<") + element_string + std::string(">");
  } else if (cpp_type->which == CppType::Which::kEnum) {
    return ToCTypename(cpp_type->name);
  } else if (cpp_type->which == CppType::Which::kIndirect) {
    return CppTypeToString(cpp_type->indirect_type);
  }
  return std::string();
}

bool DumpStructMembers(
    const std::vector<std::pair<std::string, CppType*>>& members) {
  for (const auto& x : members) {
    std::string type_string;
    if (x.second->which == CppType::Which::kStruct) {
      if (x.second->struct_type.key_type ==
          CppType::Struct::KeyType::kPlainGroup) {
        if (!DumpStructMembers(x.second->struct_type.members)) {
          return false;
        }
        continue;
      } else {
        type_string = ToCTypename(x.first);
      }
    } else {
      type_string = CppTypeToString(x.second);
    }
    if (type_string.empty()) {
      return false;
    }
    printf("  %s %s;\n", type_string.c_str(), ToCId(x.first).c_str());
  }
  return true;
}

bool DumpStructDefsAlt() {
  for (const auto& entry : g_cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    switch (type->which) {
      case CppType::Which::kEnum: {
        printf("\nenum %s {\n", ToCTypename(name).c_str());
        for (const auto& x : type->enum_type.members) {
          printf("  k%s = %lu,\n", ToCTypename(x.first).c_str(), x.second);
        }
        printf("};\n");
      } break;
      case CppType::Which::kStruct: {
        if (type->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
          continue;
        }
        printf("\nstruct %s {\n", ToCTypename(name).c_str());
        if (!DumpStructMembers(type->struct_type.members)) {
          return false;
        }
        printf("};\n");
      } break;
      default:
        break;
    }
  }
  return true;
}

int GetMapSize(const std::vector<std::pair<std::string, CppType*>>& members) {
  int num_members = 0;
  for (const auto& x : members) {
    if (x.second->which == CppType::Which::kStruct &&
        x.second->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
      num_members += GetMapSize(x.second->struct_type.members);
    } else {
      ++num_members;
    }
  }
  return num_members;
}

bool DumpMapEncoder(
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int encoder_depth = 1);
bool DumpArrayEncoder(
    const std::vector<std::pair<std::string, CppType*>>& members,
    int encoder_depth = 1);

bool DumpEncoder(const std::string& name,
                 CppType* cpp_type,
                 int encoder_depth) {
  switch (cpp_type->which) {
    case CppType::Which::kStruct:
      if (cpp_type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
        if (!DumpMapEncoder(name, cpp_type->struct_type.members,
                            encoder_depth)) {
          return false;
        }
        return true;
      } else if (cpp_type->struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        if (!DumpArrayEncoder(cpp_type->struct_type.members, encoder_depth)) {
          return false;
        }
        return true;
      } else {
        for (const auto& x : cpp_type->struct_type.members) {
          printf(
              "  CBOR_RETURN_ON_ERROR(\n"
              "      cbor_encode_text_string(&encoder%d, \"%s\", "
              "sizeof(\"%s\") - "
              "1));\n",
              encoder_depth, x.first.c_str(), x.first.c_str());
          if (!DumpEncoder(name + std::string(".") + ToCId(x.first), x.second,
                           encoder_depth)) {
            return false;
          }
        }
        return true;
      }
      break;
    case CppType::Which::kUint64:
      printf("  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder%d, %s));\n",
             encoder_depth, ToCId(name).c_str());
      return true;
      break;
    case CppType::Which::kString: {
      std::string cid = ToCId(name);
      printf("  CBOR_RETURN_ON_ERROR(IsValidUtf8(%s));\n", cid.c_str());
      printf("  CBOR_RETURN_ON_ERROR(\n");
      printf(
          "      cbor_encode_text_string(&encoder%d, %s.c_str(), "
          "%s.size()));\n",
          encoder_depth, cid.c_str(), cid.c_str());
      return true;
    } break;
    case CppType::Which::kVector: {
      std::string cid = ToCId(name);
      printf("  CborEncoder encoder%d;\n", encoder_depth + 1);
      printf("  CBOR_RETURN_ON_ERROR(\n");
      printf(
          "      cbor_encoder_create_array(&encoder%d, &encoder%d, "
          "%s.size()));\n",
          encoder_depth, encoder_depth + 1, cid.c_str());
      printf("  for (const auto& x : %s) {\n", cid.c_str());
      if (!DumpEncoder("x", cpp_type->vector_type.element_type,
                       encoder_depth + 1)) {
        return false;
      }
      printf("  }\n");
      printf(
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
          "&encoder%d));\n",
          encoder_depth, encoder_depth + 1);
      return true;
    } break;
    case CppType::Which::kEnum: {
      printf("  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder%d, %s));\n",
             encoder_depth, ToCId(name).c_str());
      return true;
    } break;
    default:
      break;
  }
  return false;
}

bool DumpMapEncoder(
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int encoder_depth) {
  printf("  CborEncoder encoder%d;\n", encoder_depth);
  printf(
      "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder%d, &encoder%d, "
      "%d));\n",
      encoder_depth - 1, encoder_depth, GetMapSize(members));

  for (const auto& x : members) {
    std::string basename = name;
    if (x.second->which != CppType::Which::kStruct ||
        x.second->struct_type.key_type !=
            CppType::Struct::KeyType::kPlainGroup) {
      printf(
          "  CBOR_RETURN_ON_ERROR(\n"
          "      cbor_encode_text_string(&encoder%d, \"%s\", sizeof(\"%s\") - "
          "1));\n",
          encoder_depth, x.first.c_str(), x.first.c_str());
      basename = basename + std::string(".") + x.first;
    }
    if (!DumpEncoder(basename, x.second, encoder_depth)) {
      return false;
    }
  }

  printf(
      "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
      "&encoder%d));\n",
      encoder_depth - 1, encoder_depth);
  return true;
}

bool DumpArrayEncoder(
    const std::vector<std::pair<std::string, CppType*>>& members,
    int encoder_depth) {
  return false;
}

bool DumpEncoders() {
  for (const auto& entry : g_cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCTypename(name);
    printf("\nssize_t Encode%s(\n", cpp_name.c_str());
    printf("    const %s& data,\n", cpp_name.c_str());
    printf("    uint8_t* buffer,\n    size_t length) {\n");
    printf("  CborEncoder encoder0;\n");
    printf("  cbor_encoder_init(&encoder0, buffer, length, 0);\n");

    if (type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!DumpMapEncoder("data", type->struct_type.members)) {
        return false;
      }
    } else {
      if (!DumpArrayEncoder(type->struct_type.members)) {
        return false;
      }
    }

    printf(
        "  size_t extra_bytes_needed = "
        "cbor_encoder_get_extra_bytes_needed(&encoder0);\n");
    printf("  if (extra_bytes_needed) {\n");
    printf("    return static_cast<ssize_t>(length + extra_bytes_needed);\n");
    printf("  } else {\n");
    printf(
        "    return "
        "static_cast<ssize_t>(cbor_encoder_get_buffer_size(&encoder0, "
        "buffer));\n");
    printf("  }\n");
    printf("}\n");
  }
  return true;
}
