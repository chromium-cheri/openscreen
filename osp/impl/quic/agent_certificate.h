// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_AGENT_CERTIFICATE_H_
#define OSP_IMPL_QUIC_AGENT_CERTIFICATE_H_

#include <memory>
#include <string>

#include "quiche/quic/core/crypto/proof_source_x509.h"

namespace openscreen::osp {

class AgentCertificate final {
 public:
  AgentCertificate();
  ~AgentCertificate();
  AgentCertificate(const AgentCertificate&) = delete;
  AgentCertificate& operator=(const AgentCertificate&) = delete;
  AgentCertificate(AgentCertificate&&) noexcept = delete;
  AgentCertificate& operator=(AgentCertificate&&) noexcept = delete;

  // Returns the currently active agent's certificate.
  std::unique_ptr<quic::ProofSource> CreateProofSource();

  // Returns the fingerprint of the currently active agent's certificate. The
  // fingerprint is sent to the client as a DNS TXT record. Client uses the
  // fingerprint to verify agent's certificate.
  std::string GetFingerprint();
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_AGENT_CERTIFICATE_H_
