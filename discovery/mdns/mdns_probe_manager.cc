// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe_manager.h"

#include <string>
#include <utility>

#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {
namespace {

// The timespan by which to delay subsequent mDNS Probe queries for the same
// domain name when a simultaneous query from another host is detected, as
// described in RFC 6762 section 8.2
constexpr std::chrono::seconds kSimultaneousProbeDelay =
    std::chrono::seconds(1);

// Maximum size of the first label in a DomainLabel.
constexpr size_t kMaxDomainLabelSize = 64;

DomainName CreateRetryDomainName(const DomainName& name, int attempt) {
  OSP_DCHECK(name.labels().size());
  std::vector<std::string> labels = name.labels();
  std::string label = std::move(labels[0]);
  std::string attempts_str = std::to_string(attempt);
  if (label.size() + attempts_str.size() >= kMaxDomainLabelSize) {
    label = label.substr(0, kMaxDomainLabelSize - attempts_str.size());
  }
  label.append(std::move(attempts_str));

  labels[0] = std::move(label);
  return DomainName(labels);
}

}  // namespace

MdnsProbeManager::~MdnsProbeManager() = default;

MdnsProbeManagerImpl::MdnsProbeManagerImpl(MdnsSender* sender,
                                           MdnsQuerier* querier,
                                           MdnsRandom* random_delay,
                                           TaskRunner* task_runner)
    : sender_(sender),
      querier_(querier),
      random_delay_(random_delay),
      task_runner_(task_runner) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(querier_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
}

MdnsProbeManagerImpl::~MdnsProbeManagerImpl() = default;

Error MdnsProbeManagerImpl::StartProbe(MdnsDomainConfirmedProvider* callback,
                                       DomainName requested_name,
                                       IPEndpoint endpoint) {
  // Check if |requested_name| is already being queried for.
  auto ongoing_it =
      std::find_if(ongoing_probes_.begin(), ongoing_probes_.end(),
                   [&requested_name](const OngoingProbe& ongoing) {
                     return ongoing.requested_name == requested_name;
                   });
  if (ongoing_it != ongoing_probes_.end()) {
    return Error::Code::kItemAlreadyExists;
  }

  // Check if |requested_name| is already claimed.
  auto completed_it = std::find_if(
      completed_probes_.begin(), completed_probes_.end(),
      [&requested_name](const std::unique_ptr<MdnsProbe>& completed) {
        return completed->target_name() == requested_name;
      });
  if (completed_it != completed_probes_.end()) {
    return Error::Code::kItemAlreadyExists;
  }

  // Begin a new probe.
  auto probe = CreateProbe(requested_name, std::move(endpoint));
  ongoing_probes_.emplace_back(std::move(probe), std::move(requested_name),
                               callback);
  return Error::None();
}

Error MdnsProbeManagerImpl::StopProbe(const DomainName& requested_name) {
  auto it = std::find_if(ongoing_probes_.begin(), ongoing_probes_.end(),
                         [&requested_name](const OngoingProbe& ongoing) {
                           return ongoing.requested_name == requested_name;
                         });
  if (it == ongoing_probes_.end()) {
    return Error::Code::kItemNotFound;
  }

  ongoing_probes_.erase(it);
  return Error::None();
}

bool MdnsProbeManagerImpl::IsDomainClaimed(const DomainName& domain) const {
  return std::find_if(completed_probes_.begin(), completed_probes_.end(),
                      [&domain](const std::unique_ptr<MdnsProbe>& probe) {
                        return probe->target_name() == domain;
                      }) != completed_probes_.end();
}

void MdnsProbeManagerImpl::RespondToProbeQuery(const MdnsMessage& message,
                                               const IPEndpoint& src) {
  OSP_DCHECK(!message.questions().empty());

  const std::vector<MdnsQuestion>& questions = message.questions();
  MdnsMessage send_message(CreateMessageId(), MessageType::Response);

  // Iterate across all questions asked and all completed probes and add A or
  // AAAA records associated with the endpoints for which the names match.
  // |questions| is expected to be of size 1 and |completed_probes| should be
  // small (generally size 1), so this should be fast.
  for (const auto& question : questions) {
    for (auto it = completed_probes_.begin(); it != completed_probes_.end();
         it++) {
      if (question.name() == (*it)->target_name()) {
        send_message.AddAnswer(
            CreateAddressRecord((*it)->target_name(), (*it)->endpoint()));
        break;
      }
    }
  }

  if (!send_message.answers().empty()) {
    sender_->SendUnicast(send_message, src);
  } else {
    // If the name isn't already claimed, check to see if a probe is ongoing. If
    // so, compare the address record for that probe with the one in the
    // received message and resolve as specified in RFC 6762 section 8.2.
    TiebreakSimultaneousProbes(message);
  }
}

void MdnsProbeManagerImpl::TiebreakSimultaneousProbes(
    const MdnsMessage& message) {
  OSP_DCHECK(!message.questions().empty());
  OSP_DCHECK(!message.authority_records().empty());

  for (const auto& question : message.questions()) {
    for (auto it = ongoing_probes_.begin(); it != ongoing_probes_.end(); it++) {
      if (it->probe->target_name() == question.name()) {
        // When a host is probing for a set of records with the same name, or a
        // message is received containing multiple tiebreaker records answering
        // a given probe question in the Question Section, the host's records
        // and the tiebreaker records from the message are each sorted into
        // order, and then compared pairwise, using the same comparison
        // technique described above, until a difference is found. Because the
        // probe object is guaranteed to only have the address record, only the
        // lowest authority record is needed.
        MdnsRecord const* lowest_record = &message.authority_records()[0];
        for (auto auth_it = ++message.authority_records().begin();
             auth_it != message.authority_records().end(); auth_it++) {
          if (*auth_it < *lowest_record) {
            lowest_record = &(*auth_it);
          }
        }

        // If this host finds that its own data is lexicographically later, it
        // simply ignores the other host's probe. The other host will have
        // receive this host's probe simultaneously, and will reject its own
        // probe through this same calculation.
        //
        // Otherwise, it defers to the winning host by waiting one second, and
        // then begins probing for this record again. See RFC 6762 section 8.2
        // for the logic behind waiting one second.
        MdnsRecord probe_record = CreateAddressRecord(it->probe->target_name(),
                                                      it->probe->endpoint());
        if (probe_record < *lowest_record) {
          it->probe->Postpone(kSimultaneousProbeDelay);
        }

        break;
      }
    }
  }
}

MdnsRecord MdnsProbeManagerImpl::CreateAddressRecord(
    DomainName name,
    const IPEndpoint& endpoint) {
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

void MdnsProbeManagerImpl::OnProbeSuccess(MdnsProbe* probe) {
  auto ongoing_it = std::find_if(ongoing_probes_.begin(), ongoing_probes_.end(),
                                 [probe](const OngoingProbe& ongoing) {
                                   return ongoing.probe.get() == probe;
                                 });

  if (ongoing_it != ongoing_probes_.end()) {
    DomainName target_name = ongoing_it->probe->target_name();
    completed_probes_.push_back(std::move(ongoing_it->probe));
    DomainName requested = std::move(ongoing_it->requested_name);
    MdnsDomainConfirmedProvider* callback = ongoing_it->callback;
    ongoing_probes_.erase(ongoing_it);
    callback->OnDomainFound(std::move(requested), std::move(target_name));
  }
}

void MdnsProbeManagerImpl::OnProbeFailure(MdnsProbe* probe) {
  auto it = std::find_if(ongoing_probes_.begin(), ongoing_probes_.end(),
                         [probe](const OngoingProbe& ongoing) {
                           return ongoing.probe.get() == probe;
                         });
  if (it == ongoing_probes_.end()) {
    // This means that the probe was canceled.
    return;
  }

  // Create a new probe with a modified domain name.
  DomainName new_name =
      CreateRetryDomainName(it->requested_name, ++it->attempts);

  // If this domain has already been claimed, skip ahead to knowing it's
  // claimed.
  auto completed_it =
      std::find_if(completed_probes_.begin(), completed_probes_.end(),
                   [&new_name](const std::unique_ptr<MdnsProbe>& completed) {
                     return completed->target_name() == new_name;
                   });

  if (completed_it != completed_probes_.end()) {
    DomainName requested_name = std::move(it->requested_name);
    MdnsDomainConfirmedProvider* callback = it->callback;
    ongoing_probes_.erase(it);
    callback->OnDomainFound(requested_name, (*completed_it)->target_name());
  } else {
    std::unique_ptr<MdnsProbe> new_probe =
        CreateProbe(std::move(new_name), it->probe->endpoint());
    it->probe = std::move(new_probe);
  }
}

MdnsProbeManagerImpl::OngoingProbe::OngoingProbe(
    std::unique_ptr<MdnsProbe> probe,
    DomainName name,
    MdnsDomainConfirmedProvider* callback)
    : probe(std::move(probe)),
      requested_name(std::move(name)),
      callback(callback) {}

}  // namespace discovery
}  // namespace openscreen
