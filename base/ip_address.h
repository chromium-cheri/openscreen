// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IP_ADDRESS_H_
#define BASE_IP_ADDRESS_H_

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>

namespace openscreen {

struct IPV4Address {
 public:
  static bool Parse(const std::string& s, IPV4Address* address);

  IPV4Address();
  explicit IPV4Address(const std::array<uint8_t, 4>& bytes);
  explicit IPV4Address(const uint8_t (&b)[4]);
  // IPV4Address(const uint8_t*) disambiguated with
  // IPV4Address(const uint8_t (&)[4]).
  template <typename T,
            typename = typename std::enable_if<
                std::is_convertible<T, const uint8_t*>::value &&
                !std::is_convertible<T, const uint8_t (&)[4]>::value>::type>
  explicit IPV4Address(T b) : bytes({b[0], b[1], b[2], b[3]}) {}
  IPV4Address(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
  IPV4Address(const IPV4Address& o);
  ~IPV4Address() = default;

  IPV4Address& operator=(const IPV4Address& o);

  bool operator==(const IPV4Address& o) const;
  bool operator!=(const IPV4Address& o) const;

  std::array<uint8_t, 4> bytes;
};

struct IPV6Address {
 public:
  static bool Parse(const std::string& s, IPV6Address* address);

  IPV6Address();
  explicit IPV6Address(const std::array<uint8_t, 16>& bytes);
  explicit IPV6Address(const uint8_t (&b)[16]);
  template <typename T,
            typename = typename std::enable_if<
                std::is_convertible<T, const uint8_t*>::value &&
                !std::is_convertible<T, const uint8_t (&)[16]>::value>::type>
  explicit IPV6Address(T b)
      : bytes({b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9],
               b[10], b[11], b[12], b[13], b[14], b[15]}) {}
  IPV6Address(uint8_t b1,
              uint8_t b2,
              uint8_t b3,
              uint8_t b4,
              uint8_t b5,
              uint8_t b6,
              uint8_t b7,
              uint8_t b8,
              uint8_t b9,
              uint8_t b10,
              uint8_t b11,
              uint8_t b12,
              uint8_t b13,
              uint8_t b14,
              uint8_t b15,
              uint8_t b16);
  IPV6Address(const IPV6Address& o);
  ~IPV6Address() = default;

  IPV6Address& operator=(const IPV6Address& o);

  bool operator==(const IPV6Address& o) const;
  bool operator!=(const IPV6Address& o) const;

  std::array<uint8_t, 16> bytes;
};

struct IPV4Endpoint {
 public:
  IPV4Address address;
  uint16_t port;
};

struct IPV6Endpoint {
 public:
  IPV6Address address;
  uint16_t port;
};

}  // namespace openscreen

#endif
