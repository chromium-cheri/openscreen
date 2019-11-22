// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder.h"

#include <utility>
#include <vector>

#include "discovery/mdns/mdns_publisher.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {
namespace {

bool IsProbeQuery(const MdnsQuestion& query) {
  return false;
}

bool AddRecord(std::function<void(MdnsRecord record)> add_func,
               MdnsPublisher* publisher,
               const DomainName& domain,
               DnsType type,
               DnsClass clazz,
               bool add_negative_on_unknown) {
  auto extra_records = publisher->GetRecords(domain, type, clazz);
  if (extra_records.size()) {
    for (const MdnsRecord& record : extra_records) {
      add_func(record);
    }
  } else if (add_negative_on_unknown) {
    // TODO(rwkeane): Add negative response NSEC record.
  }

  return extra_records.size();
}

inline bool AddAdditionalRecords(MdnsMessage* message,
                                 MdnsPublisher* publisher,
                                 const DomainName& domain,
                                 DnsType type,
                                 DnsClass clazz) {
  OSP_DCHECK(type == DnsType::kSRV || type == DnsType::kTXT ||
             type == DnsType::kA || type == DnsType::kAAAA);

  auto add_func = [message](MdnsRecord record) {
    message->AddAdditionalRecord(record);
  };
  return AddRecord(add_func, publisher, domain, type, clazz, true);
}

inline bool AddResponseRecords(MdnsMessage* message,
                               MdnsPublisher* publisher,
                               const DomainName& domain,
                               DnsType type,
                               DnsClass clazz,
                               bool add_negative_on_unknown) {
  OSP_DCHECK(type != DnsType::kANY);

  auto add_func = [message](MdnsRecord record) { message->AddAnswer(record); };
  return AddRecord(add_func, publisher, domain, type, clazz,
                   add_negative_on_unknown);
}

void ApplyAnyQueryResults(MdnsMessage* message,
                          MdnsPublisher* publisher,
                          const DomainName& domain,
                          DnsClass clazz) {
  if (AddResponseRecords(message, publisher, domain, DnsType::kPTR, clazz,
                         false)) {
    return;
  }

  if (!AddResponseRecords(message, publisher, domain, DnsType::kSRV, clazz,
                          false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
  if (!AddResponseRecords(message, publisher, domain, DnsType::kTXT, clazz,
                          false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
  if (!AddResponseRecords(message, publisher, domain, DnsType::kA, clazz,
                          false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
  if (!AddResponseRecords(message, publisher, domain, DnsType::kAAAA, clazz,
                          false)) {
    // TODO(rwkeane): Add negative response NSEC record to additional records.
  }
}

void ApplyTypedQueryResults(MdnsMessage* message,
                            MdnsPublisher* publisher,
                            const DomainName& domain,
                            DnsType type,
                            DnsClass clazz) {
  OSP_DCHECK(type != DnsType::kANY);

  if (type == DnsType::kPTR) {
    auto response_records = publisher->GetRecords(domain, type, clazz);
    for (const MdnsRecord& record : response_records) {
      message->AddAnswer(record);

      const DomainName& name =
          absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
      AddAdditionalRecords(message, publisher, name, DnsType::kSRV, clazz);
      AddAdditionalRecords(message, publisher, name, DnsType::kTXT, clazz);
      AddAdditionalRecords(message, publisher, name, DnsType::kA, clazz);
      AddAdditionalRecords(message, publisher, name, DnsType::kAAAA, clazz);
    }
  } else {
    if (AddResponseRecords(message, publisher, domain, type, clazz, true)) {
      if (type == DnsType::kSRV) {
        AddAdditionalRecords(message, publisher, domain, DnsType::kA, clazz);
        AddAdditionalRecords(message, publisher, domain, DnsType::kAAAA, clazz);
      } else if (type == DnsType::kA) {
        AddAdditionalRecords(message, publisher, domain, DnsType::kAAAA, clazz);
      } else if (type == DnsType::kAAAA) {
        AddAdditionalRecords(message, publisher, domain, DnsType::kA, clazz);
      }
    }
  }
}

}  // namespace

MdnsResponder::MdnsResponder(MdnsPublisher* publisher,
                             MdnsSender* sender,
                             MdnsReceiver* receiver,
                             platform::TaskRunner* task_runner,
                             MdnsRandom* random_delay)
    : publisher_(publisher),
      sender_(sender),
      receiver_(receiver),
      task_runner_(task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(publisher_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
  auto func = [this](const MdnsMessage& message, const IPEndpoint& src) {
    OnMessageReceived(message, src);
  };
  receiver_->SetQueryCallback(func);
}

MdnsResponder::~MdnsResponder() {
  receiver_->SetQueryCallback(nullptr);
}

void MdnsResponder::OnMessageReceived(const MdnsMessage& message,
                                      const IPEndpoint& src) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Query);

  for (const auto& question : message.questions()) {
    bool is_exclusive_owner = publisher_->IsExclusiveOwner(question.name());
    if (is_exclusive_owner ||
        publisher_->HasRecords(question.name(), question.dns_type(),
                               question.dns_class())) {
      std::function<void(const MdnsMessage&)> send_response =
          [this](const MdnsMessage& message) {
            sender_->SendMulticast(message);
          };
      if (question.response_type() == ResponseType::kUnicast) {
        send_response = [this, src](const MdnsMessage& message) {
          sender_->SendUnicast(message, src);
        };
      }

      if (is_exclusive_owner || IsProbeQuery(question)) {
        SendResponse(question, send_response, is_exclusive_owner);
      } else {
        platform::Clock::duration delay =
            random_delay_->GetTruncatedQueryResponseDelay();
        auto response = [this, question, send_response, is_exclusive_owner]() {
          SendResponse(question, send_response, is_exclusive_owner);
        };
        task_runner_->PostTaskWithDelay(response, delay);
      }
    }
  }
}

void MdnsResponder::SendResponse(
    MdnsQuestion question,
    std::function<void(const MdnsMessage&)> send_response,
    bool is_exclusive_owner) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  MdnsMessage message(CreateMessageId(), MessageType::Response);

  if (question.dns_type() == DnsType::kANY) {
    ApplyAnyQueryResults(&message, publisher_, question.name(),
                         question.dns_class());
  } else {
    ApplyTypedQueryResults(&message, publisher_, question.name(),
                           question.dns_type(), question.dns_class());
  }

  if (message.answers().size()) {
    // Send the response only if there are any records we care about.
    send_response(message);
  }
}

}  // namespace discovery
}  // namespace openscreen
