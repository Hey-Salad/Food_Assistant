#ifndef SALLY_UPLINK_TLS_H
#define SALLY_UPLINK_TLS_H

#include <WiFiClientSecure.h>

#include "tls_roots.h"

// ESP-IDF embeds the Mozilla-based x509 bundle as this symbol inside
// libmbedtls. Arduino's WiFiClientSecure::setCACertBundle expects that same
// layout. A strong reference pulls the object in; verification then uses
// MBEDTLS_SSL_VERIFY_REQUIRED via the bundle callback. The PEM is the
// fallback for a core that does not ship the bundle. Certificate
// verification stays enabled on both paths.

#if defined(ARDUINO_ARCH_ESP32)
extern "C" {
extern const uint8_t sally_x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
}
#endif

namespace SallyTls {

inline bool configureClient(WiFiClientSecure& client) {
#if defined(ARDUINO_ARCH_ESP32)
    client.setCACertBundle(sally_x509_crt_bundle_start);
    return true;
#else
    client.setCACert(kRootCaPem);
    return false;
#endif
}

inline const char* verificationLabel(bool usedBundle) {
    return usedBundle ? "esp32-x509-bundle" : "embedded-isrg-gts-roots";
}

}  // namespace SallyTls

#endif
