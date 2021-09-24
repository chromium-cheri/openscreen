// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/pem_helpers.h"

#include <openssl/bytestring.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <stdio.h>

#include <cstring>
#include <utility>

#include "absl/strings/match.h"
#include "util/osp_logging.h"

namespace openscreen {

std::vector<std::vector<uint8_t>> ReadCertificatesFromPemFile(
    absl::string_view filename) {
  FILE* fp = fopen(filename.data(), "r");
  OSP_CHECK(fp) << "Unable to open " << filename;
  std::vector<std::vector<uint8_t>> certs;
  char* name;
  char* header;
  unsigned char* data;
  long length;  // NOLINT
  while (PEM_read(fp, &name, &header, &data, &length) == 1) {
    if (absl::StartsWith(name, "CERTIFICATE")) {
      std::vector<uint8_t> cert(static_cast<size_t>(length));
      std::memcpy(cert.data(), data, static_cast<size_t>(length));
      certs.push_back(std::move(cert));
    }
    OPENSSL_free(name);
    OPENSSL_free(header);
    OPENSSL_free(data);
  }
  fclose(fp);
  return certs;
}

std::vector<std::uint8_t> ReadCertificateFromPemFile(
    absl::string_view filename) {
  std::vector<std::vector<std::uint8_t>> certs =
      ReadCertificatesFromPemFile(filename);
  OSP_DCHECK_EQ(1u, certs.size());
  return certs[0];
}

bssl::UniquePtr<EVP_PKEY> ReadKeyFromPemFile(absl::string_view filename) {
  FILE* fp = fopen(filename.data(), "r");
  if (!fp) {
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> pkey;
  char* name;
  char* header;
  unsigned char* data;
  long length;  // NOLINT
  while (PEM_read(fp, &name, &header, &data, &length) == 1) {
    if (absl::StartsWith(name, "RSA PRIVATE KEY")) {
      OSP_DCHECK(!pkey);
      CBS cbs;
      CBS_init(&cbs, data, length);
      RSA* rsa = RSA_parse_private_key(&cbs);
      if (rsa) {
        pkey.reset(EVP_PKEY_new());
        EVP_PKEY_assign_RSA(pkey.get(), rsa);
      }
    }
    OPENSSL_free(name);
    OPENSSL_free(header);
    OPENSSL_free(data);
  }
  fclose(fp);
  return pkey;
}

}  // namespace openscreen
