// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_PRESENTATION_URL_AVAILABILITY_LISTENER_H_
#define API_IMPL_PRESENTATION_URL_AVAILABILITY_LISTENER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "api/public/message_demuxer.h"
#include "api/public/presentation/presentation_controller.h"
#include "api/public/protocol_connection_client.h"
#include "api/public/screen_info.h"
#include "msgs/osp_messages.h"
#include "platform/api/time.h"

namespace openscreen {
namespace presentation {

class UrlAvailabilityListener {
 public:
  explicit UrlAvailabilityListener(const std::vector<ScreenInfo>& screens);
  ~UrlAvailabilityListener();

  void AddObserver(const std::vector<std::string>& urls,
                   platform::TimeDelta now,
                   ScreenObserver* observer);
  void RemoveObserver(const std::vector<std::string>& urls,
                      ScreenObserver* observer);

  void OnScreenAdded(const ScreenInfo& info, platform::TimeDelta now);
  void OnScreenChanged(const ScreenInfo& info);
  void OnScreenRemoved(const ScreenInfo& info);
  void OnAllScreensRemoved();

  void RefreshWatches(platform::TimeDelta now);

 private:
  struct AvailabilityClient final
      : ProtocolConnectionClient::ConnectionRequestCallback,
        MessageDemuxer::MessageCallback {
    struct AvailabilityWatch {
      platform::TimeDelta remaining_time;
      std::vector<std::string> urls;
    };

    AvailabilityClient(UrlAvailabilityListener* listener,
                       const std::string& screen_id,
                       const IPEndpoint& endpoint);
    ~AvailabilityClient() override;

    void OnObserverRequest(const std::vector<std::string>& urls,
                           platform::TimeDelta now,
                           ScreenObserver* observer);
    void StartOrQueueRequest(std::vector<std::string> urls);
    bool StartRequest(uint64_t request_id,
                      const std::vector<std::string>& urls);
    void RefreshWatches(platform::TimeDelta now);
    void UpdateAvailabilities(
        const std::vector<std::string>& urls,
        const std::vector<msgs::PresentationUrlAvailability>& availabilities);
    void CancelSubsetWatches(const std::set<std::string>& urls);
    void OnScreenRemoved();

    // ProtocolConnectionClient::ConnectionRequestCallback overrides.
    void OnConnectionOpened(
        uint64_t request_id,
        std::unique_ptr<ProtocolConnection>&& connection) override;
    void OnConnectionFailed(uint64_t request_id) override;

    // MessageDemuxer::MessageCallback overrides.
    ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                    uint64_t connection_id,
                                    msgs::Type message_type,
                                    const uint8_t* buffer,
                                    size_t buffer_size,
                                    platform::TimeDelta now) override;

    ErrorOr<size_t> HandleMessage(msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size);

    UrlAvailabilityListener* const listener;

    // TODO(btolsch): Probably need to make this part of a per-receiver global
    // object so requests of all types are consistent.
    uint64_t next_request_id = 1;
    uint64_t next_watch_id = 1;

    const std::string screen_id;
    uint64_t endpoint_id;

    ProtocolConnectionClient::ConnectRequest connect_request;
    // TODO(btolsch): Observe connection and restart all the things on close.
    std::unique_ptr<ProtocolConnection> stream;

    MessageDemuxer::MessageWatch response_watch;
    std::map<uint64_t, std::vector<std::string>> requests;
    MessageDemuxer::MessageWatch event_watch;
    std::map<uint64_t, AvailabilityWatch> availability_watches;

    platform::TimeDelta last_update_time =
        platform::TimeDelta::FromMilliseconds(0);

    std::map<std::string, msgs::PresentationUrlAvailability>
        current_availabilities;
  };

  // Key is a url.
  std::map<std::string, std::vector<ScreenObserver*>> observers_by_url_;

  // Key is a screen ID.
  std::map<std::string, std::unique_ptr<AvailabilityClient>> clients_;
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_IMPL_PRESENTATION_URL_AVAILABILITY_LISTENER_H_
