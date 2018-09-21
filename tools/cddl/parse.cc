// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/parse.h"

#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

std::vector<std::unique_ptr<AstNode>> g_nodes;

bool IsBinaryDigit(char x) {
  return '0' == x || x == '1';
}

bool IsDigit1(char x) {
  return '1' <= x && x <= '9';
}

bool IsDigit(char x) {
  return '0' <= x && x <= '9';
}

bool IsExtendedAlpha(char x) {
  return (0x41 <= x && x <= 0x5a) || (0x61 <= x && x <= 0x7a) || x == '@' ||
         x == '_' || x == '$';
}

// TODO(btolsch): UTF-8
bool IsPchar(char x) {
  return x >= 0x20;
}

char* IsCrlf(char* x) {
  if (*x == '\n') {
    return x + 1;
  } else if (*x == '\r' && x[1] == '\n') {
    return x + 2;
  }
  return x;
}

bool HandleComment(Parser* p) {
  char* it = p->data;
  if (*it != ';') {
    return false;
  }
  ++it;
  while (true) {
    char* crlf_skip = nullptr;
    if (IsPchar(*it)) {
      ++it;
      continue;
    } else if ((crlf_skip = IsCrlf(it)) != it) {
      it = crlf_skip;
      break;
    }
    return false;
  }
  p->data = it;
  return true;
}

bool HandleSpace(Parser* p) {
  char* it = p->data;
  while (*it == ' ' || *it == ';' || *it == '\r' || *it == '\n') {
    char* crlf_skip = IsCrlf(it);
    if (*it == ' ') {
      ++it;
      continue;
    } else if (crlf_skip != it) {
      it = crlf_skip;
      continue;
    }
    p->data = it;
    if (!HandleComment(p)) {
      return false;
    }
  }
  p->data = it;
  return true;
}

enum class AssignType {
  kInvalid = -1,
  kAssign,
  kAssignT,
  kAssignG,
};

AssignType HandleAssign(Parser* p) {
  char* it = p->data;
  if (*it == '=') {
    p->data = it + 1;
    return AssignType::kAssign;
  } else if (*it == '/') {
    ++it;
    if (*it == '/' && it[1] == '=') {
      p->data = it + 2;
      return AssignType::kAssignG;
    } else if (*it == '=') {
      p->data = it + 1;
      return AssignType::kAssignT;
    }
  }
  return AssignType::kInvalid;
}

AstNode* HandleType1(Parser* p);
AstNode* HandleType(Parser* p);
AstNode* HandleId(Parser* p);

bool HandleUint(Parser* p) {
  char* it = p->data;
  if (*it == '0') {
    if (it[1] == 'x') {
    } else if (it[1] == 'b') {
      it = it + 2;
      if (!IsBinaryDigit(*it)) {
        return false;
      }
    } else {
      p->data = it + 1;
    }
  } else if (IsDigit1(*it)) {
    ++it;
    while (IsDigit(*it)) {
      ++it;
    }
    p->data = it;
  }
  return true;
}

AstNode* HandleNumber(Parser* p) {
  Parser plocal = *p;
  if (IsDigit(*plocal.data) || *plocal.data == '-') {
    if (*plocal.data == '-') {
      ++plocal.data;
    }
    if (!HandleUint(&plocal)) {
      return nullptr;
    }
    g_nodes.emplace_back(new AstNode);
    AstNode* node = g_nodes.back().get();
    node->children = nullptr;
    node->sibling = nullptr;
    node->type = AstNode::Type::kNumber;
    node->text = p->data;
    node->text_size = plocal.data - p->data;
    *p = plocal;
    return node;
  }
  // TODO(btolsch): hexfloat, fraction, exponent.
  return nullptr;
}

AstNode* HandleText(Parser* p) {
  return nullptr;
}

AstNode* HandleBytes(Parser* p) {
  return nullptr;
}

AstNode* HandleValue(Parser* p) {
  AstNode* node = HandleNumber(p);
  if (!node) {
    node = HandleText(p);
  }
  if (!node) {
    HandleBytes(p);
  }
  return node;
}

AstNode* HandleOccur(Parser* p) {
  if (*p->data == '*') {
    g_nodes.emplace_back(new AstNode);
    AstNode* node = g_nodes.back().get();
    node->children = nullptr;
    node->sibling = nullptr;
    node->type = AstNode::Type::kOccur;
    node->text = p->data;
    node->text_size = 1;
    ++p->data;
    return node;
  }
  return nullptr;
}

AstNode* HandleMemberKey1(Parser* p) {
  Parser plocal = *p;
  if (!HandleType1(&plocal)) {
    return nullptr;
  }
  if (!HandleSpace(&plocal)) {
    return nullptr;
  }
  if (*plocal.data++ != '=' || *plocal.data++ != '>') {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = nullptr;
  node->sibling = nullptr;
  node->type = AstNode::Type::kMemberKey;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

AstNode* HandleMemberKey2(Parser* p) {
  Parser plocal = *p;
  AstNode* id = HandleId(&plocal);
  if (!id) {
    return nullptr;
  }
  if (!HandleSpace(&plocal)) {
    return nullptr;
  }
  if (*plocal.data++ != ':') {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = id;
  node->sibling = nullptr;
  node->type = AstNode::Type::kMemberKey;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

AstNode* HandleMemberKey3(Parser* p) {
  Parser plocal = *p;
  AstNode* value = HandleValue(&plocal);
  if (!value) {
    return nullptr;
  }
  if (!HandleSpace(&plocal)) {
    return nullptr;
  }
  if (*plocal.data++ != ':') {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = value;
  node->sibling = nullptr;
  node->type = AstNode::Type::kMemberKey;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

AstNode* HandleMemberKey(Parser* p) {
  AstNode* node = HandleMemberKey1(p);
  if (!node) {
    node = HandleMemberKey2(p);
  }
  if (!node) {
    node = HandleMemberKey3(p);
  }
  return node;
}

AstNode* HandleGroupEntry(Parser* p);

bool HandleOptcom(Parser* p) {
  if (!HandleSpace(p)) {
    return false;
  }
  if (*p->data == ',') {
    ++p->data;
    if (!HandleSpace(p)) {
      return false;
    }
  }
  return true;
}

AstNode* HandleGroupChoice(Parser* p) {
  g_nodes.emplace_back(new AstNode);
  AstNode* tail = nullptr;
  AstNode* group_node = g_nodes.back().get();
  group_node->children = nullptr;
  group_node->sibling = nullptr;
  group_node->type = AstNode::Type::kGrpchoice;
  group_node->text = p->data;
  while (true) {
    char* orig = p->data;
    AstNode* group_entry = HandleGroupEntry(p);
    if (!group_entry) {
      p->data = orig;
      if (!group_node->children) {
        g_nodes.pop_back();
        return nullptr;
      }
      group_node->text_size = p->data - group_node->text;
      return group_node;
    }
    if (!group_node->children) {
      group_node->children = group_entry;
    }
    if (tail) {
      tail->sibling = group_entry;
    }
    tail = group_entry;
    if (!HandleOptcom(p)) {
      return nullptr;
    }
  }
  return nullptr;
}

AstNode* HandleGroup(Parser* p) {
  char* orig = p->data;
  AstNode* group_choice = HandleGroupChoice(p);
  if (!group_choice) {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* group = g_nodes.back().get();
  group->children = group_choice;
  group->sibling = nullptr;
  group->type = AstNode::Type::kGroup;
  group->text = orig;
  group->text_size = p->data - orig;
  return group;
}

AstNode* HandleType2(Parser* p) {
  char* orig = p->data;
  char* it = p->data;
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = nullptr;
  node->sibling = nullptr;
  node->type = AstNode::Type::kType2;
  node->text = orig;
  if (*it == '-' || IsDigit(*it) ||               // FIRST(number)
      *it == '"' ||                               // FIRST(text)
      *it == '\'' || *it == 'h' || *it == 'b') {  // FIRST(bytes)
    // TODO(btolsch): disambiguate 'h' and 'b' with typename
    AstNode* value = HandleValue(p);
    if (!value) {
      return nullptr;
    }
    node->children = value;
  } else if (IsExtendedAlpha(*it)) {
    AstNode* id = HandleId(p);
    if (!id) {
      return nullptr;
    }
    if (*p->data == '<') {
      std::cerr << "It looks like you're trying to use a generic argument, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
    id->type = AstNode::Type::kTypename;
    node->children = id;
  } else if (*it == '(') {
    p->data = it + 1;
    if (!HandleSpace(p)) {
      return nullptr;
    }
    AstNode* type = HandleType(p);
    if (!type) {
      return nullptr;
    }
    if (!HandleSpace(p)) {
      return nullptr;
    }
    if (*p->data++ != ')') {
      return nullptr;
    }
    node->children = type;
  } else if (*it == '{') {
    p->data = it + 1;
    if (!HandleSpace(p)) {
      return nullptr;
    }
    AstNode* group = HandleGroup(p);
    if (!group) {
      return nullptr;
    }
    if (!HandleSpace(p)) {
      return nullptr;
    }
    if (*p->data++ != '}') {
      return nullptr;
    }
    node->children = group;
  } else if (*it == '[') {
    p->data = it + 1;
    if (!HandleSpace(p)) {
      return nullptr;
    }
    AstNode* group = HandleGroup(p);
    if (!group) {
      return nullptr;
    }
    if (!HandleSpace(p)) {
      return nullptr;
    }
    if (*p->data++ != ']') {
      return nullptr;
    }
    node->children = group;
  } else if (*it == '~') {
    p->data = it + 1;
    if (!HandleSpace(p)) {
      return nullptr;
    }
    if (!HandleId(p)) {
      return nullptr;
    }
    if (*p->data == '<') {
      std::cerr << "It looks like you're trying to use a generic argument, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
  } else if (*it == '&') {
    p->data = it + 1;
    if (!HandleSpace(p)) {
      return nullptr;
    }
    if (*p->data == '(') {
      ++p->data;
      if (!HandleSpace(p)) {
        return nullptr;
      }
      AstNode* group = HandleGroup(p);
      if (!group) {
        return nullptr;
      }
      if (!HandleSpace(p)) {
        return nullptr;
      }
      if (*p->data != ')') {
        return nullptr;
      }
      ++p->data;
      node->children = group;
    } else {
      AstNode* id = HandleId(p);
      if (id) {
        if (*p->data == '<') {
          std::cerr << "It looks like you're trying to use a generic argument, "
                       "which we don't support"
                    << std::endl;
          return nullptr;
        }
        node->children = id;
      } else {
        return nullptr;
      }
    }
  } else if (*it == '#') {
    ++it;
    if (*it == '6') {
      ++it;
      if (*it == '.') {
        p->data = it + 1;
        if (!HandleUint(p)) {
          return nullptr;
        }
        it = p->data;
      }
      if (*it++ != '(') {
        return nullptr;
      }
      p->data = it;
      if (!HandleSpace(p)) {
        return nullptr;
      }
      AstNode* type = HandleType(p);
      if (!type) {
        return nullptr;
      }
      if (!HandleSpace(p)) {
        return nullptr;
      }
      if (*p->data++ != ')') {
        return nullptr;
      }
      node->children = type;
    } else if (IsDigit(*it)) {
      std::cerr << "# MAJOR unimplemented" << std::endl;
      return nullptr;
    } else {
      p->data = it;
    }
  } else {
    return nullptr;
  }
  node->text_size = p->data - orig;
  return node;
}

AstNode* HandleType1(Parser* p) {
  char* orig = p->data;
  AstNode* type2 = HandleType2(p);
  if (!type2) {
    return nullptr;
  }
  // TODO: rangeop + ctlop
  // if (!HandleSpace(p)) {
  //   return false;
  // }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = type2;
  node->sibling = nullptr;
  node->type = AstNode::Type::kType1;
  node->text = orig;
  node->text_size = p->data - orig;
  return node;
}

AstNode* HandleType(Parser* p) {
  Parser plocal = *p;
  AstNode* type1 = HandleType1(&plocal);
  if (!type1) {
    return nullptr;
  }
  if (!HandleSpace(&plocal)) {
    return nullptr;
  }
  AstNode* tail = type1;
  while (*plocal.data == '/') {
    ++plocal.data;
    if (!HandleSpace(&plocal)) {
      return nullptr;
    }
    AstNode* next_type1 = HandleType1(&plocal);
    if (!next_type1) {
      return nullptr;
    }
    tail->sibling = next_type1;
    tail = next_type1;
    if (!HandleSpace(&plocal)) {
      return nullptr;
    }
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = type1;
  node->sibling = nullptr;
  node->type = AstNode::Type::kType;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

AstNode* HandleId(Parser* p) {
  char* id = p->data;
  if (!IsExtendedAlpha(*id)) {
    return nullptr;
  }
  char* it = id + 1;
  while (true) {
    if (*it == '-' || *it == '.') {
      ++it;
      if (!IsExtendedAlpha(*it) && !IsDigit(*it)) {
        return nullptr;
      }
      ++it;
    } else if (IsExtendedAlpha(*it) || IsDigit(*it)) {
      ++it;
    } else {
      break;
    }
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = nullptr;
  node->sibling = nullptr;
  node->type = AstNode::Type::kId;
  node->text = p->data;
  node->text_size = it - p->data;
  p->data = it;
  return node;
}

AstNode* HandleGroupEntry1(Parser* p) {
  Parser plocal = *p;
  AstNode* occur = HandleOccur(&plocal);
  if (occur) {
    if (!HandleSpace(&plocal)) {
      return nullptr;
    }
  }
  AstNode* member_key = HandleMemberKey(&plocal);
  if (member_key) {
    if (!HandleSpace(&plocal)) {
      return nullptr;
    }
  }
  AstNode* type = HandleType(&plocal);
  if (!type) {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  if (occur) {
    node->children = occur;
    if (member_key) {
      occur->sibling = member_key;
      member_key->sibling = type;
    } else {
      occur->sibling = type;
    }
  } else if (member_key) {
    node->children = member_key;
    member_key->sibling = type;
  } else {
    node->children = type;
  }
  node->sibling = nullptr;
  node->type = AstNode::Type::kGrpent;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

// NOTE: This should probably never be hit, why is it in the grammar?
AstNode* HandleGroupEntry2(Parser* p) {
  Parser plocal = *p;
  AstNode* occur = HandleOccur(&plocal);
  if (occur) {
    if (!HandleSpace(&plocal)) {
      return nullptr;
    }
  }
  AstNode* id = HandleId(&plocal);
  if (!id) {
    return nullptr;
  }
  id->type = AstNode::Type::kGroupname;
  if (*plocal.data == '<') {
    std::cerr << "It looks like you're trying to use a generic argument, "
                 "which we don't support"
              << std::endl;
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  if (occur) {
    occur->sibling = id;
    node->children = occur;
  } else {
    node->children = id;
  }
  node->sibling = nullptr;
  node->type = AstNode::Type::kGrpent;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

AstNode* HandleGroupEntry3(Parser* p) {
  Parser plocal = *p;
  AstNode* occur = HandleOccur(&plocal);
  if (occur) {
    if (!HandleSpace(&plocal)) {
      return nullptr;
    }
  }
  if (*plocal.data != '(') {
    return nullptr;
  }
  ++plocal.data;
  if (!HandleSpace(&plocal)) {
    return nullptr;
  }
  AstNode* group = HandleGroup(&plocal);
  if (!group) {
    return nullptr;
  }
  if (!HandleSpace(&plocal)) {
    return nullptr;
  }
  if (*plocal.data != ')') {
    return nullptr;
  }
  ++plocal.data;
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  if (occur) {
    node->children = occur;
    occur->sibling = group;
  } else {
    node->children = group;
  }
  node->sibling = nullptr;
  node->type = AstNode::Type::kGrpent;
  node->text = p->data;
  node->text_size = plocal.data - p->data;
  *p = plocal;
  return node;
}

AstNode* HandleGroupEntry(Parser* p) {
  AstNode* node = HandleGroupEntry1(p);
  if (!node) {
    node = HandleGroupEntry2(p);
  }
  if (!node) {
    node = HandleGroupEntry3(p);
  }
  return node;
}

AstNode* HandleRule(Parser* p) {
  char* start = p->data;
  AstNode* id = HandleId(p);
  if (!id) {
    return nullptr;
  }
  if (*p->data == '<') {
    std::cerr << "It looks like you're trying to use a generic parameter, "
                 "which we don't support"
              << std::endl;
    return nullptr;
  }
  if (!HandleSpace(p)) {
    return nullptr;
  }
  char* assign_start = p->data;
  AssignType assign_type = HandleAssign(p);
  if (assign_type == AssignType::kInvalid) {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* assign_node = g_nodes.back().get();
  assign_node->children = nullptr;
  assign_node->sibling = nullptr;
  assign_node->type = (assign_type == AssignType::kAssign)
                          ? AstNode::Type::kAssign
                          : (assign_type == AssignType::kAssignT)
                                ? AstNode::Type::kAssignT
                                : AstNode::Type::kAssignG;
  assign_node->text = assign_start;
  assign_node->text_size = p->data - assign_start;
  id->sibling = assign_node;

  if (!HandleSpace(p)) {
    return nullptr;
  }
  AstNode* type = HandleType(p);
  id->type = AstNode::Type::kTypename;
  if (!type) {
    type = HandleGroupEntry(p);
    id->type = AstNode::Type::kGroupname;
    if (!type) {
      return nullptr;
    }
  }
  assign_node->sibling = type;
  if (!HandleSpace(p)) {
    return nullptr;
  }
  g_nodes.emplace_back(new AstNode);
  AstNode* node = g_nodes.back().get();
  node->children = id;
  node->sibling = nullptr;
  node->type = AstNode::Type::kRule;
  node->text = start;
  node->text_size = p->data - start;
  return node;
}

AstNode* HandleCddl(Parser* p) {
  if (*p->data == 0) {
    return nullptr;
  }
  if (!HandleSpace(p)) {
    return nullptr;
  }
  AstNode* root = nullptr;
  AstNode* tail = nullptr;
  do {
    AstNode* next = HandleRule(p);
    if (!next) {
      return nullptr;
    }
    if (!root) {
      root = next;
    }
    if (tail) {
      tail->sibling = next;
    }
    tail = next;
    if (!HandleSpace(p)) {
      return nullptr;
    }
  } while (*p->data);
  return root;
}

void PrintCollapsed(int size, char* text) {
  for (int i = 0; i < size; ++i, ++text) {
    if (*text == ' ' || *text == '\n') {
      write(STDOUT_FILENO, " ", 1);
      while (i < size && (*text == ' ' || *text == '\n')) {
        ++i;
        ++text;
      }
    }
    if (i < size) {
      write(STDOUT_FILENO, text, 1);
    }
  }
  write(STDOUT_FILENO, "\n", 1);
}

void DumpAst(AstNode* node, int indent_level) {
  while (node) {
    for (int i = 0; i <= indent_level; ++i) {
      write(STDOUT_FILENO, "--", 2);
    }
    switch (node->type) {
      case AstNode::Type::kRule:
        printf("kRule: ");
        fflush(stdout);
        break;
      case AstNode::Type::kTypename:
        printf("kTypename: ");
        fflush(stdout);
        break;
      case AstNode::Type::kGroupname:
        printf("kGroupname: ");
        fflush(stdout);
        break;
      case AstNode::Type::kAssign:
        printf("kAssign: ");
        fflush(stdout);
        break;
      case AstNode::Type::kAssignT:
        printf("kAssignT: ");
        fflush(stdout);
        break;
      case AstNode::Type::kAssignG:
        printf("kAssignG: ");
        fflush(stdout);
        break;
      case AstNode::Type::kType:
        printf("kType: ");
        fflush(stdout);
        break;
      case AstNode::Type::kGrpent:
        printf("kGrpent: ");
        fflush(stdout);
        break;
      case AstNode::Type::kType1:
        printf("kType1: ");
        fflush(stdout);
        break;
      case AstNode::Type::kType2:
        printf("kType2: ");
        fflush(stdout);
        break;
      case AstNode::Type::kValue:
        printf("kValue: ");
        fflush(stdout);
        break;
      case AstNode::Type::kGroup:
        printf("kGroup: ");
        fflush(stdout);
        break;
      case AstNode::Type::kUint:
        printf("kUint: ");
        fflush(stdout);
        break;
      case AstNode::Type::kDigit:
        printf("kDigit: ");
        fflush(stdout);
        break;
      case AstNode::Type::kRangeop:
        printf("kRangeop: ");
        fflush(stdout);
        break;
      case AstNode::Type::kCtlop:
        printf("kCtlop: ");
        fflush(stdout);
        break;
      case AstNode::Type::kGrpchoice:
        printf("kGrpchoice: ");
        fflush(stdout);
        break;
      case AstNode::Type::kOccur:
        printf("kOccur: ");
        fflush(stdout);
        break;
      case AstNode::Type::kMemberKey:
        printf("kMemberKey: ");
        fflush(stdout);
        break;
      case AstNode::Type::kId:
        printf("kId: ");
        fflush(stdout);
        break;
      case AstNode::Type::kNumber:
        printf("kNumber: ");
        fflush(stdout);
        break;
      case AstNode::Type::kText:
        printf("kText: ");
        fflush(stdout);
        break;
      case AstNode::Type::kBytes:
        printf("kBytes: ");
        fflush(stdout);
        break;
      case AstNode::Type::kOther:
        printf("kOther: ");
        fflush(stdout);
        break;
    }
    PrintCollapsed(static_cast<int>(node->text_size), node->text);
    DumpAst(node->children, indent_level + 1);
    node = node->sibling;
  }
}
