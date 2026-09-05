#ifndef SALLY_SPATIAL_PROTOCOL_H
#define SALLY_SPATIAL_PROTOCOL_H

#include <Arduino.h>

namespace SallySpatialProtocol {

constexpr const char* FIRMWARE_VERSION = "sally-spatial-m1.1.0";
constexpr uint32_t FRAME_INTERVAL_MS = 1000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 15000;
constexpr uint32_t HTTP_TIMEOUT_MS = 3000;
// TLS handshake + POST to a cloud endpoint needs more headroom than LAN HTTP.
constexpr uint32_t HTTPS_TIMEOUT_MS = 10000;
constexpr uint32_t MAX_BACKOFF_MS = 30000;

inline String trimTrailingSlashes(String value) {
    value.trim();
    while (value.endsWith("/")) {
        value.remove(value.length() - 1);
    }
    return value;
}

inline bool isValidAlias(const String& alias) {
    if (alias.isEmpty() || alias.length() > 32) return false;
    for (size_t i = 0; i < alias.length(); ++i) {
        const char c = alias.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

inline uint32_t nextBackoffMs(uint8_t failures, uint32_t jitter) {
    const uint8_t exponent = failures > 5 ? 5 : failures;
    uint32_t delayMs = 1000UL << exponent;
    if (delayMs > MAX_BACKOFF_MS) delayMs = MAX_BACKOFF_MS;
    const uint32_t boundedJitter = jitter % 251UL;
    return delayMs >= MAX_BACKOFF_MS ? MAX_BACKOFF_MS : min(MAX_BACKOFF_MS, delayMs + boundedJitter);
}

}  // namespace SallySpatialProtocol

#endif
