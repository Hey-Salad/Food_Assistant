#ifndef SALLY_SPATIAL_PROTOCOL_H
#define SALLY_SPATIAL_PROTOCOL_H

#include <Arduino.h>

#include "sally_policy.h"

namespace SallySpatialProtocol {

constexpr const char* FIRMWARE_VERSION = "sally-spatial-m1.2.0";
constexpr uint32_t FRAME_INTERVAL_MS = SallyPolicy::kDefaultSnapshotIntervalSec * 1000UL;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = SallyPolicy::kHeartbeatIntervalMs;
constexpr uint32_t HTTP_TIMEOUT_MS = 3000;
// TLS handshake + POST to a cloud endpoint needs more headroom than LAN HTTP.
constexpr uint32_t HTTPS_TIMEOUT_MS = 10000;
constexpr uint32_t MAX_BACKOFF_MS = SallyPolicy::kFailureBackoffCapMs;

static_assert(FRAME_INTERVAL_MS == 300000UL, "default snapshot cadence is 5 minutes");
static_assert(HEARTBEAT_INTERVAL_MS == 15000UL, "heartbeat stays at 15 seconds");

inline String trimTrailingSlashes(String value) {
    return String(SallyPolicy::trimTrailingSlashes(std::string(value.c_str())).c_str());
}

inline bool isValidAlias(const String& alias) {
    return SallyPolicy::isValidAlias(std::string(alias.c_str()));
}

inline uint32_t nextBackoffMs(uint8_t failures, uint32_t jitter) {
    return SallyPolicy::failureBackoffMs(failures, jitter);
}

}  // namespace SallySpatialProtocol

#endif
