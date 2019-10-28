// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_service_impl.h"

namespace cast {
namespace mdns {

void MdnsServiceImpl::StartQuery(const DomainName& name,
                                 DnsType dns_type,
                                 DnsClass dns_class,
                                 MdnsRecordChangedCallback* callback) {}

void MdnsServiceImpl::StopQuery(const DomainName& name,
                                DnsType dns_type,
                                DnsClass dns_class,
                                MdnsRecordChangedCallback* callback) {}

void MdnsServiceImpl::RegisterRecord(const MdnsRecord& record) {}

void MdnsServiceImpl::DeregisterRecord(const MdnsRecord& record) {}

}  // namespace mdns
}  // namespace cast
