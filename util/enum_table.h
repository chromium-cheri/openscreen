// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_ENUM_TABLE_H_
#define UTIL_ENUM_TABLE_H_

#include <cstdint>
#include <cstring>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "util/osp_logging.h"

// A bidirectional mapping between enum values and strings.
// Ported from Chrome's components/cast_channel/enum_table.
//
// Typical Usage
// -------------
//
// The typical way to use this class is to define a specialization of
// EnumTable<E>::instance for a specific enum type, which is initialized with
// a table of enum values and their corresponding strings:
//
// In .h file:
//     enum class MyEnum { kFoo, kBar, ..., kOther, kMaxValue = kOther };
//
// In .cc file:
//
//     template <>
//     const EnumTable<MyEnum> EnumTable<MyEnum>::instance({
//         {MyEnum::kFoo, "FOO"},
//         {MyEnum::kBar, "BAR"},
//         ...
//         {MyEnum::kOther},  // Not all values need strings.
//       },
//       MyEnum::kMaxValue);  // Needed to detect missing table entries.
//
// The functions EnumToString() and StringToEnum() are used to look up values
// in the table.  In some cases, it may be useful to wrap these functions for
// special handling of unknown values, etc.:
//
//     const char* MyEnumToString(MyEnum value) {
//       return EnumToString(value).value_or(nullptr).data();
//     }
//
//     MyEnum StringToMyEnum(const std::string& name) {
//       return StringToEnum<MyEnum>(name).value_or(MyEnum::kOther);
//     }
//
// NOTE: Because it's a template specialization, the definition of
// EnumTable<E>::instance has to be in the openscreen namespace.  Alternatively,
// you can declare your own table with any name you want and call its members
// functions directly.
//
// To get a the string for an enum value that is known at compile time, there is
// a special zero-argument form of EnumToString(), (and a corresponding member
// function in EnumTable):
//
// Compiles to absl::string_view("FOO").
//     EnumToString<MyEnum, MyEnum::kFoo>();
//
// The syntax is a little awkward, but it saves you from having to duplicate
// string literals or define each enum string as a named constant.
//
//
// Consecutive Enum Tables
// -----------------------
//
// When using an EnumTable, it's generally best for the following conditions
// to be true:
//
// - The enum is defined with "enum class" syntax.
//
// - The members have the default values assigned by the compiler.
//
// - There is an extra member named kMaxValue which is set equal to the highest
//   ordinary value.  (The Chromium style checker will verify that kMaxValue
//   really is the maximum value.)
//
// - The values in the EnumTable constructor appear in sorted order.
//
// - Every member of the enum (other than kMaxValue) is included in the table.
//
// When these conditions are met, the enum's |kMaxValue| member should be passed
// as the second argument of the EnumTable.  This will create a table where enum
// values can be converted to strings in constant time.  It will also reliably
// detect an incomplete enum table during startup of a debug build.
//
//
// Non-Consecutive Enum Tables
// ---------------------------
//
// When the conditions in the previous section cannot be satisfied, the second
// argument of the EnumTable constructor should be the special constant
// |NonConsecutiveEnumTable|.  Doing so has some unfortunate side-effects: there
// is no automatic way to detect when an enum value is missing from the table,
// and looking up a non-constant enum value requires a linear search.

namespace openscreen {

class
#ifdef ARCH_CPU_64_BITS
    alignas(16)
#endif
        GenericEnumTableEntry {
 public:
  inline constexpr GenericEnumTableEntry(int32_t value);  // NOLINT
  inline constexpr GenericEnumTableEntry(int32_t value, absl::string_view str);

 private:
  static const GenericEnumTableEntry* FindByString(
      const std::vector<GenericEnumTableEntry>& data,
      absl::string_view str);

  static absl::optional<absl::string_view> FindByValue(
      const std::vector<GenericEnumTableEntry>& data,
      int value);

  constexpr absl::string_view str() const {
    OSP_DCHECK(has_str());
    return {chars, length};
  }

  constexpr bool has_str() const { return chars != nullptr; }

 private:
  // Instead of storing a absl::string_view, it's broken apart and stored as a
  // pointer and an unsigned int (rather than a std::size_t) so that table
  // entries fit in 16 bytes with no padding on 64-bit platforms.
  const char* chars;
  uint32_t length;

  int32_t value;

  template <typename E>
  friend class EnumTable;
};

// Yes, these constructors really needs to be inlined.  Even though they look
// "complex" to the style checker, everything is executed at compile time, so an
// EnumTable instance can be fully initialized without executing any code.

inline constexpr GenericEnumTableEntry::GenericEnumTableEntry(int32_t value)
    : chars(nullptr), length(UINT32_MAX), value(value) {}

inline constexpr GenericEnumTableEntry::GenericEnumTableEntry(
    int32_t value,
    absl::string_view str)
    : chars(str.data()),
      length(static_cast<uint32_t>(str.length())),
      value(value) {}

struct NonConsecutiveEnumTable_t {};
constexpr NonConsecutiveEnumTable_t NonConsecutiveEnumTable;

// A table for associating enum values with string literals.  This class is
// designed for use as an initialized global variable, so it has a trivial
// destructor and a simple constant-time constructor.
template <typename E>
class EnumTable {
 public:
  class Entry : public GenericEnumTableEntry {
   public:
    // Constructor for placeholder entries with no string value.
    constexpr Entry(E value)  // NOLINT
        : GenericEnumTableEntry(static_cast<int32_t>(value)) {}

    // Constructor for regular entries.
    constexpr Entry(E value, absl::string_view str)
        : GenericEnumTableEntry(static_cast<int32_t>(value), str) {}
  };

  static_assert(sizeof(E) <= sizeof(int32_t),
                "Enum must not use more than 32 bits of storage.");
  static_assert(sizeof(Entry) == sizeof(GenericEnumTableEntry),
                "EnumTable<E>::Entry must not have its own member variables.");

  // Creates an EnumTable where data[i].value == i for all i.  When a table is
  // created with this constructor, the GetString() method is a simple array
  // lookup that runs in constant time.
  //
  // If |max_value| is specified, all enum values in |data| must be less than or
  // equal to |max_value|.  This feature is intended to help catch errors cause
  // by a new value being added to an enum without the new value being added to
  // the corresponding table.  For best results, use an enum class and create a
  // constant named kMaxValue.  For more details, see
  // https://www.chromium.org/developers/coding-style/chromium-style-checker-errors#TOC-Enumerator-max-values
  constexpr EnumTable(std::initializer_list<Entry> data, E max_value)
      : EnumTable(data, true) {
#ifndef NDEBUG
    // This is compiled out when NDEBUG is defined, even if DCHECKS are normally
    // enabled because DCHECK_ALWAYS_ON is defined.  NDEBUG is only defined
    // inside of Chrome, so these are always compiled in in standalone builds.
    // The reason for this is that if DCHECKs are enabled, global EnumTable
    // instances will have a nontrivial constructor, which is flagged as a style
    // violation by the linux-rel trybot.
    auto found = FindNonConsecutiveEntry(data);
    OSP_DCHECK(!found) << "Entries' numerical values must be consecutive "
                       << "integers starting from 0; found problem at index "
                       << *found;

    const auto int_max_value = static_cast<int32_t>(max_value);
    OSP_DCHECK(data.end()[-1].value == int_max_value)
        << "Missing entry for enum value " << int_max_value;
#endif  // NDEBUG
  }

  // Creates an EnumTable where data[i].value != i for some values of i.  When
  // a table is created with this constructor, the GetString() method must
  // perform a linear search to find the correct string.
  constexpr EnumTable(std::initializer_list<Entry> data,
                      NonConsecutiveEnumTable_t)
      : EnumTable(data, false) {
#ifndef NDEBUG
    OSP_DCHECK(FindNonConsecutiveEntry(data))
        << "Don't use this constructor for sorted entries.";
#endif  // NDEBUG
  }

  // Gets the string associated with the given enum value.  When the argument
  // is a constant, prefer the zero-argument form below.
  inline absl::optional<absl::string_view> GetString(E value) const {
    if (is_sorted_) {
      const std::size_t index = static_cast<std::size_t>(value);
      if (index < data_.size()) {
        const auto& entry = data_.begin()[index];
        if (entry.has_str()) {
          return entry.str();
        }
      }
      return absl::nullopt;
    }
    return GenericEnumTableEntry::FindByValue(data_,
                                              static_cast<int32_t>(value));
  }

  // This overload of GetString is designed for cases where the argument is a
  // literal value.  It will be evaluated at compile time in non-debug builds.
  //
  // If |Value| has no string defined in the table, you'll get an error at
  // runtime in debug builds, but in release builds this function will just
  // return a string holding an error message.  Unfortunately it doesn't seem
  // possible to report an error in evaluating a constexpr function at compile
  // time without using exceptions.
  template <E Value>
  absl::string_view GetString() const {
    for (const auto& entry : data_) {
      if (entry.value == static_cast<int32_t>(Value) && entry.has_str())
        return entry.str();
    }

    OSP_NOTREACHED() << "No string for enum value: "
                     << static_cast<int32_t>(Value);
    return "[invalid enum value]";
  }

  // Gets the enum value associated with the given string.  Unlike
  // GetString(), this method is not defined as a constexpr, because it should
  // never be called with a literal string; it's simpler to just refer to the
  // enum value directly.
  absl::optional<E> GetEnum(absl::string_view str) const {
    auto* entry = GenericEnumTableEntry::FindByString(data_, str);
    return entry ? static_cast<E>(entry->value) : absl::optional<E>();
  }

  // The default instance of this class.  There should normally only be one
  // instance of this class for a given enum type.  Users of this class are
  // responsible for providing a suitable definition for each enum type if the
  // EnumToString() or StringToEnum() functions are used.
  static const EnumTable<E> instance;

 private:
#ifdef ARCH_CPU_64_BITS
  // Align the data on a cache line boundary.
  alignas(64)
#endif
      std::vector<GenericEnumTableEntry> data_;
  bool is_sorted_;

  constexpr EnumTable(std::initializer_list<Entry> data, bool is_sorted)
      : is_sorted_(is_sorted) {
#ifndef NDEBUG
    // If the table is too large, it's probably time to update this class to do
    // something smarter than a linear search.
    OSP_DCHECK(data.size() <= 32) << "Table too large.";
    data_.reserve(data.size());
    for (std::size_t i = 0; i < data.size(); i++) {
      data_.push_back(data.begin()[i]);
      for (std::size_t j = i + 1; j < data.size(); j++) {
        const Entry& ei = data.begin()[i];
        const Entry& ej = data.begin()[j];
        OSP_DCHECK(ei.value != ej.value)
            << "Found duplicate enum values at indices " << i << " and " << j;
        OSP_DCHECK(!(ei.has_str() && ej.has_str() && ei.str() == ej.str()))
            << "Found duplicate strings at indices " << i << " and " << j;
      }
    }
#endif  // NDEBUG
  }

#ifndef NDEBUG
  // Finds and returns the first i for which data[i].value != i;
  constexpr static absl::optional<std::size_t> FindNonConsecutiveEntry(
      std::initializer_list<Entry> data) {
    int32_t counter = 0;
    for (const auto& entry : data) {
      if (entry.value != counter) {
        return counter;
      }
      ++counter;
    }
    return {};
  }
#endif  // NDEBUG
};

// Converts an enum value to a string using the default table
// (EnumTable<E>::instance) for the given enum type.
template <typename E>
inline absl::optional<absl::string_view> EnumToString(E value) {
  return EnumTable<E>::instance.GetString(value);
}

// Converts a literal enum value to a string at compile time using the default
// table (EnumTable<E>::instance) for the given enum type.
template <typename E, E Value>
inline absl::string_view EnumToString() {
  return EnumTable<E>::instance.template GetString<Value>();
}

// Converts a string to an enum value using the default table
// (EnumTable<E>::instance) for the given enum type.
template <typename E>
inline absl::optional<E> StringToEnum(absl::string_view str) {
  return EnumTable<E>::instance.GetEnum(str);
}

}  // namespace openscreen

#endif  // UTIL_ENUM_TABLE_H_
