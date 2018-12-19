// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/presentation/url_availability_listener.h"

#include <algorithm>

#include "api/public/network_service_manager.h"
#include "base/make_unique.h"
#include "platform/api/logging.h"
// TODO(btolsch): Probably need a better way to get EOF error.
#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen {
namespace presentation {

static constexpr uint32_t kWatchDurationSeconds = 20;

UrlAvailabilityListener::UrlAvailabilityListener(
    const std::vector<ScreenInfo>& screens) {
  for (const auto& info : screens) {
    clients_.emplace(info.screen_id, MakeUnique<AvailabilityClient>(
                                         this, info.screen_id, info.endpoint));
  }
}

UrlAvailabilityListener::~UrlAvailabilityListener() = default;

void UrlAvailabilityListener::AddObserver(const std::vector<std::string>& urls,
                                          platform::TimeDelta now,
                                          ScreenObserver* observer) {
  for (const auto& url : urls) {
    observers_by_url_[url].push_back(observer);
  }
  for (auto& client : clients_) {
    client.second->OnObserverRequest(urls, now, observer);
  }
}

void UrlAvailabilityListener::RemoveObserver(
    const std::vector<std::string>& urls,
    ScreenObserver* observer) {
  std::set<std::string> dropped_urls;
  for (const auto& url : urls) {
    auto observer_entry = observers_by_url_.find(url);
    auto& observers = observer_entry->second;
    observers.erase(std::remove(observers.begin(), observers.end(), observer),
                    observers.end());
    if (observers.empty()) {
      dropped_urls.emplace(std::move(observer_entry->first));
      observers_by_url_.erase(observer_entry);
      for (auto& client : clients_) {
        client.second->current_availabilities.erase(url);
      }
    }
  }

  if (!dropped_urls.empty()) {
    for (auto& client : clients_)
      client.second->CancelSubsetWatches(dropped_urls);
  }
}

void UrlAvailabilityListener::OnScreenAdded(const ScreenInfo& info,
                                            platform::TimeDelta now) {
  RefreshWatches(now);
  auto result = clients_.emplace(
      info.screen_id,
      MakeUnique<AvailabilityClient>(this, info.screen_id, info.endpoint));
  std::unique_ptr<AvailabilityClient>& client = result.first->second;
  client->last_update_time = now;
  if (!observers_by_url_.empty()) {
    std::vector<std::string> urls;
    urls.reserve(observers_by_url_.size());
    for (const auto& url : observers_by_url_) {
      urls.push_back(url.first);
    }
    client->StartOrQueueRequest(std::move(urls));
  }
}

void UrlAvailabilityListener::OnScreenChanged(const ScreenInfo& info) {}

void UrlAvailabilityListener::OnScreenRemoved(const ScreenInfo& info) {
  auto client_entry = clients_.find(info.screen_id);
  if (client_entry != clients_.end()) {
    client_entry->second->OnScreenRemoved();
    clients_.erase(client_entry);
  }
}

void UrlAvailabilityListener::OnAllScreensRemoved() {
  for (auto& client : clients_)
    client.second->OnScreenRemoved();
  clients_.clear();
}

void UrlAvailabilityListener::RefreshWatches(platform::TimeDelta now) {
  for (auto& client : clients_)
    client.second->RefreshWatches(now);
}

UrlAvailabilityListener::AvailabilityClient::AvailabilityClient(
    UrlAvailabilityListener* listener,
    const std::string& screen_id,
    const IPEndpoint& endpoint)
    : listener(listener),
      screen_id(screen_id),
      connect_request(
          NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
              endpoint,
              this)) {}

UrlAvailabilityListener::AvailabilityClient::~AvailabilityClient() = default;

void UrlAvailabilityListener::AvailabilityClient::OnObserverRequest(
    const std::vector<std::string>& urls,
    platform::TimeDelta now,
    ScreenObserver* observer) {
  std::vector<std::string> unmatched_urls;
  for (const auto& url : urls) {
    auto availability_entry = current_availabilities.find(url);
    if (availability_entry == current_availabilities.end()) {
      unmatched_urls.emplace_back(url);
      continue;
    }

    if (observer) {
      switch (availability_entry->second) {
        case msgs::kCompatible:
          observer->OnScreensAvailable(url, screen_id);
          break;
        case msgs::kNotCompatible:
        case msgs::kNotValid:
          observer->OnScreensUnavailable(url, screen_id);
          break;
      }
    }
  }
  RefreshWatches(now);
  if (!unmatched_urls.empty()) {
    StartOrQueueRequest(std::move(unmatched_urls));
  }
}

void UrlAvailabilityListener::AvailabilityClient::StartOrQueueRequest(
    std::vector<std::string> urls) {
  uint64_t request_id = next_request_id++;
  if (stream) {
    if (StartRequest(request_id, urls)) {
      requests.emplace(request_id, std::move(urls));
    }
  } else {
    requests.emplace(request_id, std::move(urls));
  }
}

bool UrlAvailabilityListener::AvailabilityClient::StartRequest(
    uint64_t request_id,
    const std::vector<std::string>& urls) {
  uint64_t watch_id = next_watch_id++;
  msgs::PresentationUrlAvailabilityRequest cbor_request;
  cbor_request.request_id = request_id;
  cbor_request.urls = urls;
  cbor_request.watch_id = watch_id;

  msgs::CborEncodeBuffer buffer;
  if (msgs::EncodePresentationUrlAvailabilityRequest(cbor_request, &buffer)) {
    OSP_VLOG(1) << "writing presentation-url-availability-request";
    stream->Write(buffer.data(), buffer.size());
    availability_watches.emplace(
        watch_id,
        AvailabilityWatch{
            platform::TimeDelta::FromSeconds(kWatchDurationSeconds), urls});
    if (!event_watch) {
      event_watch =
          NetworkServiceManager::Get()
              ->GetProtocolConnectionClient()
              ->message_demuxer()
              ->WatchMessageType(endpoint_id,
                                 msgs::Type::kPresentationUrlAvailabilityEvent,
                                 this);
    }
    if (!response_watch) {
      response_watch =
          NetworkServiceManager::Get()
              ->GetProtocolConnectionClient()
              ->message_demuxer()
              ->WatchMessageType(
                  endpoint_id, msgs::Type::kPresentationUrlAvailabilityResponse,
                  this);
    }
    return true;
  }
  return false;
}

void UrlAvailabilityListener::AvailabilityClient::RefreshWatches(
    platform::TimeDelta now) {
  platform::TimeDelta update_delta = now - last_update_time;

  std::vector<std::vector<std::string>> new_requests;
  for (auto entry = availability_watches.begin();
       entry != availability_watches.end();) {
    if (update_delta > entry->second.remaining_time) {
      new_requests.emplace_back(std::move(entry->second.urls));
      entry = availability_watches.erase(entry);
    } else {
      entry->second.remaining_time -= update_delta;
      ++entry;
    }
  }

  last_update_time = now;
  for (auto& request : new_requests)
    StartOrQueueRequest(std::move(request));
}

void UrlAvailabilityListener::AvailabilityClient::UpdateAvailabilities(
    const std::vector<std::string>& urls,
    const std::vector<msgs::PresentationUrlAvailability>& availabilities) {
  auto availability_it = availabilities.begin();
  for (const auto& url : urls) {
    auto entry = current_availabilities.find(url);
    if (entry == current_availabilities.end()) {
      current_availabilities.emplace(url, *availability_it);
    } else if (entry->second != *availability_it) {
      entry->second = *availability_it;
    } else {
      ++availability_it;
      continue;
    }
    switch (*availability_it) {
      case msgs::kCompatible:
        for (auto* observer : listener->observers_by_url_[url])
          observer->OnScreensAvailable(url, screen_id);
        break;
      case msgs::kNotCompatible:
      case msgs::kNotValid:
        for (auto* observer : listener->observers_by_url_[url])
          observer->OnScreensUnavailable(url, screen_id);
        break;
      default:
        break;
    }
    ++availability_it;
  }
}

void UrlAvailabilityListener::AvailabilityClient::CancelSubsetWatches(
    const std::set<std::string>& urls) {
  for (auto entry = availability_watches.begin();
       entry != availability_watches.end();) {
    AvailabilityWatch& watch = entry->second;
    std::set<std::string> watched_urls(watch.urls.begin(), watch.urls.end());
    if (std::includes(urls.begin(), urls.end(), watched_urls.begin(),
                      watched_urls.end())) {
      entry = availability_watches.erase(entry);
    } else {
      ++entry;
    }
  }
}

void UrlAvailabilityListener::AvailabilityClient::OnScreenRemoved() {
  for (const auto& availability : current_availabilities) {
    if (availability.second == msgs::kCompatible) {
      const std::string& url = availability.first;
      for (auto& observer : listener->observers_by_url_[url])
        observer->OnScreensUnavailable(url, screen_id);
    }
  }
}

void UrlAvailabilityListener::AvailabilityClient::OnConnectionOpened(
    uint64_t request_id,
    std::unique_ptr<ProtocolConnection>&& connection) {
  connect_request.MarkComplete();
  endpoint_id = connection->endpoint_id();
  stream = std::move(connection);
  for (auto entry = requests.begin(); entry != requests.end();) {
    if (StartRequest(entry->first, entry->second)) {
      ++entry;
    } else {
      entry = requests.erase(entry);
    }
  }
}

void UrlAvailabilityListener::AvailabilityClient::OnConnectionFailed(
    uint64_t request_id) {
  connect_request.MarkComplete();
  std::string id = std::move(screen_id);
  listener->clients_.erase(id);
}

ErrorOr<size_t> UrlAvailabilityListener::AvailabilityClient::OnStreamMessage(
    uint64_t endpoint_id,
    uint64_t connection_id,
    msgs::Type message_type,
    const uint8_t* buffer,
    size_t buffer_size,
    platform::TimeDelta now) {
  ErrorOr<size_t> result = HandleMessage(message_type, buffer, buffer_size);
  RefreshWatches(now);
  return result;
}

ErrorOr<size_t> UrlAvailabilityListener::AvailabilityClient::HandleMessage(
    msgs::Type message_type,
    const uint8_t* buffer,
    size_t buffer_size) {
  switch (message_type) {
    case msgs::Type::kPresentationUrlAvailabilityResponse: {
      msgs::PresentationUrlAvailabilityResponse response;
      ssize_t result = msgs::DecodePresentationUrlAvailabilityResponse(
          buffer, buffer_size, &response);
      if (result < 0) {
        if (result == -CborErrorUnexpectedEOF)
          return Error::Code::kCborIncompleteMessage;
        OSP_LOG_WARN << "parse error: " << result;
        return Error::Code::kCborParsing;
      } else {
        auto request_entry = requests.find(response.request_id);
        if (request_entry == requests.end()) {
          OSP_LOG_WARN << "bad response id: " << response.request_id;
          return Error::Code::kCborParsing;
        }
        std::vector<std::string>& urls = request_entry->second;
        if (urls.size() != response.url_availabilities.size()) {
          OSP_LOG_WARN << "bad response size: expected " << urls.size()
                       << " but got " << response.url_availabilities.size();
          return Error::Code::kCborParsing;
        }
        UpdateAvailabilities(urls, response.url_availabilities);
        requests.erase(response.request_id);
        if (requests.empty())
          response_watch = MessageDemuxer::MessageWatch();
        return result;
      }
    } break;
    case msgs::Type::kPresentationUrlAvailabilityEvent: {
      msgs::PresentationUrlAvailabilityEvent event;
      ssize_t result = msgs::DecodePresentationUrlAvailabilityEvent(
          buffer, buffer_size, &event);
      if (result < 0) {
        if (result == -CborErrorUnexpectedEOF)
          return Error::Code::kCborIncompleteMessage;
        OSP_LOG_WARN << "parse error: " << result;
        return Error::Code::kCborParsing;
      } else {
        auto watch_entry = availability_watches.find(event.watch_id);
        if (watch_entry == availability_watches.end()) {
          OSP_LOG_WARN << "bad watch id: " << event.watch_id;
          return result;
        }
        UpdateAvailabilities(event.urls, event.url_availabilities);
        return result;
      }
    } break;
    default:
      break;
  }
  return Error::Code::kCborParsing;
}

}  // namespace presentation
}  // namespace openscreen
