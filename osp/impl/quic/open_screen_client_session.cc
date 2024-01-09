// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/open_screen_client_session.h"

#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

OpenScreenClientSession::OpenScreenClientSession(
    std::unique_ptr<quic::QuicConnection> connection,
    quic::QuicCryptoClientConfig* quic_crypto_client_config,
    OpenScreenSessionBase::Visitor* visitor,
    const quic::QuicConfig& config,
    const quic::ParsedQuicVersionVector& supported_versions,
    const quic::QuicServerId& server_id)
    : OpenScreenSessionBase(std::move(connection),
                            visitor,
                            config,
                            supported_versions),
      server_id_(server_id),
      quic_crypto_client_config_(quic_crypto_client_config) {
  Initialize();
}

OpenScreenClientSession::~OpenScreenClientSession() = default;

void OpenScreenClientSession::Initialize() {
  // Initialize must be called first, as that's what generates the crypto
  // stream.
  OpenScreenSessionBase::Initialize();
  OSP_LOG_INFO << "QuicClient starts crypto connecting.";
  static_cast<quic::QuicCryptoClientStreamBase*>(GetMutableCryptoStream())
      ->CryptoConnect();
}

std::unique_ptr<quic::QuicCryptoStream>
OpenScreenClientSession::CreateCryptoStream() {
  return std::make_unique<quic::QuicCryptoClientStream>(
      server_id_, this, nullptr, quic_crypto_client_config_, this,
      /*has_application_state*/ true);
}

void OpenScreenClientSession::OnProofValid(
    const quic::QuicCryptoClientConfig::CachedState& cached) {}

void OpenScreenClientSession::OnProofVerifyDetailsAvailable(
    const quic::ProofVerifyDetails& verify_details) {}

}  // namespace openscreen::osp
