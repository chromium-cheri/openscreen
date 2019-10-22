// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_sender.h"
#include "cast/common/mdns/mdns_trackers.h"

//#include <unordered_set>

namespace cast {
namespace mdns {

MdnsQuerier::MdnsQuerier(MdnsSender* sender,
                         MdnsReceiver* receiver,
                         TaskRunner* task_runner,
                         ClockNowFunctionPtr now_function,
                         MdnsRandom* random_delay)
    : sender_(sender),
      receiver_(receiver),
      task_runner_(task_runner),
      now_function_(now_function),
      random_delay_(random_delay) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(receiver_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(now_function_);
  OSP_DCHECK(random_delay_);

  receiver_->SetResponseCallback(
      [this](const MdnsMessage& message) { OnMessageReceived(message); });
}

MdnsQuerier::~MdnsQuerier() {
  receiver_->SetResponseCallback(nullptr);
}

void MdnsQuerier::StartQuery(const DomainName& name,
                             DnsType dns_type,
                             DnsClass dns_class,
                             MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback);

  const Key key(name, dns_type, dns_class);

  auto callbacks = callbacks_.equal_range(key);
  std::pair<const Key, MdnsRecordChangedCallback*> key_value(key, callback);
  if (std::find(callbacks.first, callbacks.second, key_value) ==
      callbacks.second) {
    callbacks_.emplace(std::move(key_value));
  }

  if (questions_.find(key) == questions_.end()) {
    questions_.emplace(key,
                       CreateTracker(MdnsQuestion(name, dns_type, dns_class,
                                                  ResponseType::kMulticast)));
  }
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType dns_type,
                            DnsClass dns_class,
                            MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback);

  const Key key(name, dns_type, dns_class);

  auto callbacks = callbacks_.equal_range(key);
  std::pair<const Key, MdnsRecordChangedCallback*> key_value(key, callback);
  auto find_result = std::find(callbacks.first, callbacks.second, key_value);
  if (find_result != callbacks.second) {
    if (std::next(callbacks.first) == callbacks.second) {
      // Key matches a single callback entry that we are about to remove.
      // Remove the question with the same key as there are no clients
      // interested in answers to this question.
      questions_.erase(key);
    }
    callbacks_.erase(find_result);
  }
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Response);

  // TODO(yakimakha): Check authority records
  // TODO(yakimakha): Handle questions with type ANY and class ANY
  ProcessRecords(message.answers());
  ProcessRecords(message.additional_records());
}

void MdnsQuerier::ProcessRecords(const std::vector<MdnsRecord>& records) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  for (const MdnsRecord& record : records) {
    switch (record.record_type()) {
      case RecordType::kShared: {
        ProcessSharedRecord(record);
        break;
      }
      case RecordType::kUnique: {
        ProcessUniqueRecord(record);
        break;
      }
    }
  }
}

void MdnsQuerier::ProcessSharedRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(record.record_type() == RecordType::kShared);

  const Key key(record.name(), record.dns_type(), record.dns_class());
  auto records = records_.equal_range(key);

  bool is_updated = false;
  for (auto entry = records.first; entry != records.second; ++entry) {
    MdnsRecordTracker* tracker = entry->second.get();
    if (tracker->record().rdata() == record.rdata()) {
      // Already have this shared record, update the existing one.
      // This is a TTL only update since we've already checked that RDATA
      // matches. No notification is necessary on a TTL only update.
      is_updated = true;
      tracker->Update(record);
      break;
    }
  }
  if (!is_updated) {
    // Have never before seen this shared record, insert a new one.
    records_.emplace(key, CreateTracker(record));
    ProcessQuestions(record, RecordChangedEvent::kCreated);
  }
}

void MdnsQuerier::ProcessUniqueRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(record.record_type() == RecordType::kUnique);

  const Key key(record.name(), record.dns_type(), record.dns_class());
  auto records = records_.equal_range(key);

  if (records.first == records.second) {
    // Have not seen any records with this key before.
    records_.emplace(key, CreateTracker(record));
    ProcessQuestions(record, RecordChangedEvent::kCreated);
  } else if (std::next(records.first) == records.second) {
    // There's only one record with this key.
    // If RDATA on the record is different, notify that the record has been
    // updated, otherwise simply reset TTL.
    MdnsRecordTracker* tracker = records.first->second.get();
    bool is_updated = (record.rdata() != tracker->record().rdata());
    // TODO: notify if update is successful only? Can't actually fail here
    tracker->Update(record);
    if (is_updated) {
      ProcessQuestions(record, RecordChangedEvent::kUpdated);
    }
  } else {
    // Multiple records with the same key. Erase all record with non-matching
    // RDATA. Update the record with the matching RDATA if it exists, otherwise
    // insert a new record.
    bool is_updated = false;
    for (auto entry = records.first; entry != records.second; ++entry) {
      MdnsRecordTracker* tracker = entry->second.get();
      if (tracker->record().rdata() == record.rdata()) {
        // This is a TTL only update since we've already checked that RDATA
        // matches. No notification is necessary on a TTL only update.
        is_updated = true;
        tracker->Update(record);
      } else {
        // Mark records for expiration
        tracker->Expire();
      }
    }
    if (!is_updated) {
      // Did not find an existing record to update,
      records_.emplace(key, CreateTracker(record));
      ProcessQuestions(record, RecordChangedEvent::kCreated);
    }
  }
}

void MdnsQuerier::ProcessQuestions(const MdnsRecord& record,
                                   RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  ProcessQuestion(Key(record.name(), record.dns_type(), record.dns_class()),
                  record, event);
  ProcessQuestion(Key(record.name(), DnsType::kANY, record.dns_class()), record,
                  event);
  ProcessQuestion(Key(record.name(), record.dns_type(), DnsClass::kANY), record,
                  event);
  ProcessQuestion(Key(record.name(), DnsType::kANY, DnsClass::kANY), record,
                  event);
}

void MdnsQuerier::ProcessQuestion(Key key,
                                  const MdnsRecord& record,
                                  RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto callbacks = callbacks_.equal_range(key);
  for (auto entry = callbacks.first; entry != callbacks.second; ++entry) {
    entry->second->OnRecordChanged(record, event);
  }

  // TODO(yakimakha): update question matching the key with created or updated
  // known answer
}

void MdnsQuerier::OnRecordExpired(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  const Key key(record.name(), record.dns_type(), record.dns_class());
  auto callbacks = callbacks_.equal_range(key);

  for (auto entry = callbacks.first; entry != callbacks.second; ++entry) {
    MdnsRecordChangedCallback* callback = entry->second;
    callback->OnRecordChanged(record, RecordChangedEvent::kDeleted);
  }

  // TODO(yakimakha): update question matching the key with the deleted known
  // answer
}

std::unique_ptr<MdnsQuestionTracker> MdnsQuerier::CreateTracker(
    MdnsQuestion question) {
  return std::make_unique<MdnsQuestionTracker>(
      std::move(question), sender_, task_runner_, now_function_, random_delay_);
}

std::unique_ptr<MdnsRecordTracker> MdnsQuerier::CreateTracker(
    MdnsRecord record) {
  return std::make_unique<MdnsRecordTracker>(
      std::move(record), sender_, task_runner_, now_function_, random_delay_,
      [this](const MdnsRecord& record) {
        MdnsQuerier::OnRecordExpired(record);
      });
}

}  // namespace mdns
}  // namespace cast
