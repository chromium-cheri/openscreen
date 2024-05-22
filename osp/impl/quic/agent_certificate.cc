// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/agent_certificate.h"

#include <utility>

#include "util/crypto/pem_helpers.h"
#include "util/osp_logging.h"
#include "util/read_file.h"

namespace openscreen::osp {

namespace {

constexpr char kCertificatesPath[] =
    "osp/impl/quic/certificates/openscreen.pem";
constexpr char kPrivateKeyPath[] = "osp/impl/quic/certificates/openscreen.key";
constexpr char kFingerPrint[] =
    "50:87:8D:CA:1B:9B:67:76:CB:87:88:1C:43:20:82:7A:91:F5:9B:74:4D:85:95:D0:"
    "76:E6:0B:50:7F:D3:29:D9";

}  // namespace

AgentCertificate::AgentCertificate() = default;

AgentCertificate::~AgentCertificate() = default;

// TODO(issuetracker.google.com/300236996): Replace with OSP certificate
// generation.
std::unique_ptr<quic::ProofSource> AgentCertificate::CreateProofSource() {
  if (certificates_.empty()) {
    certificates_ = ReadCertificatesFromPemFile(kCertificatesPath);
  }
  OSP_CHECK_EQ(certificates_.size(), 1u)
      << "Failed to parse the certificates file.";
  auto chain = quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>(
      new quic::ProofSource::Chain(certificates_));
  OSP_CHECK(chain) << "Failed to create the quic::ProofSource::Chain.";

  if (key_raw_.empty()) {
    key_raw_ = ReadEntireFileToString(kPrivateKeyPath);
  }
  std::unique_ptr<quic::CertificatePrivateKey> key =
      quic::CertificatePrivateKey::LoadFromDer(key_raw_);
  OSP_CHECK(key) << "Failed to parse the key file.";

  return quic::ProofSourceX509::Create(std::move(chain), std::move(*key));
}

// TODO(issuetracker.google.com/300236996): Replace with OSP certificate
// generation.
std::string AgentCertificate::GetFingerprint() {
  return kFingerPrint;
}

}  // namespace openscreen::osp
