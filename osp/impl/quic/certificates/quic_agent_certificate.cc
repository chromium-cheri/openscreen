// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/certificates/quic_agent_certificate.h"

#include <chrono>
#include <utility>

#include "quiche/quic/core/crypto/proof_source_x509.h"
#include "quiche/quic/core/quic_utils.h"
#include "util/base64.h"
#include "util/crypto/certificate_utils.h"
#include "util/crypto/pem_helpers.h"
#include "util/osp_logging.h"
#include "util/read_file.h"

namespace openscreen::osp {

namespace {

using FileUniquePtr = std::unique_ptr<FILE, decltype(&fclose)>;

constexpr char kCertificatesPath[] =
    "osp/impl/quic/certificates/agent_certificate.crt";
constexpr char kPrivateKeyPath[] = "osp/impl/quic/certificates/private_key.key";
constexpr int kOneYearInSeconds = 365 * 24 * 60 * 60;
constexpr auto kCertificateDuration = std::chrono::seconds(kOneYearInSeconds);

bssl::UniquePtr<EVP_PKEY> GeneratePrivateKey() {
  bssl::UniquePtr<EVP_PKEY> root_key = GenerateRsaKeyPair();
  OSP_CHECK(root_key);
  return root_key;
}

// TODO(issuetracker.google.com/300236996): There are currently some spec issues
// about certificates that are still under discussion. Make all fields of the
// certificate comply with the requirements of the spec once all the issues are
// closed.
bssl::UniquePtr<X509> GenerateRootCert(const EVP_PKEY& root_key) {
  ErrorOr<bssl::UniquePtr<X509>> root_cert_or_error =
      CreateSelfSignedX509Certificate("Open Screen Certificate",
                                      kCertificateDuration, root_key,
                                      GetWallTimeSinceUnixEpoch(), true);
  OSP_CHECK(root_cert_or_error);
  return std::move(root_cert_or_error.value());
}

}  // namespace

QuicAgentCertificate::QuicAgentCertificate() {
  bool result = GenerateCredentialsToFile() && LoadCredentials();
  OSP_CHECK(result);
}

QuicAgentCertificate::~QuicAgentCertificate() = default;

bool QuicAgentCertificate::LoadAgentCertificate(std::string_view filename) {
  certificates_.clear();
  agent_fingerprint_.clear();

  // NOTE: There are currently some spec issues about certificates that are
  // still under discussion. Add validations to check if this is a valid OSP
  // agent certificate once all the issues are closed.
  certificates_ = ReadCertificatesFromPemFile(filename);
  if (!certificates_.empty()) {
    agent_fingerprint_ = base64::Encode(quic::RawSha256(certificates_[0]));
    return !agent_fingerprint_.empty();
  } else {
    return false;
  }
}

bool QuicAgentCertificate::LoadPrivateKey(std::string_view filename) {
  key_.reset();

  FileUniquePtr key_file(fopen(std::string(filename).c_str(), "r"), &fclose);
  if (!key_file) {
    return false;
  }

  key_ = bssl::UniquePtr<EVP_PKEY>(PEM_read_PrivateKey(
      key_file.get(), nullptr /* x */, nullptr /* cb */, nullptr /* u */));

  return (key_ != nullptr);
}

bool QuicAgentCertificate::RotateAgentCertificate() {
  return GenerateCredentialsToFile() && LoadCredentials();
}

AgentFingerprint QuicAgentCertificate::GetAgentFingerprint() {
  return agent_fingerprint_;
}

std::unique_ptr<quic::ProofSource>
QuicAgentCertificate::CreateServerProofSource() {
  if (certificates_.empty() || !key_ || agent_fingerprint_.empty()) {
    return nullptr;
  }

  auto chain = quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>(
      new quic::ProofSource::Chain(certificates_));
  OSP_CHECK(chain) << "Failed to create the quic::ProofSource::Chain.";

  quic::CertificatePrivateKey key{std::move(key_)};
  return quic::ProofSourceX509::Create(std::move(chain), std::move(key));
}

std::unique_ptr<quic::ClientProofSource>
QuicAgentCertificate::CreateClientProofSource(
    std::string_view server_hostname) {
  if (certificates_.empty() || !key_ || agent_fingerprint_.empty()) {
    return nullptr;
  }

  auto chain = quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>(
      new quic::ProofSource::Chain(certificates_));
  OSP_CHECK(chain) << "Failed to create the quic::ProofSource::Chain.";

  quic::CertificatePrivateKey key{std::move(key_)};

  auto client_proof_source = std::make_unique<quic::DefaultClientProofSource>();
  client_proof_source->AddCertAndKey(
      std::vector<std::string>{std::string{server_hostname}}, std::move(chain),
      std::move(key));
  return client_proof_source;
}

void QuicAgentCertificate::ResetCredentials() {
  agent_fingerprint_.clear();
  certificates_.clear();
  key_.reset();
}

bool QuicAgentCertificate::GenerateCredentialsToFile() {
  bssl::UniquePtr<EVP_PKEY> root_key = GeneratePrivateKey();
  bssl::UniquePtr<X509> root_cert = GenerateRootCert(*root_key);

  FileUniquePtr f(fopen(kPrivateKeyPath, "w"), &fclose);
  if (PEM_write_PrivateKey(f.get(), root_key.get(), nullptr, nullptr, 0, 0,
                           nullptr) != 1) {
    OSP_LOG_ERROR << "Failed to write private key, check permissions?";
    return false;
  }
  OSP_LOG_INFO << "Generated new private key in file: " << kPrivateKeyPath;

  FileUniquePtr cert_file(fopen(kCertificatesPath, "w"), &fclose);
  if (PEM_write_X509(cert_file.get(), root_cert.get()) != 1) {
    OSP_LOG_ERROR << "Failed to write agent certificate, check permissions?";
    return false;
  }
  OSP_LOG_INFO << "Generated new agent certificate in file: "
               << kCertificatesPath;

  return true;
}

bool QuicAgentCertificate::LoadCredentials() {
  bool result = LoadAgentCertificate(kCertificatesPath) &&
                LoadPrivateKey(kPrivateKeyPath);
  if (!result) {
    ResetCredentials();
    return false;
  } else {
    return true;
  }
}

}  // namespace openscreen::osp
