// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/cast_socket_impl.h"

#include "cast/common/channel/message_framer.h"

namespace cast {
namespace channel {

using openscreen::ErrorOr;
using openscreen::platform::TlsConnection;

CastSocketImpl::CastSocketImpl(std::unique_ptr<TlsConnection> connection,
                               CastSocket::Delegate* delegate,
                               uint32_t socket_id)
    : CastSocket(delegate, socket_id), connection_(std::move(connection)) {
  connection_->set_client(this);
}

CastSocketImpl::~CastSocketImpl() = default;

Error CastSocketImpl::SendMessage(const CastMessage& message) {
  if (write_blocked_) {
    message_queue_.emplace_back(message);
    return Error::Code::kNone;
  }

  ErrorOr<std::string> out = MessageFramer::Serialize(message);
  if (!out) {
    return out.error();
  }

  connection_->Write(out.value().data(), out.value().size());
  return Error::Code::kNone;
}

void CastSocketImpl::OnWriteBlocked(TlsConnection* connection) {
  write_blocked_ = true;
}

void CastSocketImpl::OnWriteUnblocked(TlsConnection* connection) {
  write_blocked_ = false;
  for (const CastMessage& message : message_queue_) {
    SendMessage(message);
  }
  message_queue_.clear();
}

void CastSocketImpl::OnError(TlsConnection* connection, Error error) {
  delegate_->OnError(this, error);
}

void CastSocketImpl::OnRead(TlsConnection* connection,
                            std::vector<uint8_t> block) {
  read_buffer_.insert(read_buffer_.end(), block.begin(), block.end());
  size_t length = 0;
  ErrorOr<CastMessage> message_or_error = MessageFramer::TryDeserialize(
      absl::Span<uint8_t>(&read_buffer_[0], read_buffer_.size()), &length);
  if (!message_or_error) {
    return;
  }
  read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + length);
  delegate_->OnMessage(this, &message_or_error.value());
}

}  // namespace channel
}  // namespace cast
