// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef UTIL_SUBSCRIBER_MODEL_H_
#define UTIL_SUBSCRIBER_MODEL_H_

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"

namespace openscreen {

// This class is intended to act as a thread-safe subscriber model, where many
// "producer" threads can add or remove subscribers and a single "consumer"
// thread will read from the set of subscribers.
//   SubscriberType = Data type to Subscribe and Unsubscribe with this model.
// TODO(issues/74): Enforce threading assumptions currently documented in
// comments.
template <typename SubscriberType>
class SubscriberModel {
 public:
  // Cancels any currently running operations at the soonest safe time to do so.
  // The provided function acts as a way for the SubscriberModel to exit ongoing
  // operations. When called, the callee is expected to 'quickly' complete any
  // ongoing operation and call SubscriberModel::ApplyPendingChanges(). This
  // call is expected to be thread-safe.
  SubscriberModel(std::function<void()> cancellation_function = nullptr)
      : cancellation_function_(cancellation_function) {}

  // Subscribes the provided subscriber, overwriting any current subscriber
  // which is equal as defined by == operation for SubscriberType.
  void Subscribe(const SubscriberType& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    has_pending_changes_ = true;
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kAdd);
  }

  // Unsubscribes the provided subscriber.
  void Unsubscribe(const SubscriberType& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    has_pending_changes_ = true;
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kRemove);
  }

  // Unsubscribes the provided subscriber and blocks until the pending
  // unsubscribe call has been applied.
  void UnsubscribeBlocking(const SubscriberType& subscriber) {
    std::unique_lock<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kRemove);
    has_pending_changes_ = true;
    if (cancellation_function_) {
      cancellation_function_();
    }
    unsubscribe_block_.wait(lock, [this, &subscriber]() {
      return !this->IsSubscriberChangePending(subscriber);
    });
  }

  // Applies any pending subscribe and unsubscribe calls. This method should
  // only be called from the consumer thread.
  void ApplyPendingChanges() {
    if (!has_pending_changes_) {
      return;
    }

    std::vector<SubscriberOperationInfo> subscriber_changes_local;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      subscriber_changes_.swap(subscriber_changes_local);
      unsubscribe_block_.notify_all();
      has_pending_changes_ = false;
    }

    for (const SubscriberOperationInfo& operation : subscriber_changes_local) {
      const SubscriberType& subscriber = operation.first;
      const auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
                                   [&subscriber](const SubscriberType& other) {
                                     return subscriber == other;
                                   });
      switch (operation.second) {
        case SubscriberOperation::kRemove:
          if (it != subscribers_.end()) {
            subscribers_.erase(it);
          }
          break;
        case SubscriberOperation::kAdd:
          if (it != subscribers_.end()) {
            *it = subscriber;
          } else {
            subscribers_.push_back(subscriber);
          }
          break;
      }
    }
  }

  // Returns the set of subscribers after applying any pending subscriber or
  // unsubscribe calls. This method should only be called from the consumer
  // thread.
  const std::vector<SubscriberType>& subscribers() { return subscribers_; }

 private:
  bool IsSubscriberChangePending(const SubscriberType& subscriber)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return std::find_if(
               subscriber_changes_.begin(), subscriber_changes_.end(),
               [&subscriber](const SubscriberOperationInfo& operation) {
                 return subscriber == operation.first;
               }) != subscriber_changes_.end();
  }

  // Set of provider operations that may be performed by outside callers.
  enum class SubscriberOperation { kAdd, kRemove };

  std::atomic_bool has_pending_changes_{false};

  using SubscriberOperationInfo =
      std::pair<SubscriberType, SubscriberOperation>;

  std::function<void()> cancellation_function_;

  // Guards against concurrent access to subscriber_changes_.
  std::mutex mutex_;

  // Set of all operations to perform on the subscribers_ vector when a valid
  // time to do so arises.
  std::vector<SubscriberOperationInfo> subscriber_changes_ GUARDED_BY(mutex_);

  // Subscribers currently being used by this object. This object should only
  // be accessed on the consumer thread.
  std::vector<SubscriberType> subscribers_;

  // Allows UnsubscribeBlocking(...) operations to block until completed.
  std::condition_variable unsubscribe_block_;
};

}  // namespace openscreen

#endif  // UTIL_SUBSCRIBER_MODEL_H_
