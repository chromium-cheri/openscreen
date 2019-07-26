#ifndef CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_INTERNAL_H_
#define CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_INTERNAL_H_

#include <openssl/x509.h>

#include <vector>

namespace cast {
namespace certificate {

struct TrustStore {
  std::vector<bssl::UniquePtr<X509>> certs;
};

}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_INTERNAL_H_
