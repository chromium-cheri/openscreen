// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_CERTIFICATES_QUIC_AGENT_CERTIFICATE_H_
#define OSP_IMPL_QUIC_CERTIFICATES_QUIC_AGENT_CERTIFICATE_H_

#include <memory>
#include <string>
#include <vector>

#include "osp/public/agent_certificate.h"
#include "quiche/quic/core/crypto/proof_source_x509.h"

namespace openscreen::osp {

class QuicAgentCertificate final : public AgentCertificate {
 public:
  QuicAgentCertificate();
  ~QuicAgentCertificate() override;
  QuicAgentCertificate(const QuicAgentCertificate&) = delete;
  QuicAgentCertificate& operator=(const QuicAgentCertificate&) = delete;
  QuicAgentCertificate(QuicAgentCertificate&&) noexcept = delete;
  QuicAgentCertificate& operator=(QuicAgentCertificate&&) noexcept = delete;

  // AgentCertificate overrides.
  bool LoadAgentCertificate(std::string_view filename) override;
  bool LoadPrivateKey(std::string_view filename) override;
  bool RotateAgentCertificate() override;
  AgentFingerprint GetAgentFingerprint() override;

  // Create a ProofSource using currently active agent certificate and private
  // key.
  std::unique_ptr<quic::ProofSource> CreateProofSource();

 private:
  AgentFingerprint agent_fingerprint_;
  std::vector<std::string> certificates_;
  std::string key_raw_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_CERTIFICATES_QUIC_AGENT_CERTIFICATE_H_
