// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_RECEIVER_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_RECEIVER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/public/message_demuxer.h"
#include "api/public/presentation/presentation_connection.h"
#include "msgs/osp_messages.h"

namespace openscreen {
namespace presentation {

enum class ResponseResult {
  kSuccess = 0,
  kInvalidUrl,
  kRequestTimedOut,
  kRequestFailedTransient,
  kRequestFailedPermanent,
  kHttpError,
  kUnknown,
};

class ReceiverDelegate {
 public:
  virtual ~ReceiverDelegate() = default;

  virtual std::vector<msgs::PresentationUrlAvailability>
  OnUrlAvailabilityRequest(uint64_t client_id,
                           uint64_t request_duration,
                           std::vector<std::string>&& urls) = 0;

  // Called when a new presentation is requested by a controller.  This should
  // return true if the presentation was accepted, false otherwise.
  virtual bool StartPresentation(const Connection::Info& info,
                                 uint64_t source_id,
                                 const std::string& http_headers) = 0;

  virtual bool ConnectToPresentation(uint64_t request_id,
                                     const std::string& id,
                                     uint64_t source_id) = 0;

  // Called when a presentation is requested to be terminated by a controller.
  virtual void TerminatePresentation(const std::string& id,
                                     TerminationReason reason) = 0;
};

class Receiver final : public MessageDemuxer::MessageCallback {
 public:
  // Returns the single instance.
  // TODO(btolsch): It'd be relatively easy to make this non-singleton to
  // support per-profile state, but maybe wait until the Controller can also be
  // non-singleton?
  static Receiver* Get();

  // TODO(btolsch): More consequences of being a singleton.
  void Init();
  void Deinit();

  // Sets the object to call when a new receiver connection is available.
  // |delegate| must either outlive PresentationReceiver or live until a new
  // delegate (possibly nullptr) is set.  Setting the delegate to nullptr will
  // automatically ignore all future receiver requests.
  void SetReceiverDelegate(ReceiverDelegate* delegate);

  void OnUrlAvailabilityUpdate(
      uint64_t client_id,
      const std::vector<msgs::PresentationUrlAvailability>& availabilities);

  // Called by the embedder to report its response to StartPresentation.
  void OnPresentationStarted(const std::string& presentation_id,
                             Connection* connection,
                             ResponseResult result);

  void OnConnectionCreated(uint64_t request_id,
                           Connection* connection,
                           ResponseResult result);

  // Called by the embedder to report that a presentation has been terminated.
  void OnPresentationTerminated(const std::string& presentation_id,
                                TerminationReason reason);

  void OnConnectionDestroyed(Connection* connection);

  // MessageDemuxer::MessageCallback overrides.
  ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  platform::TimeDelta now) override;

 private:
  struct QueuedResponse {
    uint64_t request_id;
    uint64_t connection_id;
    uint64_t endpoint_id;
  };

  struct Presentation {
    uint64_t endpoint_id;
    MessageDemuxer::MessageWatch terminate_watch;
    uint64_t terminate_request_id;
    std::vector<Connection*> connections;
  };

  Receiver();
  ~Receiver();

  ReceiverDelegate* delegate_ = nullptr;
  std::map<std::string, QueuedResponse> queued_initiation_responses_;
  std::map<std::string, std::vector<QueuedResponse>>
      queued_connection_responses_;
  std::map<std::string, Presentation> presentations_;
  std::unique_ptr<ConnectionManager> connection_manager_;
  MessageDemuxer::MessageWatch availability_watch_;
  MessageDemuxer::MessageWatch initiation_watch_;
  MessageDemuxer::MessageWatch connection_watch_;
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_RECEIVER_H_
