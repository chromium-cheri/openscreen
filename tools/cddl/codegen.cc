// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/codegen.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

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
  } else if (cpp_type->which == CppType::Which::kStruct) {
    return ToCTypename(cpp_type->name);
  } else if (cpp_type->which == CppType::Which::kTaggedType) {
    return CppTypeToString(cpp_type->tagged_type.real_type);
  }
  return std::string();
}

bool DumpStructMembers(
    int fd,
    const std::vector<std::pair<std::string, CppType*>>& members) {
  for (const auto& x : members) {
    std::string type_string;
    if (x.second->which == CppType::Which::kStruct) {
      if (x.second->struct_type.key_type ==
          CppType::Struct::KeyType::kPlainGroup) {
        if (!DumpStructMembers(fd, x.second->struct_type.members)) {
          return false;
        }
        continue;
      } else {
        type_string = ToCTypename(x.first);
      }
    } else if (x.second->which == CppType::Which::kOptional) {
      dprintf(fd, "  bool has_%s;\n", ToCId(x.first).c_str());
      type_string = CppTypeToString(x.second->optional_type);
    } else if (x.second->which == CppType::Which::kDiscriminatedUnion) {
      // TODO(btolsch): Ctor/dtor when there's a d-union and bool/uninitialized
      // state when no uint present for this purpose.
      std::string cid = ToCId(x.first);
      dprintf(fd, "  enum class Which%s {\n",
              ToCTypename(x.second->name).c_str());
      for (auto* union_member : x.second->discriminated_union.members) {
        switch (union_member->which) {
          case CppType::Which::kUint64:
            dprintf(fd, "    kUint64,\n");
            break;
          case CppType::Which::kString:
            dprintf(fd, "    kString,\n");
            break;
          case CppType::Which::kBytes:
            dprintf(fd, "    kBytes,\n");
            break;
          default:
            return false;
        }
      }
      dprintf(fd, "  } which_%s;\n", cid.c_str());
      dprintf(fd, "  union {\n");
      for (auto* union_member : x.second->discriminated_union.members) {
        switch (union_member->which) {
          case CppType::Which::kUint64:
            dprintf(fd, "    uint64_t uint;\n");
            break;
          case CppType::Which::kString:
            dprintf(fd, "    std::string str;\n");
            break;
          case CppType::Which::kBytes:
            dprintf(fd, "    std::vector<uint8_t> bytes;\n");
            break;
          default:
            return false;
        }
      }
      dprintf(fd, "  } %s;\n", cid.c_str());
      return true;
    } else {
      type_string = CppTypeToString(x.second);
    }
    if (type_string.empty()) {
      return false;
    }
    dprintf(fd, "  %s %s;\n", type_string.c_str(), ToCId(x.first).c_str());
  }
  return true;
}

bool DumpDef(int fd, CppType* type) {
  switch (type->which) {
    case CppType::Which::kEnum: {
      dprintf(fd, "\nenum %s : uint64_t {\n", ToCTypename(type->name).c_str());
      for (const auto& x : type->enum_type.members) {
        dprintf(fd, "  k%s = %luull,\n", ToCTypename(x.first).c_str(),
                x.second);
      }
      dprintf(fd, "};\n");
    } break;
    case CppType::Which::kStruct: {
      dprintf(fd, "\nstruct %s {\n", ToCTypename(type->name).c_str());
      if (!DumpStructMembers(fd, type->struct_type.members)) {
        return false;
      }
      dprintf(fd, "};\n");
    } break;
    default:
      break;
  }
  return true;
}

bool EnsureDepsOutput(int fd, CppType* cpp_type, std::set<std::string>* defs) {
  switch (cpp_type->which) {
    case CppType::Which::kVector: {
      return EnsureDepsOutput(fd, cpp_type->vector_type.element_type, defs);
    } break;
    case CppType::Which::kEnum: {
      if (defs->find(cpp_type->name) != defs->end()) {
        return true;
      }
      for (auto* x : cpp_type->enum_type.sub_members) {
        if (!EnsureDepsOutput(fd, x, defs)) {
          return false;
        }
      }
      defs->emplace(cpp_type->name);
      DumpDef(fd, cpp_type);
    } break;
    case CppType::Which::kStruct: {
      if (cpp_type->struct_type.key_type !=
          CppType::Struct::KeyType::kPlainGroup) {
        if (defs->find(cpp_type->name) != defs->end()) {
          return true;
        }
        for (auto& x : cpp_type->struct_type.members) {
          if (!EnsureDepsOutput(fd, x.second, defs)) {
            return false;
          }
        }
        defs->emplace(cpp_type->name);
        DumpDef(fd, cpp_type);
      }
    } break;
    case CppType::Which::kOptional: {
      return EnsureDepsOutput(fd, cpp_type->optional_type, defs);
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      for (auto* x : cpp_type->discriminated_union.members) {
        if (!EnsureDepsOutput(fd, x, defs)) {
          return false;
        }
      }
    } break;
    case CppType::Which::kTaggedType: {
      if (!EnsureDepsOutput(fd, cpp_type->tagged_type.real_type, defs)) {
        return false;
      }
    } break;
    default:
      break;
  }
  return true;
}

bool DumpDefs(int fd, CppSymbolTable* table) {
  std::set<std::string> defs;
  for (auto& entry : table->cpp_type_map) {
    auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    if (!EnsureDepsOutput(fd, type, &defs)) {
      return false;
    }
  }
  for (const auto& entry : table->cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCTypename(name);
    dprintf(fd, "\nssize_t Encode%s(\n", cpp_name.c_str());
    dprintf(fd, "    const %s& data,\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length);\n");
    dprintf(fd, "ssize_t Decode%s(\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length,\n");
    dprintf(fd, "    %s* data);\n", cpp_name.c_str());
  }
  return true;
}

bool DumpMapEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth = 1);
bool DumpArrayEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth = 1);

bool DumpEncoder(int fd,
                 const std::string& name,
                 CppType* cpp_type,
                 const std::string& nested_type_scope,
                 int encoder_depth) {
  switch (cpp_type->which) {
    case CppType::Which::kStruct:
      if (cpp_type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
        if (!DumpMapEncoder(fd, name, cpp_type->struct_type.members,
                            cpp_type->name, encoder_depth)) {
          return false;
        }
        return true;
      } else if (cpp_type->struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        if (!DumpArrayEncoder(fd, name, cpp_type->struct_type.members,
                              cpp_type->name, encoder_depth)) {
          return false;
        }
        return true;
      } else {
        for (const auto& x : cpp_type->struct_type.members) {
          dprintf(fd,
                  "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder%d, "
                  "\"%s\", sizeof(\"%s\") - 1));\n",
                  encoder_depth, x.first.c_str(), x.first.c_str());
          if (!DumpEncoder(fd, name + std::string(".") + ToCId(x.first),
                           x.second, nested_type_scope, encoder_depth)) {
            return false;
          }
        }
        return true;
      }
      break;
    case CppType::Which::kUint64:
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder%d, %s));\n",
              encoder_depth, ToCId(name).c_str());
      return true;
      break;
    case CppType::Which::kString: {
      std::string cid = ToCId(name);
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(IsValidUtf8(%s));\n", cid.c_str());
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder%d, "
              "%s.c_str(), %s.size()));\n",
              encoder_depth, cid.c_str(), cid.c_str());
      return true;
    } break;
    case CppType::Which::kBytes: {
      std::string cid = ToCId(name);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_byte_string(&encoder%d, "
              "%s.data(), "
              "%s.size()));\n",
              encoder_depth, cid.c_str(), cid.c_str());
      return true;
    } break;
    case CppType::Which::kVector: {
      std::string cid = ToCId(name);
      dprintf(fd, "  CborEncoder encoder%d;\n", encoder_depth + 1);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder%d, "
              "&encoder%d, %s.size()));\n",
              encoder_depth, encoder_depth + 1, cid.c_str());
      dprintf(fd, "  for (const auto& x : %s) {\n", cid.c_str());
      if (!DumpEncoder(fd, "x", cpp_type->vector_type.element_type,
                       nested_type_scope, encoder_depth + 1)) {
        return false;
      }
      dprintf(fd, "  }\n");
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
              "&encoder%d));\n",
              encoder_depth, encoder_depth + 1);
      return true;
    } break;
    case CppType::Which::kEnum: {
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder%d, %s));\n",
              encoder_depth, ToCId(name).c_str());
      return true;
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      for (auto* union_member : cpp_type->discriminated_union.members) {
        switch (union_member->which) {
          case CppType::Which::kUint64:
            dprintf(fd, "  case %s::Which%s::kUint64:\n",
                    ToCTypename(nested_type_scope).c_str(),
                    ToCTypename(union_member->name).c_str());
            if (!DumpEncoder(fd, ToCId(name + std::string(".uint")),
                             union_member, nested_type_scope, encoder_depth)) {
              return false;
            }
            dprintf(fd, "    break;\n");
            break;
          case CppType::Which::kString:
            dprintf(fd, "  case %s::Which%s::kString:\n",
                    ToCTypename(nested_type_scope).c_str(),
                    ToCTypename(union_member->name).c_str());
            if (!DumpEncoder(fd, ToCId(name + std::string(".str")),
                             union_member, nested_type_scope, encoder_depth)) {
              return false;
            }
            dprintf(fd, "    break;\n");
            break;
          case CppType::Which::kBytes:
            dprintf(fd, "  case %s::Which%s::kBytes:\n",
                    ToCTypename(nested_type_scope).c_str(),
                    ToCTypename(union_member->name).c_str());
            if (!DumpEncoder(fd, ToCId(name + std::string(".bytes")),
                             union_member, nested_type_scope, encoder_depth)) {
              return false;
            }
            dprintf(fd, "    break;\n");
            break;
          default:
            return false;
        }
      }
      return true;
    } break;
    case CppType::Which::kTaggedType: {
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_tag(&encoder%d, %luull));\n",
              encoder_depth, cpp_type->tagged_type.tag);
      if (!DumpEncoder(fd, name, cpp_type->tagged_type.real_type,
                       nested_type_scope, encoder_depth)) {
        return false;
      }
      return true;
    } break;
    default:
      break;
  }
  return false;
}

bool DumpMapEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth) {
  dprintf(fd, "  CborEncoder encoder%d;\n", encoder_depth);
  dprintf(
      fd,
      "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder%d, &encoder%d, "
      "%d));\n",
      encoder_depth - 1, encoder_depth, static_cast<int>(members.size()));

  for (const auto& x : members) {
    std::string fullname = name;
    CppType* member_type = x.second;
    if (x.second->which != CppType::Which::kStruct ||
        x.second->struct_type.key_type !=
            CppType::Struct::KeyType::kPlainGroup) {
      if (x.second->which == CppType::Which::kOptional) {
        member_type = x.second->optional_type;
        dprintf(fd, "  if (%s.has_%s) {\n", ToCId(name).c_str(),
                ToCId(x.first).c_str());
      }
      dprintf(
          fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder%d, \"%s\", "
          "sizeof(\"%s\") - 1));\n",
          encoder_depth, x.first.c_str(), x.first.c_str());
      if (x.second->which == CppType::Which::kDiscriminatedUnion) {
        dprintf(fd, "  switch (%s.which_%s) {\n", fullname.c_str(),
                x.first.c_str());
      }
      fullname = fullname + std::string(".") + x.first;
    }
    if (!DumpEncoder(fd, fullname, member_type, nested_type_scope,
                     encoder_depth)) {
      return false;
    }
    if (x.second->which == CppType::Which::kOptional ||
        x.second->which == CppType::Which::kDiscriminatedUnion) {
      dprintf(fd, "  }\n");
    }
  }

  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
          "&encoder%d));\n",
          encoder_depth - 1, encoder_depth);
  return true;
}

bool DumpArrayEncoder(
    int fd,
    const std::string& name,
    const std::vector<std::pair<std::string, CppType*>>& members,
    const std::string& nested_type_scope,
    int encoder_depth) {
  dprintf(fd, "  CborEncoder encoder%d;\n", encoder_depth);
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder%d, "
          "&encoder%d, %d));\n",
          encoder_depth - 1, encoder_depth, static_cast<int>(members.size()));

  for (const auto& x : members) {
    std::string fullname = name;
    CppType* member_type = x.second;
    if (x.second->which != CppType::Which::kStruct ||
        x.second->struct_type.key_type !=
            CppType::Struct::KeyType::kPlainGroup) {
      if (x.second->which == CppType::Which::kOptional) {
        member_type = x.second->optional_type;
        dprintf(fd, "  if (%s.has_%s) {\n", ToCId(name).c_str(),
                ToCId(x.first).c_str());
      }
      if (x.second->which == CppType::Which::kDiscriminatedUnion) {
        dprintf(fd, "  switch (%s.which_%s) {\n", fullname.c_str(),
                x.first.c_str());
      }
      fullname = fullname + std::string(".") + x.first;
    }
    if (!DumpEncoder(fd, fullname, member_type, nested_type_scope,
                     encoder_depth)) {
      return false;
    }
    if (x.second->which == CppType::Which::kOptional ||
        x.second->which == CppType::Which::kDiscriminatedUnion) {
      dprintf(fd, "  }\n");
    }
  }

  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder%d, "
          "&encoder%d));\n",
          encoder_depth - 1, encoder_depth);
  return true;
}

bool DumpEncoders(int fd, CppSymbolTable* table) {
  for (const auto& entry : table->cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCTypename(name);
    dprintf(fd, "\nssize_t Encode%s(\n", cpp_name.c_str());
    dprintf(fd, "    const %s& data,\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length) {\n");
    dprintf(fd, "  CborEncoder encoder0;\n");
    dprintf(fd, "  cbor_encoder_init(&encoder0, buffer, length, 0);\n");

    if (type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!DumpMapEncoder(fd, "data", type->struct_type.members, type->name)) {
        return false;
      }
    } else {
      if (!DumpArrayEncoder(fd, "data", type->struct_type.members,
                            type->name)) {
        return false;
      }
    }

    dprintf(fd,
            "  size_t extra_bytes_needed = "
            "cbor_encoder_get_extra_bytes_needed(&encoder0);\n");
    dprintf(fd, "  if (extra_bytes_needed) {\n");
    dprintf(fd,
            "    return static_cast<ssize_t>(length + extra_bytes_needed);\n");
    dprintf(fd, "  } else {\n");
    dprintf(fd,
            "    return "
            "static_cast<ssize_t>(cbor_encoder_get_buffer_size(&encoder0, "
            "buffer));\n");
    dprintf(fd, "  }\n");
    dprintf(fd, "}\n");
  }
  return true;
}

bool DumpMapDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count);
bool DumpArrayDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count);

bool DumpDecoder(int fd,
                 const std::string& name,
                 const std::string& member_accessor,
                 CppType* cpp_type,
                 int decoder_depth,
                 int* temporary_count) {
  switch (cpp_type->which) {
    case CppType::Which::kUint64: {
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&it%d, &%s));\n",
              decoder_depth, name.c_str());
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it%d));\n",
              decoder_depth);
      return true;
    } break;
    case CppType::Which::kString: {
      int temp_length = (*temporary_count)++;
      dprintf(fd, "  size_t length%d = 0;", temp_length);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_validate(&it%d, "
              "CborValidateUtf8));\n",
              decoder_depth);
      dprintf(fd, "  if (cbor_value_is_length_known(&it%d)) {\n",
              decoder_depth);
      dprintf(fd,
              "    CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&it%d, "
              "&length%d));\n",
              decoder_depth, temp_length);
      dprintf(fd, "  } else {\n");
      dprintf(
          fd,
          "    CBOR_RETURN_ON_ERROR(cbor_value_calculate_string_length(&it%d, "
          "&length%d));\n",
          decoder_depth, temp_length);
      dprintf(fd, "  }\n");
      dprintf(fd, "  %s%sresize(length%d);\n", name.c_str(),
              member_accessor.c_str(), temp_length);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_copy_text_string(&it%d, "
              "const_cast<char*>(%s%sdata()), &length%d, nullptr));\n",
              decoder_depth, name.c_str(), member_accessor.c_str(),
              temp_length);
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance(&it%d));\n",
              decoder_depth);
      return true;
    } break;
    case CppType::Which::kBytes: {
      int temp_length = (*temporary_count)++;
      dprintf(fd, "  size_t length%d = 0;", temp_length);
      dprintf(fd, "  if (cbor_value_is_length_known(&it%d)) {\n",
              decoder_depth);
      dprintf(fd,
              "    CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&it%d, "
              "&length%d));\n",
              decoder_depth, temp_length);
      dprintf(fd, "  } else {\n");
      dprintf(
          fd,
          "    CBOR_RETURN_ON_ERROR(cbor_value_calculate_string_length(&it%d, "
          "&length%d));\n",
          decoder_depth, temp_length);
      dprintf(fd, "  }\n");
      dprintf(fd, "  %s%sresize(length%d);\n", name.c_str(),
              member_accessor.c_str(), temp_length);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_copy_byte_string(&it%d, "
              "const_cast<uint8_t*>(%s%sdata()), &length%d, nullptr));\n",
              decoder_depth, name.c_str(), member_accessor.c_str(),
              temp_length);
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance(&it%d));\n",
              decoder_depth);
      return true;
    } break;
    case CppType::Which::kVector: {
      dprintf(fd, "  if (cbor_value_get_type(&it%d) != CborArrayType) {\n",
              decoder_depth);
      dprintf(fd, "    return -1;\n");
      dprintf(fd, "  }\n");
      dprintf(fd, "  CborValue it%d;\n", decoder_depth + 1);
      dprintf(fd, "  size_t it%d_length = 0;\n", decoder_depth + 1);
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&it%d, "
              "&it%d_length));\n",
              decoder_depth, decoder_depth + 1);
      dprintf(fd, "  %s%sresize(it%d_length);\n", name.c_str(),
              member_accessor.c_str(), decoder_depth + 1);
      dprintf(
          fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it%d, &it%d));\n",
          decoder_depth, decoder_depth + 1);
      dprintf(fd, "  for (auto i = %s%sbegin(); i != %s%send(); ++i) {\n",
              name.c_str(), member_accessor.c_str(), name.c_str(),
              member_accessor.c_str());
      if (!DumpDecoder(fd, "i", "->", cpp_type->vector_type.element_type,
                       decoder_depth + 1, temporary_count)) {
        return false;
      }
      dprintf(fd, "  }\n");
      dprintf(
          fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it%d, &it%d));\n",
          decoder_depth, decoder_depth + 1);
      return true;
    } break;
    case CppType::Which::kEnum: {
      dprintf(fd,
              "  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&it%d, "
              "reinterpret_cast<uint64_t*>(&%s)));\n",
              decoder_depth, name.c_str());
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it%d));\n",
              decoder_depth);
      // TODO(btolsch): Validate against enum members.
      return true;
    } break;
    case CppType::Which::kStruct: {
      if (cpp_type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
        return DumpMapDecoder(fd, name, member_accessor,
                              cpp_type->struct_type.members, decoder_depth + 1,
                              temporary_count);
      } else if (cpp_type->struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        return DumpArrayDecoder(fd, name, member_accessor,
                                cpp_type->struct_type.members,
                                decoder_depth + 1, temporary_count);
      }
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      int temp_value_type = (*temporary_count)++;
      dprintf(fd, "  CborType type%d = cbor_value_get_type(&it%d);\n",
              temp_value_type, decoder_depth);
      bool first = true;
      for (auto* x : cpp_type->discriminated_union.members) {
        if (first) {
          first = false;
        } else {
          dprintf(fd, " else ");
        }
        switch (x->which) {
          case CppType::Which::kUint64:
            dprintf(fd,
                    "  if (type%d == CborIntegerType && (it%d.flags & "
                    "CborIteratorFlag_NegativeInteger) == 0) {\n",
                    temp_value_type, decoder_depth);
            // TODO(btolsch): assign which_*.
            if (!DumpDecoder(fd, name + std::string(".uint"), ".", x,
                             decoder_depth, temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kString:
            dprintf(fd, "  if (type%d == CborTextStringType) {\n",
                    temp_value_type);
            if (!DumpDecoder(fd, name + std::string(".str"), ".", x,
                             decoder_depth, temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kBytes:
            dprintf(fd, "  if (type%d == CborByteStringType) {\n",
                    temp_value_type);
            if (!DumpDecoder(fd, name + std::string(".bytes"), ".", x,
                             decoder_depth, temporary_count)) {
              return false;
            }
            break;
          default:
            return false;
        }
        dprintf(fd, "  }\n");
      }
      dprintf(fd, " else { return -1; }\n");
      return true;
    } break;
    case CppType::Which::kTaggedType: {
      int temp_tag = (*temporary_count)++;
      dprintf(fd, "  uint64_t tag%d = 0;\n", temp_tag);
      dprintf(fd, "  cbor_value_get_tag(&it%d, &tag%d);\n", decoder_depth,
              temp_tag);
      dprintf(fd, "  if (tag%d != %luull) {\n", temp_tag,
              cpp_type->tagged_type.tag);
      dprintf(fd, "    return -1;\n");
      dprintf(fd, "  }\n");
      dprintf(fd, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it%d));\n",
              decoder_depth);
      if (!DumpDecoder(fd, name, member_accessor,
                       cpp_type->tagged_type.real_type, decoder_depth,
                       temporary_count)) {
        return false;
      }
      return true;
    } break;
    default:
      break;
  }
  return false;
}

bool DumpMapDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count) {
  dprintf(fd, "  if (cbor_value_get_type(&it%d) != CborMapType) {\n",
          decoder_depth - 1);
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd, "  CborValue it%d;\n", decoder_depth);
  dprintf(fd, "  size_t it%d_length = 0;\n", decoder_depth);
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_get_map_length(&it%d, "
          "&it%d_length));\n",
          decoder_depth - 1, decoder_depth);
  // TODO(btolsch): Account for optional combinations.
  dprintf(fd, "  if (it%d_length != %d) {\n", decoder_depth,
          static_cast<int>(members.size()));
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  int member_pos = 0;
  for (auto& x : members) {
    std::string cid = ToCId(x.first);
    std::string fullname = name + std::string("->") + cid;
    dprintf(fd, "  CBOR_RETURN_ON_ERROR(EXPECT_KEY_CONSTANT(&it%d, \"%s\"));\n",
            decoder_depth, x.first.c_str());
    if (x.second->which == CppType::Which::kOptional) {
      dprintf(fd, "  if (it%d_length > %d) {\n", decoder_depth, member_pos);
      dprintf(fd, "    %s%shas_%s = true;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      if (!DumpDecoder(fd, fullname, ".", x.second->optional_type,
                       decoder_depth, temporary_count)) {
        return false;
      }
      dprintf(fd, "  } else {\n");
      dprintf(fd, "    %s%shas_%s = false;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      dprintf(fd, "  }\n");
    } else {
      if (!DumpDecoder(fd, fullname, ".", x.second, decoder_depth,
                       temporary_count)) {
        return false;
      }
    }
    ++member_pos;
  }
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  return true;
}

bool DumpArrayDecoder(
    int fd,
    const std::string& name,
    const std::string& member_accessor,
    const std::vector<std::pair<std::string, CppType*>>& members,
    int decoder_depth,
    int* temporary_count) {
  dprintf(fd, "  if (cbor_value_get_type(&it%d) != CborArrayType) {\n",
          decoder_depth - 1);
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd, "  CborValue it%d;\n", decoder_depth);
  dprintf(fd, "  size_t it%d_length = 0;\n", decoder_depth);
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&it%d, "
          "&it%d_length));\n",
          decoder_depth - 1, decoder_depth);
  // TODO(btolsch): Account for optional combinations.
  dprintf(fd, "  if (it%d_length != %d) {\n", decoder_depth,
          static_cast<int>(members.size()));
  dprintf(fd, "    return -1;\n");
  dprintf(fd, "  }\n");
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  int member_pos = 0;
  for (auto& x : members) {
    std::string cid = ToCId(x.first);
    std::string fullname = name + member_accessor + cid;
    if (x.second->which == CppType::Which::kOptional) {
      dprintf(fd, "  if (it%d_length > %d) {\n", decoder_depth, member_pos);
      dprintf(fd, "    %s%shas_%s = true;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      if (!DumpDecoder(fd, fullname, ".", x.second->optional_type,
                       decoder_depth, temporary_count)) {
        return false;
      }
      dprintf(fd, "  } else {\n");
      dprintf(fd, "    %s%shas_%s = false;\n", name.c_str(),
              member_accessor.c_str(), cid.c_str());
      dprintf(fd, "  }\n");
    } else {
      if (!DumpDecoder(fd, fullname, ".", x.second, decoder_depth,
                       temporary_count)) {
        return false;
      }
    }
    ++member_pos;
  }
  dprintf(fd,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it%d, &it%d));\n",
          decoder_depth - 1, decoder_depth);
  return true;
}

bool DumpDecoders(int fd, CppSymbolTable* table) {
  int temporary_count = 0;
  for (const auto& entry : table->cpp_type_map) {
    const auto& name = entry.first;
    const auto* type = entry.second;
    if (type->which != CppType::Which::kStruct ||
        type->struct_type.key_type == CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCTypename(name);
    dprintf(fd, "\nssize_t Decode%s(\n", cpp_name.c_str());
    dprintf(fd, "    uint8_t* buffer,\n    size_t length,\n");
    dprintf(fd, "    %s* data) {\n", cpp_name.c_str());
    dprintf(fd, "  CborParser parser;\n");
    dprintf(fd, "  CborValue it0;\n");
    dprintf(
        fd,
        "  CBOR_RETURN_ON_ERROR(cbor_parser_init(buffer, length, 0, &parser, "
        "&it0));\n");
    if (type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!DumpMapDecoder(fd, "data", "->", type->struct_type.members, 1,
                          &temporary_count)) {
        return false;
      }
    } else {
      if (!DumpArrayDecoder(fd, "data", "->", type->struct_type.members, 1,
                            &temporary_count)) {
        return false;
      }
    }
    dprintf(
        fd,
        "  auto result = static_cast<ssize_t>(cbor_value_get_next_byte(&it0) - "
        "buffer);\n");
    dprintf(fd, "  return result;\n");
    dprintf(fd, "}\n");
  }
  return true;
}
