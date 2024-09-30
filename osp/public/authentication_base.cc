// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/authentication_base.h"

#include <utility>

#include "openssl/bn.h"
#include "openssl/ecdh.h"
#include "openssl/evp.h"
#include "openssl/sha.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

AuthenticationBase::AuthenticationBase(MessageDemuxer& demuxer,
                                       Delegate& delegate,
                                       std::vector<uint8_t> fingerprint)
    : delegate_(delegate), fingerprint_(std::move(fingerprint)) {
  auth_handshake_watch_ = demuxer.SetDefaultMessageTypeWatch(
      msgs::Type::kAuthSpake2Handshake, this);
  auth_confirmation_watch_ = demuxer.SetDefaultMessageTypeWatch(
      msgs::Type::kAuthSpake2Confirmation, this);
  auth_status_watch_ =
      demuxer.SetDefaultMessageTypeWatch(msgs::Type::kAuthStatus, this);
}

AuthenticationBase::~AuthenticationBase() = default;

void AuthenticationBase::SetSender(uint64_t instance_id,
                                   std::unique_ptr<ProtocolConnection> sender) {
  auth_data_[instance_id].sender = std::move(sender);
}

void AuthenticationBase::SetReceiver(
    uint64_t instance_id,
    std::unique_ptr<ProtocolConnection> receiver) {
  auth_data_[instance_id].receiver = std::move(receiver);
}

void AuthenticationBase::SetAuthenticationToken(uint64_t instance_id,
                                                const std::string& auth_token) {
  auth_data_[instance_id].auth_token = auth_token;
}

void AuthenticationBase::SetPassword(uint64_t instance_id,
                                     const std::string& password) {
  auth_data_[instance_id].password = password;
}

std::vector<uint8_t> AuthenticationBase::ComputePublicValue(
    const std::vector<uint8_t>& self_private_key) {
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  OSP_CHECK(key);

  BIGNUM* private_bn =
      BN_bin2bn(self_private_key.data(), self_private_key.size(), nullptr);
  OSP_CHECK(private_bn);
  OSP_CHECK(EC_KEY_set_private_key(key, private_bn));

  EC_POINT* point = EC_POINT_new(EC_KEY_get0_group(key));
  OSP_CHECK(point);

  if (!EC_POINT_mul(EC_KEY_get0_group(key), point, private_bn, nullptr, nullptr,
                    nullptr)) {
    // Handle error.
    EC_POINT_free(point);
    BN_free(private_bn);
    EC_KEY_free(key);
    return {};
  }

  OSP_CHECK(EC_KEY_set_public_key(key, point));
  size_t length = i2o_ECPublicKey(key, nullptr);
  OSP_CHECK_GT(length, 0);
  std::vector<uint8_t> public_value(length);
  unsigned char* buf = public_value.data();
  size_t written_length = i2o_ECPublicKey(key, &buf);
  OSP_CHECK_EQ(length, written_length);

  // Release resources.
  EC_POINT_free(point);
  BN_free(private_bn);
  EC_KEY_free(key);

  return public_value;
}

std::array<uint8_t, 64> AuthenticationBase::ComputeSharedKey(
    const std::vector<uint8_t>& self_private_key,
    const std::vector<uint8_t>& peer_public_value,
    const std::string& password) {
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  OSP_CHECK(key);

  BIGNUM* private_bn =
      BN_bin2bn(self_private_key.data(), self_private_key.size(), nullptr);
  OSP_CHECK(private_bn);
  OSP_CHECK(EC_KEY_set_private_key(key, private_bn));

  const unsigned char* buf = peer_public_value.data();
  OSP_CHECK(o2i_ECPublicKey(&key, &buf, peer_public_value.size()));

  std::array<uint8_t, 32> shared_key_data;
  size_t secret_length =
      ECDH_compute_key(shared_key_data.data(), shared_key_data.size(),
                       EC_KEY_get0_public_key(key), key, nullptr);
  OSP_CHECK_GT(secret_length, 0);

  SHA512_CTX sha512;
  SHA512_Init(&sha512);
  SHA512_Update(&sha512, shared_key_data.data(), secret_length);
  SHA512_Update(&sha512, password.data(), password.size());
  std::array<uint8_t, 64> shared_key;
  SHA512_Final(shared_key.data(), &sha512);

  // Release resources.
  BN_free(private_bn);
  EC_KEY_free(key);

  return shared_key;
}

}  // namespace openscreen::osp
