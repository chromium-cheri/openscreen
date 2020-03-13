// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cast/standalone_receiver/cast_agent.h"
#include "discovery/common/config.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

namespace {
enum class ReceiverStatus : uint8_t { kIdle = 0, kBusyJoin = 1 };

// See:
// https://docs.google.com/document/d/1d1wuxHioJ9cBVBQ6UwqBFVMrn48Nb3Yp5k1a_eTXsHE/edit#

// and:
// chromium/src/chromecast/internal/receiver/app/mdns_util_test.cc

// TODO(jophba): add an API for properly setting these fields.
/*
Key
Value
Value Size
id
(integer, 128 bits) A UUID for the Cast receiver.
32

ve
(integer, 8 bits) Cast protocol version supported.  Begins at 2 and is
incremented by 1 with each version.
2

ca
(integer, 64 bits) A bitfield of device capabilities.  Values defined below.
16

st
(integer, 8 bits) Receiver status flag (see below).
2

dc
(integer, 32 bits) CRC-32 checksum of the receiver extra data.  If set, the
client may use this to cache the extra data.
8

pk
(binary, 256 bits) 256-bit receiver Subject Public Key Identifier from the SSL
cert. 64

fn
(string) The friendly name of the device, e.g. “Living Room TV”
64

md
(string) The model name of the device, e.g. “Eureka v1”, “Mollie”
16

dn
(string) The uPnP Unique Device Name, without the uuid: prefix and with dashes
removed, if the device is also advertised through DIAL.  See section 1.1.4 of
the uPnP specification for the format. 16

Total (approximately)
240
*/
// TODO: set most on construction, update as needed
void SetRecordProperties(discovery::DnsSdTxtRecord* record) {
  record->SetValue("id", 0 /* 128 bit uuid integer */);
  record->SetValue("ve", 2 /* Cast protocol version supported */);
  // TODO(jopbha): define capabilities bitfield
  record->SetValue("ca", 0 /* device capabilities bitfield */);

  // TODO(jophba): add an API for notifying senders that the ReceiverStatus is
  // busy.
  record->SetValue("st", static_cast<uint8_t>(ReceiverStatus::kIdle));
  record->SetValue(
      "pk", "" /* 256-bit receiver Subject Public Key ID from SSL cert */);
  record->SetValue(
      "fn", "Libcast Standalone Receiver" /* the devices' friendly name */);
}

}  // namespace

CastAgentPublisher::CastAgentPublisher(TaskRunner* task_runner,
                                       InterfaceInfo interface)
    : task_runner_(task_runner),
      interface_(std::move(interface)),
      // TODO: how to instantiate the record_?
      record_("instance",
              "service",
              "domain",
              IPEndpoint{},
              discovery::DnsSdTxtRecord{}) {
  discovery::Config config;
  config.interface = interface_;
  dns_sd_service_ = discovery::DnsSdService::Create(task_runner_, this, config);
}

CastAgentPublisher::~CastAgentPublisher() = default;

Error CastAgentPublisher::Publish() {
  if (!dns_sd_service_->GetPublisher()) {
    return Error(Error::Code::kRecordPublicationError,
                 "Failed to publish over DNS-SD");
  }

  SetRecordProperties(&record_.txt());
  auto error = dns_sd_service_->GetPublisher()->Register(record_, this);
  if (!error.ok()) {
    return error;
  }

  return Error::None();
}

Error CastAgentPublisher::Unpublish() {
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

void CastAgentPublisher::OnFatalError(Error error) {
  OSP_LOG_ERROR << "Cast agent received fatal discovery error: " << error;
}

void CastAgentPublisher::OnRecoverableError(Error error) {
  OSP_LOG_WARN << "Cast agent received recoverable discovery error: " << error;
}

void CastAgentPublisher::OnInstanceClaimed(
    const discovery::DnsSdInstanceRecord& requested_record,
    const discovery::DnsSdInstanceRecord& claimed_record) {
  OSP_VLOG << "Successfully claimed instanced record, requested: "
           << requested_record.address_v4()
           << ", actual: " << claimed_record.address_v4();
}

}  // namespace cast
}  // namespace openscreen
