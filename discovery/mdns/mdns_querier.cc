// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_querier.h"

#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_sender.h"
#include "discovery/mdns/mdns_trackers.h"

namespace openscreen {
namespace discovery {

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

  // Add a new callback if haven't seen it before
  auto callbacks = callbacks_.equal_range(name);
  for (auto entry = callbacks.first; entry != callbacks.second; ++entry) {
    const CallbackInfo& callback_info = entry->second;
    if (dns_type == callback_info.dns_type &&
        dns_class == callback_info.dns_class &&
        callback == callback_info.callback) {
      // Already have this callback
      return;
    }
  }
  callbacks_.emplace(name, CallbackInfo{callback, dns_type, dns_class});

  // Notify the new callback with previously cached records
  auto records = records_.equal_range(name);
  for (auto entry = records.first; entry != records.second; ++entry) {
    const MdnsRecord& record = entry->second->record();
    if ((dns_type == DnsType::kANY || dns_type == record.dns_type()) &&
        (dns_class == DnsClass::kANY || dns_class == record.dns_class())) {
      callback->OnRecordChanged(record, RecordChangedEvent::kCreated);
    }
  }

  // Add a new question if haven't seen it before
  auto questions = questions_.equal_range(name);
  for (auto entry = questions.first; entry != questions.second; ++entry) {
    const MdnsQuestion& tracked_question = entry->second->question();
    if (dns_type == tracked_question.dns_type() &&
        dns_class == tracked_question.dns_class()) {
      // Already have this question
      return;
    }
  }
  AddQuestion(
      MdnsQuestion(name, dns_type, dns_class, ResponseType::kMulticast));
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType dns_type,
                            DnsClass dns_class,
                            MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback);

  // Find and remove the callback.
  int callbacks_for_key = 0;
  auto callbacks = callbacks_.equal_range(name);
  for (auto entry = callbacks.first; entry != callbacks.second;) {
    const CallbackInfo& callback_info = entry->second;
    if (dns_type == callback_info.dns_type &&
        dns_class == callback_info.dns_class) {
      if (callback == callback_info.callback) {
        entry = callbacks_.erase(entry);
      } else {
        ++callbacks_for_key;
        ++entry;
      }
    }
  }

  // Exit if there are still callbacks registered for DomainName + DnsType +
  // DnsClass
  if (callbacks_for_key > 0) {
    return;
  }

  // Find and delete a question that does not have any associated callbacks
  auto questions = questions_.equal_range(name);
  for (auto entry = questions.first; entry != questions.second; ++entry) {
    const MdnsQuestion& tracked_question = entry->second->question();
    if (dns_type == tracked_question.dns_type() &&
        dns_class == tracked_question.dns_class()) {
      questions_.erase(entry);
      return;
    }
  }

  // TODO(yakimakha): Find and delete all records that no longer answer any
  // questions, if a question was deleted.
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Response);

  // TODO(yakimakha): Drop answers and additional records if answer records do
  // not answer any existing questions
  // TODO(yakimakha): Check authority records
  ProcessRecords(message.answers());
  ProcessRecords(message.additional_records());
}

void MdnsQuerier::OnRecordExpired(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  ProcessCallbacks(record, RecordChangedEvent::kDeleted);

  auto records = records_.equal_range(record.name());
  for (auto entry = records.first; entry != records.second; ++entry) {
    MdnsRecordTracker* tracker = entry->second.get();
    const MdnsRecord& tracked_record = tracker->record();
    if (record.dns_type() == tracked_record.dns_type() &&
        record.dns_class() == tracked_record.dns_class() &&
        record.rdata() == tracked_record.rdata()) {
      records_.erase(entry);
      break;
    }
  }
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

  auto records = records_.equal_range(record.name());
  for (auto entry = records.first; entry != records.second; ++entry) {
    MdnsRecordTracker* tracker = entry->second.get();
    const MdnsRecord& tracked_record = tracker->record();
    if (record.dns_type() == tracked_record.dns_type() &&
        record.dns_class() == tracked_record.dns_class() &&
        record.rdata() == tracked_record.rdata()) {
      // Already have this shared record, update the existing one.
      // This is a TTL only update since we've already checked that RDATA
      // matches. No notification is necessary on a TTL only update.
      tracker->Update(record);
      return;
    }
  }
  // Have never before seen this shared record, insert a new one.
  AddRecord(record);
  ProcessCallbacks(record, RecordChangedEvent::kCreated);
}

void MdnsQuerier::ProcessUniqueRecord(const MdnsRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(record.record_type() == RecordType::kUnique);

  int records_for_key = 0;
  auto records = records_.equal_range(record.name());
  for (auto entry = records.first; entry != records.second; ++entry) {
    const MdnsRecord& tracked_record = entry->second->record();
    if (record.dns_type() == tracked_record.dns_type() &&
        record.dns_class() == tracked_record.dns_class()) {
      ++records_for_key;
    }
  }

  if (records_for_key == 0) {
    // Have not seen any records with this key before.
    AddRecord(record);
    ProcessCallbacks(record, RecordChangedEvent::kCreated);
  } else if (records_for_key == 1) {
    // There's only one record with this key.
    // If RDATA on the record is different, notify that the record has been
    // updated, otherwise simply reset TTL.
    MdnsRecordTracker* tracker = records.first->second.get();
    bool is_updated = (record.rdata() != tracker->record().rdata());
    tracker->Update(record);
    if (is_updated) {
      ProcessCallbacks(record, RecordChangedEvent::kUpdated);
    }
  } else {
    // Multiple records with the same key. Expire all record with non-matching
    // RDATA. Update the record with the matching RDATA if it exists, otherwise
    // insert a new record.
    bool is_updated = false;
    for (auto entry = records.first; entry != records.second; ++entry) {
      MdnsRecordTracker* tracker = entry->second.get();
      const MdnsRecord& tracked_record = tracker->record();
      if (record.dns_type() == tracked_record.dns_type() &&
          record.dns_class() == tracked_record.dns_class()) {
        if (record.rdata() == tracked_record.rdata()) {
          // This is a TTL only update since we've already checked that RDATA
          // matches. No notification is necessary on a TTL only update.
          is_updated = true;
          tracker->Update(record);
        } else {
          tracker->ExpireSoon();
        }
      }
    }

    if (!is_updated) {
      // Did not find an existing record to update
      AddRecord(record);
      ProcessCallbacks(record, RecordChangedEvent::kCreated);
    }
  }
}

void MdnsQuerier::ProcessCallbacks(const MdnsRecord& record,
                                   RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto callbacks = callbacks_.equal_range(record.name());
  for (auto entry = callbacks.first; entry != callbacks.second; ++entry) {
    const CallbackInfo& callback_info = entry->second;
    if ((callback_info.dns_type == DnsType::kANY ||
         record.dns_type() == callback_info.dns_type) &&
        (callback_info.dns_class == DnsClass::kANY ||
         record.dns_class() == callback_info.dns_class)) {
      callback_info.callback->OnRecordChanged(record, event);
    }
  }

  // TODO(yakimakha): Update known answers for relevant questions.
}

void MdnsQuerier::AddQuestion(const MdnsQuestion& question) {
  questions_.emplace(question.name(),
                     std::make_unique<MdnsQuestionTracker>(
                         std::move(question), sender_, task_runner_,
                         now_function_, random_delay_));
}

void MdnsQuerier::AddRecord(const MdnsRecord& record) {
  auto expiration_callback = [this](const MdnsRecord& record) {
    MdnsQuerier::OnRecordExpired(record);
  };
  records_.emplace(record.name(),
                   std::make_unique<MdnsRecordTracker>(
                       std::move(record), sender_, task_runner_, now_function_,
                       random_delay_, expiration_callback));
}

}  // namespace discovery
}  // namespace openscreen
