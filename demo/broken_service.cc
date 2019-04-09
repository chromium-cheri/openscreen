#include <arpa/inet.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/ip_address.h"
#include "discovery/mdns/domain_name.h"
#include "discovery/mdns/mdns_responder_platform.h"
#include "platform/api/error.h"
#include "platform/api/event_waiter.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"
#include "third_party/mDNSResponder/src/mDNSCore/mDNSEmbeddedAPI.h"

#define ArrayCount(x) (sizeof(x) / sizeof((x)[0]))

namespace openscreen {
namespace {

// RFC 1035 specifies a max string length of 256, including the leading length
// octet.
constexpr size_t kMaxDnsStringLength = 255;

// RFC 6763 recommends a maximum key length of 9 characters.
constexpr size_t kMaxTxtKeyLength = 9;

constexpr size_t kMaxStaticTxtDataSize = 256;

constexpr char kTestServiceInstance[] = "turtle";
constexpr char kTestServiceName[] = "_googlecast";
constexpr char kTestServiceProtocol[] = "_tcp";
constexpr char kTestHostname[] = "alpha";
constexpr uint16_t kTestPort = 12345;

void MdnsStatusCallback(mDNS* mdns, mStatus result) {
  OSP_LOG_INFO << "status good? " << (result == mStatus_NoError);
}

void AssignMdnsPort(mDNSIPPort* mdns_port, uint16_t port) {
  mdns_port->b[0] = (port >> 8) & 0xff;
  mdns_port->b[1] = port & 0xff;
}

void MakeSubnetMaskFromPrefixLengthV4(uint8_t mask[4], uint8_t prefix_length) {
  for (int i = 0; i < 4; prefix_length -= 8, ++i) {
    if (prefix_length >= 8) {
      mask[i] = 0xff;
    } else if (prefix_length > 0) {
      mask[i] = 0xff << (8 - prefix_length);
    } else {
      mask[i] = 0;
    }
  }
}

void MakeSubnetMaskFromPrefixLengthV6(uint8_t mask[16], uint8_t prefix_length) {
  for (int i = 0; i < 16; prefix_length -= 8, ++i) {
    if (prefix_length >= 8) {
      mask[i] = 0xff;
    } else if (prefix_length > 0) {
      mask[i] = 0xff << (8 - prefix_length);
    } else {
      mask[i] = 0;
    }
  }
}

bool IsValidTxtDataKey(const std::string& s) {
  if (s.size() > kMaxTxtKeyLength)
    return false;
  for (unsigned char c : s)
    if (c < 0x20 || c > 0x7e || c == '=')
      return false;
  return true;
}

std::string MakeTxtData(const std::map<std::string, std::string>& txt_data) {
  std::string txt;
  txt.reserve(kMaxStaticTxtDataSize);
  for (const auto& line : txt_data) {
    const auto key_size = line.first.size();
    const auto value_size = line.second.size();
    const auto line_size = value_size ? (key_size + 1 + value_size) : key_size;
    if (!IsValidTxtDataKey(line.first) || line_size > kMaxDnsStringLength ||
        (txt.size() + 1 + line_size) > kMaxStaticTxtDataSize) {
      return {};
    }
    txt.push_back(line_size);
    txt += line.first;
    if (value_size) {
      txt.push_back('=');
      txt += line.second;
    }
  }
  return txt;
}

std::vector<platform::UdpSocketPtr> SetupMulticastSockets(
    const std::vector<platform::InterfaceIndex>& index_list) {
  std::vector<platform::UdpSocketPtr> fds;
  for (const auto ifindex : index_list) {
    auto* socket = platform::CreateUdpSocketIPv4();
    if (!JoinUdpMulticastGroup(socket, IPAddress{224, 0, 0, 251}, ifindex)) {
      OSP_LOG_ERROR << "join multicast group failed for interface " << ifindex
                    << ": " << platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }
    if (!BindUdpSocket(socket, {{}, 5353}, ifindex)) {
      OSP_LOG_ERROR << "bind failed for interface " << ifindex << ": "
                    << platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }

    OSP_LOG_INFO << "listening on interface " << ifindex;
    fds.push_back(socket);
  }
  return fds;
}

void MdnsRegisterInterface(mDNS* mdns,
                           NetworkInterfaceInfo* info,
                           const platform::InterfaceInfo& interface_info,
                           const platform::IPSubnet& subnet,
                           platform::UdpSocketPtr socket) {
  info->InterfaceID = reinterpret_cast<mDNSInterfaceID>(socket);
  info->Advertise = mDNSfalse;
  if (subnet.address.IsV4()) {
    info->ip.type = mDNSAddrType_IPv4;
    subnet.address.CopyToV4(info->ip.ip.v4.b);
    info->mask.type = mDNSAddrType_IPv4;
    MakeSubnetMaskFromPrefixLengthV4(info->mask.ip.v4.b, subnet.prefix_length);
  } else {
    info->ip.type = mDNSAddrType_IPv6;
    subnet.address.CopyToV6(info->ip.ip.v6.b);
    info->mask.type = mDNSAddrType_IPv6;
    MakeSubnetMaskFromPrefixLengthV6(info->mask.ip.v6.b, subnet.prefix_length);
  }

  interface_info.CopyHardwareAddressTo(info->MAC.b);
  info->McastTxRx = 1;
  auto result = mDNS_RegisterInterface(mdns, info, mDNSfalse);
  OSP_LOG_IF(WARN, result != mStatus_NoError)
      << "mDNS_RegisterInterface failed: " << result;
}

void BrokenServiceDemo() {
  CacheEntity rr_cache[500];
  mDNS mdns;
  mDNS_PlatformSupport platform_storage;
  NetworkInterfaceInfo info2 = {};
  NetworkInterfaceInfo info3 = {};

  mDNS_Init(&mdns, &platform_storage, rr_cache, ArrayCount(rr_cache),
            mDNS_Init_DontAdvertiseLocalAddresses, &MdnsStatusCallback,
            mDNS_Init_NoInitCallbackContext);
  std::string host_label("alpha");
  MakeDomainLabelFromLiteralString(&mdns.hostlabel, host_label.c_str());
  mDNS_SetFQDN(&mdns);

  std::vector<platform::InterfaceAddresses> interfaces =
      platform::GetInterfaceAddresses();
  std::vector<platform::InterfaceIndex> index_list{2, 3};
  std::vector<platform::UdpSocketPtr> sockets =
      SetupMulticastSockets(index_list);

  platform::InterfaceAddresses interface2;
  platform::InterfaceAddresses interface3;
  platform::UdpSocketPtr socket2 = sockets[0];
  platform::UdpSocketPtr socket3 = sockets[0];
  platform_storage.sockets.push_back(socket2);
  platform_storage.sockets.push_back(socket3);
  auto interface_id2 = reinterpret_cast<mDNSInterfaceID>(socket2);
  auto interface_id3 = reinterpret_cast<mDNSInterfaceID>(socket3);
  for (auto& iface : interfaces) {
    if (iface.info.index == 2) {
      interface2 = std::move(iface);
    } else if (iface.info.index == 3) {
      interface3 = std::move(iface);
    }
  }
  MdnsRegisterInterface(&mdns, &info2, interface2.info, interface2.addresses[0],
                        socket2);
  MdnsRegisterInterface(&mdns, &info3, interface3.info, interface3.addresses[0],
                        socket3);

  domainlabel instance;
  domainlabel name;
  domainlabel protocol;
  domainname type;
  domainname domain;
  domainlabel hostlabel;
  domainname host;
  mDNSIPPort port;
  domainname service_name;
  domainname instance_name;

  MakeDomainLabelFromLiteralString(&instance, kTestServiceInstance);
  MakeDomainLabelFromLiteralString(&name, kTestServiceName);
  MakeDomainLabelFromLiteralString(&protocol, kTestServiceProtocol);
  type.c[0] = 0;
  AppendDomainLabel(&type, &name);
  AppendDomainLabel(&type, &protocol);
  const mdns::DomainName local_domain = mdns::DomainName::GetLocalDomain();
  std::copy(local_domain.domain_name().begin(),
            local_domain.domain_name().end(), domain.c);

  MakeDomainLabelFromLiteralString(&hostlabel, kTestHostname);
  AppendDomainLabel(&host, &hostlabel);
  AppendDomainName(&host, &domain);
  port.NotAnInteger = htons(kTestPort);

  ConstructServiceName(&service_name, nullptr, &type, &domain);
  ConstructServiceName(&instance_name, &instance, &type, &domain);

  AuthRecord ptr;
  AuthRecord srv;
  AuthRecord txt;
  AuthRecord a;

  mDNS_SetupResourceRecord(&ptr, /** RDataStorage */ nullptr, interface_id2,
                           kDNSType_PTR, 120, kDNSRecordTypeShared,
                           AuthRecordAny,
                           /** Callback */ nullptr, /** Context */ nullptr);
  AssignDomainName(&ptr.namestorage, &service_name);
  AssignDomainName(&ptr.resrec.rdata->u.name, &instance_name);

  mDNS_SetupResourceRecord(&srv, /** RDataStorage */ nullptr, interface_id2,
                           kDNSType_SRV, 1200, kDNSRecordTypeUnique,
                           AuthRecordAny,
                           /** Callback */ nullptr, /** Context */ nullptr);
  AssignDomainName(&srv.namestorage, &instance_name);
  AssignDomainName(&srv.resrec.rdata->u.srv.target, &host);
  srv.resrec.rdata->u.srv.port = port;
  srv.resrec.rdata->u.srv.weight = 0;
  srv.resrec.rdata->u.srv.priority = 0;
  mDNS_Register(&mdns, &srv);

  mDNS_SetupResourceRecord(&txt, /** RDataStorage */ nullptr, interface_id2,
                           kDNSType_TXT, 1200, kDNSRecordTypeUnique,
                           AuthRecordAny,
                           /** Callback */ nullptr, /** Context */ nullptr);
  AssignDomainName(&txt.namestorage, &instance_name);
  std::string txt_data = MakeTxtData({{"fn", "TURTLE"}});
  txt.resrec.rdlength = txt_data.size();
  std::copy(txt_data.begin(), txt_data.end(), txt.resrec.rdata->u.txt.c);
  txt.DependentOn = &srv;
  mDNS_Register(&mdns, &txt);

  mDNS_SetupResourceRecord(&a, /** RDataStorage */ nullptr, interface_id3,
                           kDNSType_A, 1200, kDNSRecordTypeUnique,
                           AuthRecordAny,
                           /** Callback */ nullptr, /** Context */ nullptr);
  AssignDomainName(&a.namestorage, &host);
  interface3.addresses[0].address.CopyToV4(a.resrec.rdata->u.ipv4.b);
  mDNS_Register(&mdns, &a);

  ptr.Additional1 = &srv;
  ptr.Additional2 = &txt;
  mDNS_Register(&mdns, &ptr);
  (void)interface_id3;

  platform::EventWaiterPtr waiter = platform::CreateEventWaiter();
  platform::WatchUdpSocketReadable(waiter, socket2);
  platform::WatchUdpSocketReadable(waiter, socket3);
  while (true) {
    auto data = platform::OnePlatformLoopIteration(waiter);
    for (auto& packet : data) {
      const IPEndpoint& source = packet.source;
      const IPEndpoint& original_destination = packet.original_destination;
      mDNSAddr src;
      if (source.address.IsV4()) {
        src.type = mDNSAddrType_IPv4;
        source.address.CopyToV4(src.ip.v4.b);
      } else {
        src.type = mDNSAddrType_IPv6;
        source.address.CopyToV6(src.ip.v6.b);
      }
      mDNSIPPort srcport;
      AssignMdnsPort(&srcport, source.port);

      mDNSAddr dst;
      if (source.address.IsV4()) {
        dst.type = mDNSAddrType_IPv4;
        original_destination.address.CopyToV4(dst.ip.v4.b);
      } else {
        dst.type = mDNSAddrType_IPv6;
        original_destination.address.CopyToV6(dst.ip.v6.b);
      }
      mDNSIPPort dstport;
      AssignMdnsPort(&dstport, original_destination.port);
      mDNSCoreReceive(&mdns, packet.bytes.data(),
                      packet.bytes.data() + packet.bytes.size(), &src, srcport,
                      &dst, dstport,
                      reinterpret_cast<mDNSInterfaceID>(packet.socket));
    }
    mDNS_Execute(&mdns);
  }
}

}  // namespace
}  // namespace openscreen

int main(int argc, char** argv) {
  openscreen::platform::LogInit(nullptr);
  openscreen::platform::SetLogLevel(openscreen::platform::LogLevel::kVerbose,
                                    2);

  openscreen::BrokenServiceDemo();

  return 0;
}
