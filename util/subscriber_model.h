// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef UTIL_SUBSCRIBER_MODEL_H_
#define UTIL_SUBSCRIBER_MODEL_H_

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"

namespace openscreen {

// Helper to allow for faster response of the UnsubscribeAndBlock call inside
// SubscriberModel.
class SubscriberModelCancellationToken {
 public:
  virtual ~SubscriberModelCancellationToken() = default;

  // Cancels any currently running operations. The callee is expected to
  // 'quickly' complete any ongoing operation and call
  // SubscriberModel::ApplyPendingChanges() after recieving this call. This
  // call is expected to be thread-safe.
  virtual void InterruptOngoingOperations();
};

// This class is intended to act as a thread-safe subscriber model, where many
// threads can add or remove subscribers and a single thread (Denoted throughout
// this class as the 'primary thread') will read from the subscribers_ vector
template <class SubscriberType, class Equals = std::equal_to<SubscriberType>>
class SubscriberModel {
 public:
  SubscriberModel(
      SubscriberModelCancellationToken* cancellation_token = nullptr)
      : cancellation_token_(cancellation_token) {}

  // Subscribes the provided subscriber, overwriting any curent subscriber which
  // is equal according to the template Equals type.
  void Subscribe(const SubscriberType& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kAdd);
    empty_subscribers_block_.notify_all();
  }

  // Unsubscribes the provided subscriber.
  // NOTE: If there are any ongoing operations using the subscriber, their
  // behavior is undefined.
  void Unsubscribe(const SubscriberType& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kRemove);
  }

  // Unsubscribes the provided subscriber and blocks until the pending
  // unsubscribe call is applied.
  void UnsubscribeAndBlock(const SubscriberType& subscriber) {
    std::unique_lock<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kRemove);
    if (cancellation_token_) {
      cancellation_token_->InterruptOngoingOperations();
    }
    unsubscribe_block_.wait(lock, [this, &subscriber]() {
      return !this->IsSubscriberChangePending(subscriber);
    });
  }

  // Blocks the current thread until a subscriber is present. This method should
  // only be called from the primary thread.
  void BlockUntilSubscribersPresent() {
    if (!subscribers_.empty()) {
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    empty_subscribers_block_.wait(
        lock, [this]() { return !subscriber_changes_.empty(); });
  }

  // Applies any pending subscribe and unsubscribe calls. This method should
  // only be called from the primary thread.
  void ApplyPendingChanges() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (const SubscriberOperationInfo& operation : subscriber_changes_) {
      const SubscriberType& subscriber = operation.first;
      const auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
                                   [&subscriber](SubscriberType other) {
                                     return Equals()(subscriber, other);
                                   });
      switch (operation.second) {
        case SubscriberOperation::kAdd:
          if (it != subscribers_.end()) {
            subscribers_.erase(it);
          }
          subscribers_.push_back(subscriber);
          break;
        case SubscriberOperation::kRemove:
          if (it != subscribers_.end()) {
            subscribers_.erase(it);
          }
          break;
      }
    }
    subscriber_changes_.clear();
    unsubscribe_block_.notify_all();
  }

  // Returns the set of subscribers after applying any pending subscriber or
  // unsubscribe calls. This method should only be called from the primary
  // thread.
  // NOTE: There must be a nontrivial delay between successive calls to this
  // method, or pending Subscribe and Unsubscribe calls may not complete.
  const std::vector<SubscriberType>& subscribers() {
    ApplyPendingChanges();
    return subscribers_;
  }

 private:
  // This method expects that the caller already has ownership of the mutex_.
  bool IsSubscriberChangePending(const SubscriberType& subscriber) {
    return std::find_if(subscriber_changes_.begin(), subscriber_changes_.end(),
                        [&subscriber](SubscriberOperationInfo operation) {
                          return Equals()(subscriber, operation.first);
                        }) != subscriber_changes_.end();
  }

  // Set of provider operations that may be performed by outside callers.
  enum class SubscriberOperation { kAdd, kRemove };

  using SubscriberOperationInfo =
      std::pair<SubscriberType, SubscriberOperation>;

  SubscriberModelCancellationToken* cancellation_token_;

  // Guards against concurrent access to subscriber_changes_.
  std::mutex mutex_;

  // Set of all operations to perform on the subscribers_ vector when a valid
  // time to do so arises.
  std::vector<SubscriberOperationInfo> subscriber_changes_ GUARDED_BY(mutex_);

  // Subscribers currently being used by this object.
  std::vector<SubscriberType> subscribers_;

  // Allows StopProviding operations to block until completed.
  std::condition_variable unsubscribe_block_;

  // Allows for blocking of the thread when there are no watched Providers. This
  // prevents the CPU from wasting cycles when no useful work can be done.
  std::condition_variable empty_subscribers_block_;
};

template <class Key, class T>
class MapKeyEquals {
 public:
  bool operator()(const std::pair<Key, T>& lhs, const std::pair<Key, T>& rhs) {
    return lhs.first == rhs.first;
  }
};

// Extension of the above class to support subscribing and unsubscribing with
// (key, value) pairs.
template <class Key, class T>
class MapSubscriberModel
    : public SubscriberModel<std::pair<Key, T>, MapKeyEquals<Key, T>> {
 public:
  MapSubscriberModel(
      T default_value,
      SubscriberModelCancellationToken* cancellation_token = nullptr)
      : Parent(cancellation_token), default_value_(std::move(default_value)) {}

  void Subscribe(const Key& key, const T& value) {
    Parent::Subscribe(std::make_pair(key, value));
  }

  void Unsubscribe(const Key& key) {
    Parent::Unsubscribe(std::make_pair(key, default_value_));
  }

  void UnsubscribeAndBlock(const Key& key) {
    Parent::UnsubscribeAndBlock(std::make_pair(key, default_value_));
  }

 private:
  using Parent = SubscriberModel<std::pair<Key, T>, MapKeyEquals<Key, T>>;

  const T default_value_;
};

}  // namespace openscreen

#endif  // UTIL_SUBSCRIBER_MODEL_H_
