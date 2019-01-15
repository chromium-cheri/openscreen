// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "api/public/presentation/presentation_connection.h"
#include "api/public/protocol_connection.h"
#include "api/public/screen_listener.h"
#include "base/error.h"

namespace openscreen {
namespace presentation {

class UrlAvailabilityListener;

class RequestDelegate {
 public:
  virtual ~RequestDelegate() = default;

  virtual void OnConnection(std::unique_ptr<Connection> connection) = 0;
  virtual void OnError(const Error& error) = 0;
};

class ScreenObserver {
 public:
  virtual ~ScreenObserver() = default;

  // Called when screens compatible with |presentation_url| are known to be
  // available.
  virtual void OnScreensAvailable(const std::string& presentation_url,
                                  const std::string& screen_id) = 0;
  // Only called for |screen_id| values previously advertised as available.
  virtual void OnScreensUnavailable(const std::string& presentation_url,
                                    const std::string& screen_id) = 0;
};

class Controller final : public ScreenListener::Observer {
 public:
  // Returns the single instance.
  static Controller* Get();

  class ScreenWatch {
   public:
    ScreenWatch();
    ScreenWatch(const std::vector<std::string>& urls, ScreenObserver* observer);
    ScreenWatch(ScreenWatch&&);
    ~ScreenWatch();

    ScreenWatch& operator=(ScreenWatch&&);

    explicit operator bool() const { return observer_; }

   private:
    std::vector<std::string> urls_;
    ScreenObserver* observer_ = nullptr;
  };

  class ConnectRequest {
   public:
    ConnectRequest();
    ConnectRequest(const std::string& screen_id,
                   bool is_reconnect,
                   uint64_t request_id);
    ConnectRequest(ConnectRequest&&);
    ~ConnectRequest();

    ConnectRequest& operator=(ConnectRequest&&);

    explicit operator bool() const { return request_id_; }

   private:
    std::string screen_id_;
    bool is_reconnect_;
    uint64_t request_id_;
  };

  // TODO(btolsch): These wouldn't be needed if this weren't a singleton.
  void Init();
  void Deinit();

  // Requests screens compatible with all urls in |urls| and registers
  // |observer| for availability changes.  The screens will be a subset of the
  // screen list maintained by the ScreenListener.  Returns an RAII object that
  // tracks the registration.
  ScreenWatch RegisterScreenWatch(const std::vector<std::string>& urls,
                                  ScreenObserver* observer);

  // Requests that a new presentation be created on |screen_id| using
  // |presentation_url|, with the result passed to |delegate|.
  // |conn_delegate| is passed to the resulting connection.  The returned
  // ConnectRequest object may be destroyed before any |delegate| methods are
  // called to cancel the request.
  ConnectRequest StartPresentation(const std::string& url,
                                   const std::string& screen_id,
                                   RequestDelegate* delegate,
                                   Connection::Delegate* conn_delegate);

  // Requests reconnection to the presentation with the given id and URL running
  // on the screen with |screen_id|, with the result passed to |delegate|.
  // |conn_delegate| is passed to the resulting connection.
  // The returned ConnectRequest object may be destroyed before any |delegate|
  // methods are called to cancel the request.
  // TODO(btolsch): How will reconnect go about restricting the search set of
  // presentations by profile/browsing context?  Would it help if Controller
  // were not a singleton so we could maintain per-profile information in each
  // Controller instance?  So far, it seems like this problem won't apply to
  // Receiver, but is there any case where we _do_ need to segregate information
  // on the receiver for privacy reasons (e.g. receiver status only shows
  // title/description to original controlling context)?
  ConnectRequest ReconnectPresentation(const std::vector<std::string>& urls,
                                       const std::string& presentation_id,
                                       const std::string& screen_id,
                                       RequestDelegate* delegate,
                                       Connection::Delegate* conn_delegate);

  // Requests reconnection with a previously-connected connection.  This both
  // avoids having to respecify the parameters and connection delegate but also
  // simplifies the implementation of the Presentation API requirement to return
  // the same connection object where possible.
  ConnectRequest ReconnectConnection(std::unique_ptr<Connection>&& connection,
                                     RequestDelegate* delegate);

  // Called by the embedder to report that a presentation has been terminated.
  void OnPresentationTerminated(const std::string& presentation_id,
                                TerminationReason reason);

  std::string GetScreenIdForPresentationId(
      const std::string& presentation_id) const;
  ProtocolConnection* GetConnectionRequestGroupStream(
      const std::string& screen_id);

  void SetConnectionRequestGroupStreamForTest(
      const std::string& screen_id,
      std::unique_ptr<ProtocolConnection>&& stream);

  // TODO(btolsch): By endpoint.
  uint64_t GetNextRequestId();

  void OnConnectionDestroyed(Connection* connection);

 private:
  struct TerminateListener;
  struct InitiationRequest;
  struct ConnectionRequest;
  struct TerminationRequest;
  struct MessageGroupStreams;

  struct ControlledPresentation {
    std::string screen_id;
    std::string url;
    std::vector<Connection*> connections;
  };

  static std::string MakePresentationId(const std::string& url,
                                        const std::string& screen_id);

  Controller();
  ~Controller();

  uint64_t GetNextConnectionId(const std::string& id);

  void OpenConnection(uint64_t connection_id,
                      uint64_t endpoint_id,
                      const std::string& screen_id,
                      RequestDelegate* request_delegate,
                      std::unique_ptr<Connection>&& connection,
                      std::unique_ptr<ProtocolConnection>&& stream);

  // Cancels compatible screen monitoring for the given |urls|, |observer| pair.
  void CancelScreenWatch(const std::vector<std::string>& urls,
                         ScreenObserver* observer);

  // Cancels a presentation connect request for the given |request_id| if one is
  // pending.
  void CancelConnectRequest(const std::string& screen_id,
                            bool is_reconnect,
                            uint64_t request_id);

  // ScreenListener::Observer overrides.
  void OnStarted() override;
  void OnStopped() override;
  void OnSuspended() override;
  void OnSearching() override;
  void OnScreenAdded(const ScreenInfo& info) override;
  void OnScreenChanged(const ScreenInfo& info) override;
  void OnScreenRemoved(const ScreenInfo& info) override;
  void OnAllScreensRemoved() override;
  void OnError(ScreenListenerError) override;
  void OnMetrics(ScreenListener::Metrics) override;

  uint64_t next_request_id_ = 1;
  std::map<std::string, uint64_t> next_connection_id_;

  std::map<std::string, ControlledPresentation> presentations_;

  std::unique_ptr<ConnectionManager> connection_manager_;

  std::unique_ptr<UrlAvailabilityListener> availability_listener_;
  std::map<std::string, IPEndpoint> screen_endpoints_;

  std::map<std::string, std::unique_ptr<MessageGroupStreams>> group_streams_;
  std::map<std::string, std::unique_ptr<TerminateListener>>
      terminate_listeners_;
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
