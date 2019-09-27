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

// Helper to allow for faster response of the UnsubscribeBlocking call inside
// SubscriberModel.
class SubscriberModelCancellationToken {
 public:
  virtual ~SubscriberModelCancellationToken() = default;

  // Cancels any currently running operations at the soonest safe time to do so.
  // The callee is expected to 'quickly' complete any ongoing operation and call
  // SubscriberModel::ApplyPendingChanges() after receiving this call. This
  // call is expected to be thread-safe.
  virtual void CancelRunningOperations() = 0;
};

// This class is intended to act as a thread-safe subscriber model, where many
// threads can add or remove subscribers and a single thread (denoted throughout
// this class as the 'primary thread') will read from the subscribers_ vector.
//   SubscriberType = Data type to Subscribe and Unsubscribe with this model
//   Equals = Operation to use for comparing two SubscriberTypes for Subscribe,
//            Unsubscribe, and UnsubscribeBlocking operations.
template <class SubscriberType, class Equals = std::equal_to<SubscriberType>>
class SubscriberModel {
 public:
  SubscriberModel(
      SubscriberModelCancellationToken* cancellation_token = nullptr)
      : cancellation_token_(cancellation_token) {}

  // Subscribes the provided subscriber, overwriting any current subscriber
  // which is equal as defined by the template Equals type.
  void Subscribe(const SubscriberType& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kAdd);
    empty_subscribers_block_.notify_all();
  }

  // Unsubscribes the provided subscriber.
  void Unsubscribe(const SubscriberType& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kRemove);
  }

  // Unsubscribes the provided subscriber and blocks until the pending
  // unsubscribe call has been applied.
  void UnsubscribeBlocking(const SubscriberType& subscriber) {
    std::unique_lock<std::mutex> lock(mutex_);
    subscriber_changes_.emplace_back(subscriber, SubscriberOperation::kRemove);
    if (cancellation_token_) {
      cancellation_token_->CancelRunningOperations();
    }
    unsubscribe_block_.wait(lock, [this, &subscriber]() {
      return !this->IsSubscriberChangePending(subscriber);
    });
  }

  // Blocks the current thread until a subscriber is present. This can allow
  // users of this class to avoid wasting CPU cycles when no subscribers are
  // present. This method should only be called from the primary thread.
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
  // NOTE: There must be a nontrivial delay between successive calls to this
  // method. Else, pending Subscribe and Unsubscribe calls may block
  // indefinitely due to thread starvation.
  void ApplyPendingChanges() {
    std::vector<SubscriberOperationInfo> subscriber_changes_local;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      subscriber_changes_.swap(subscriber_changes_local);
      unsubscribe_block_.notify_all();
    }

    for (const SubscriberOperationInfo& operation : subscriber_changes_local) {
      const SubscriberType& subscriber = operation.first;
      const auto it = std::find_if(subscribers_.begin(), subscribers_.end(),
                                   [&subscriber](SubscriberType other) {
                                     return Equals()(subscriber, other);
                                   });
      if (it != subscribers_.end()) {
        subscribers_.erase(it);
      }
      if (operation.second == SubscriberOperation::kAdd) {
        subscribers_.push_back(subscriber);
      }
    }
  }

  // Returns the set of subscribers after applying any pending subscriber or
  // unsubscribe calls. This method should only be called from the primary
  // thread.
  const std::vector<SubscriberType>& subscribers() { return subscribers_; }

 private:
  // This method expects that the caller already has ownership of mutex_
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

  // Subscribers currently being used by this object. This object should only
  // be accessed on the main thread.
  std::vector<SubscriberType> subscribers_;

  // Allows StopProviding operations to block until completed.
  std::condition_variable unsubscribe_block_;

  // Allows for blocking of the thread when there are no watched Providers. This
  // prevents the CPU from wasting cycles when no useful work can be done.
  std::condition_variable empty_subscribers_block_;
};

// This class is an implementation of Equals to compare Map keys, for use with
// the above SubscriberModel class.
//   Key = Key type for the map.
//   T = value type for the map.
//   KeyEquals = Operation to use for comparing two Key instances.
template <class Key, class T, class KeyEquals = std::equal_to<Key>>
class MapKeyEquals {
 public:
  bool operator()(const std::pair<Key, T>& lhs, const std::pair<Key, T>& rhs) {
    return lhs.first == rhs.first;
  }
};

// Extension of the above class to support subscribing and unsubscribing with
// (key, value) pairs.
//   Key = Key type for the map.
//   T = value type for the map.
//   KeyEquals = Operation to use for comparing two Key instances.
template <class Key, class T, class KeyEquals = std::equal_to<Key>>
class MapSubscriberModel
    : public SubscriberModel<std::pair<Key, T>,
                             MapKeyEquals<Key, T, KeyEquals>> {
 public:
  MapSubscriberModel(
      T default_value,
      SubscriberModelCancellationToken* cancellation_token = nullptr)
      : Parent(cancellation_token), default_value_(std::move(default_value)) {}

  // Subscribes the provided subscriber, overwriting any current subscriber
  // with key equal to the provided value, as defined by the KeyEquals template
  // parameter.
  void Subscribe(const Key& key, const T& value) {
    Parent::Subscribe(std::make_pair(key, value));
  }

  // Unsubscribes any subscriber with key equal to the provided one, as defined
  // by the KeyEquals template parameter.
  void Unsubscribe(const Key& key) {
    Parent::Unsubscribe(std::make_pair(key, default_value_));
  }

  // Unsubscribes any subscriber with key equal to the provided one, as defined
  // by the KeyEquals template parameter, and blocks until the pending
  // unsubscribe call has been applied.
  void UnsubscribeBlocking(const Key& key) {
    Parent::UnsubscribeBlocking(std::make_pair(key, default_value_));
  }

 private:
  using Parent = SubscriberModel<std::pair<Key, T>, MapKeyEquals<Key, T>>;

  const T default_value_;
};

}  // namespace openscreen

#endif  // UTIL_SUBSCRIBER_MODEL_H_
