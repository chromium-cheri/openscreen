// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_AUTHENTICATION_BASE_H_
#define OSP_PUBLIC_AUTHENTICATION_BASE_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "osp/public/message_demuxer.h"
#include "osp/public/protocol_connection.h"

namespace openscreen::osp {

// There are two kinds of classes for authentication: AuthenticationAlice and
// AuthenticationBob. This class holds common codes for the two classes.
class AuthenticationBase : public MessageDemuxer::MessageCallback {
 public:
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) noexcept = delete;
    Delegate& operator=(Delegate&&) noexcept = delete;
    virtual ~Delegate() = default;

    virtual void InitAuthenticationData(std::string_view instance_name,
                                        uint64_t instance_id) = 0;
    virtual void OnAuthenticationSucceed(uint64_t instance_id) = 0;
    virtual void OnAuthenticationFailed(uint64_t instance_id) = 0;
  };

  AuthenticationBase(MessageDemuxer& demuxer,
                     Delegate& delegate,
                     std::vector<uint8_t> fingerprint);
  AuthenticationBase(const AuthenticationBase&) = delete;
  AuthenticationBase& operator=(const AuthenticationBase&) = delete;
  AuthenticationBase(AuthenticationBase&&) noexcept = delete;
  AuthenticationBase& operator=(AuthenticationBase&&) noexcept = delete;
  virtual ~AuthenticationBase();

  virtual void StartAuthentication(uint64_t instance_id) = 0;
  void SetSender(uint64_t instance_id,
                 std::unique_ptr<ProtocolConnection> sender);
  void SetReceiver(uint64_t instance_id,
                   std::unique_ptr<ProtocolConnection> receiver);
  void SetAuthenticationToken(uint64_t instance_id,
                              const std::string& auth_token);
  void SetPassword(uint64_t instance_id, const std::string& password);

 protected:
  struct AuthenticationData {
    std::unique_ptr<ProtocolConnection> sender;
    std::unique_ptr<ProtocolConnection> receiver;
    std::string auth_token;
    std::string password;
    std::array<uint8_t, 64> shared_key;
  };

  std::vector<uint8_t> ComputePublicValue(
      const std::vector<uint8_t>& self_private_key);
  std::array<uint8_t, 64> ComputeSharedKey(
      const std::vector<uint8_t>& self_private_key,
      const std::vector<uint8_t>& peer_public_value,
      const std::string& password);

  // Map an instance ID to data about authentication.
  std::map<uint64_t, AuthenticationData> auth_data_;

  Delegate& delegate_;
  std::vector<uint8_t> fingerprint_;

 private:
  MessageDemuxer::MessageWatch auth_handshake_watch_;
  MessageDemuxer::MessageWatch auth_confirmation_watch_;
  MessageDemuxer::MessageWatch auth_status_watch_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_AUTHENTICATION_BASE_H_
