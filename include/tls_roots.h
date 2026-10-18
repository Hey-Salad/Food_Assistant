#ifndef SALLY_TLS_ROOTS_H
#define SALLY_TLS_ROOTS_H

// PEM fallback and the websocket CA. The definition is in src/tls_roots.cpp.
// Roots, in order: ISRG Root X1, ISRG Root X2, GTS Root R1, R2, R3, R4.
// GTS is the public chain Cloudflare and Vercel present. Cloudflare Origin CA
// is not a public root and is not included.
// JPEG/heartbeat uploads prefer the ESP32 x509 bundle (see uplink_tls.h),
// which already contains these roots. This PEM is what APIs that only accept
// a PEM use. Certificate verification stays enabled.

namespace SallyTls {

extern const char kRootCaPem[];

}  // namespace SallyTls

#endif
