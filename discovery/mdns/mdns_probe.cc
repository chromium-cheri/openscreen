// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe.h"

#include <utility>

#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace discovery {

MdnsRecord CreateAddressRecord(DomainName name, const IPEndpoint& endpoint) {
  Rdata rdata;
  DnsType type;
  std::chrono::seconds ttl;
  if (endpoint.address.IsV4()) {
    type = DnsType::kA;
    rdata = ARecordRdata(endpoint.address);
    ttl = kARecordTtl;
  } else {
    type = DnsType::kAAAA;
    rdata = AAAARecordRdata(endpoint.address);
    ttl = kAAAARecordTtl;
  }

  return MdnsRecord(std::move(name), type, DnsClass::kIN, RecordType::kUnique,
                    ttl, std::move(rdata));
}

const Clock::duration MdnsProbe::kDelayBetweenProbeQueries =
    std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds{250});

MdnsProbe::MdnsProbe(DomainName target_name, MdnsRecord address_record)
    : target_name_(std::move(target_name)), record_(std::move(address_record)) {
  OSP_DCHECK(record_.dns_type() == DnsType::kA ||
             record_.dns_type() == DnsType::kAAAA);
}

MdnsProbe::~MdnsProbe() = default;

MdnsProbeImpl::Observer::~Observer() = default;

MdnsProbeImpl::MdnsProbeImpl(MdnsSender* sender,
                             MdnsReceiver* receiver,
                             MdnsRandom* random_delay,
                             TaskRunner* task_runner,
                             ClockNowFunctionPtr now_function,
                             Observer* observer,
                             DomainName target_name,
                             MdnsRecord record)
    : MdnsProbe(std::move(target_name), std::move(record)),
      sender_(sender),
      receiver_(receiver),
      random_delay_(random_delay),
      task_runner_(task_runner),
      now_function_(now_function),
      observer_(observer),
      alarm_(now_function_, task_runner_) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(random_delay_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(observer_);

  receiver_->AddResponseCallback(this);
  alarm_.ScheduleFromNow([this]() { ProbeOnce(); },
                         random_delay_->GetProbeInitilizationDelay());
}

MdnsProbeImpl::~MdnsProbeImpl() {
  Stop();
}

void MdnsProbeImpl::ProbeOnce() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  if (successful_probe_queries_++ < 3) {
    // MdnsQuerier cannot be used, because probe queries cannot use the cache,
    // so instead directly send the query through the MdnsSender.
    MdnsMessage probe_query(CreateMessageId(), MessageType::Query);
    MdnsQuestion probe_question(target_name(), DnsType::kANY, DnsClass::kIN,
                                ResponseType::kUnicast);
    probe_query.AddQuestion(probe_question);
    probe_query.AddAuthorityRecord(address_record());
    sender_->SendMulticast(probe_query);

    alarm_.ScheduleFromNow([this]() { ProbeOnce(); },
                           kDelayBetweenProbeQueries);
  } else {
    Stop();
    observer_->OnProbeSuccess(this);
  }
}

void MdnsProbeImpl::Stop() {
  if (is_running_) {
    alarm_.Cancel();
    receiver_->RemoveResponseCallback(this);
    is_running_ = false;
  }
}

void MdnsProbeImpl::Postpone(std::chrono::seconds delay) {
  successful_probe_queries_ = 0;
  alarm_.Cancel();
  alarm_.ScheduleFromNow([this]() { ProbeOnce(); },
                         std::chrono::duration_cast<Clock::duration>(delay));
}

void MdnsProbeImpl::OnMessageReceived(const MdnsMessage& message) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Response);

  for (const auto& record : message.answers()) {
    if (record.name() == target_name()) {
      Stop();
      observer_->OnProbeFailure(this);
    }
  }
}

}  // namespace discovery
}  // namespace openscreen
