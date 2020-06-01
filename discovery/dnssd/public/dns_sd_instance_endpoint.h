// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_ENDPOINT_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_ENDPOINT_H_

#include <string>

#include "discovery/dnssd/public/dns_sd_instance.h"
#include "discovery/dnssd/public/dns_sd_txt_record.h"
#include "platform/base/interface_info.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace discovery {

// Represents the data stored in DNS records of types SRV, TXT, A, and AAAA
class DnsSdInstanceEndpoint : public DnsSdInstance {
 public:
  // These ctors expect valid input, and will cause a crash if they are not.
  DnsSdInstanceEndpoint(std::string instance_id,
                        std::string service_id,
                        std::string domain_id,
                        DnsSdTxtRecord txt,
                        NetworkInterfaceIndex network_interface,
                        std::vector<IPAddress> addresses);
  DnsSdInstanceEndpoint(DnsSdInstance record,
                        NetworkInterfaceIndex network_interface,
                        std::vector<IPAddress> addresses);

  // NOTE: These constructors expects one endpoint to be an IPv4 address and the
  // other to be an IPv6 address.
  DnsSdInstanceEndpoint(std::string instance_id,
                        std::string service_id,
                        std::string domain_id,
                        DnsSdTxtRecord txt,
                        NetworkInterfaceIndex network_interface,
                        std::vector<IPAddress> addresses);
  DnsSdInstanceEndpoint(DnsSdInstance instance,
                        NetworkInterfaceIndex network_interface,
                        std::vector<IPAddress> addresses);

  // TODO(rwkeane): More useful ctor overloads.

  ~DnsSdInstanceEndpoint() override;

  // Returns the addresses associated with this endpoint.
  const std::vector<IPAddress>& addresses addresses() const {
    return addresses;
  }
  bool HasAddressV4() const;
  bool HasAddressV6() const;

  // Network Interface associated with this endpoint.
  NetworkInterfaceIndex network_interface() const { return network_interface_; }

 private:
  std::vector<IPAddress> addresses_;

  NetworkInterfaceIndex network_interface_;

  friend bool operator<(const DnsSdInstanceEndpoint& lhs,
                        const DnsSdInstanceEndpoint& rhs);
};

bool operator<(const DnsSdInstanceEndpoint& lhs,
               const DnsSdInstanceEndpoint& rhs);

inline bool operator>(const DnsSdInstanceEndpoint& lhs,
                      const DnsSdInstanceEndpoint& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return !(rhs > lhs);
}

inline bool operator>=(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return !(rhs < lhs);
}

inline bool operator==(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return lhs <= rhs && lhs >= rhs;
}

inline bool operator!=(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return !(lhs == rhs);
}

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_ENDPOINT_H_
