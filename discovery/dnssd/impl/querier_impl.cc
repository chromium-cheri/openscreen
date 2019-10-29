// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <string>

#include "platform/api/logging.h"

namespace openscreen {
namespace discovery {
namespace {}  // namespace

QuerierImpl::QuerierImpl(MdnsQuerier* querier) : querier_(querier) {
  OSP_DCHECK(querier_);
}

void QuerierImpl::StartQuery(const absl::string_view& service,
                             const absl::string_view& domain,
                             Callback* cb) {
  // TODO(rwkeane): Implement this method.
}

void QuerierImpl::StopQuery(const absl::string_view& service,
                            const absl::string_view& domain,
                            Callback* cb) {
  // TODO(rwkeane): Implement this method.
}

void QuerierImpl::OnRecordChanged(const cast::mdns::MdnsRecord& record,
                                  cast::mdns::RecordChangedEvent event) {
  // TODO(rwkeane): Implement this method.
}

}  // namespace discovery
}  // namespace openscreen
