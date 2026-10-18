#ifndef SALLY_POLICY_H
#define SALLY_POLICY_H

// Host-testable decisions for provisioning, LAN access, snapshot cadence, and
// HTTP 429 backoff. No Arduino types: the firmware adapters live in the .cpp
// files. Arduino's min/max macros must not be used here.

#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

namespace SallyPolicy {

constexpr uint32_t kHeartbeatIntervalMs = 15000;
constexpr uint32_t kDefaultSnapshotIntervalSec = 300;
constexpr uint32_t kMinSnapshotIntervalSec = 30;
constexpr uint32_t kMaxSnapshotIntervalSec = 24UL * 60UL * 60UL;
constexpr uint32_t kFailureBackoffCapMs = 30000;
constexpr uint32_t kRateLimitBackoffCapMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kProvisioningWindowMs = 10UL * 60UL * 1000UL;
// sally-api device tokens are 32 bytes, sent as 64 hexadecimal characters.
constexpr uint32_t kDeviceTokenBytes = 32;
constexpr uint32_t kDeviceTokenHexChars = kDeviceTokenBytes * 2;
constexpr uint32_t kMinAdminTokenLength = 32;
// POST /frames rejects a body over 2 MiB, and a second frame inside 30s is 429.
constexpr uint32_t kMaxFrameBytes = 2UL * 1024UL * 1024UL;
constexpr uint32_t kFrameMinGapMs = 30000;
constexpr uint32_t kHeartbeatRateLimitFloorMs = 15000;

// DFR1154 BOOT is GPIO0, active low. RST is the EN button and is not a GPIO.
// Holding BOOT across reset enters the ROM bootloader, so unlock is a hold
// after the app is already running.
constexpr int kBootButtonGpio = 0;
constexpr uint32_t kUnlockHoldMs = 5000;
constexpr uint32_t kFactoryResetHoldMs = 12000;

enum class BootButtonAction { None, UnlockProvisioning, FactoryReset };
enum class LanAdminDecision { Allow, Unauthorized, Disabled };

struct LegacyFields {
    std::string ssid;
    std::string password;
    std::string cloudDeviceId;
    std::string userId;
    std::string deviceToken;
    std::string piBaseUrl;
};

struct ProvisionInput {
    std::string ssid;
    std::string password;
    std::string deviceToken;
    std::string heartbeatToken;
    std::string piBaseUrl;
    std::string serverBaseUrl;
    std::string ingestBaseUrl;
    std::string alias;
    std::string adminToken;
    std::string cloudDeviceId;
    std::string userId;
    bool hasSnapshotInterval = false;
    int64_t snapshotIntervalSec = 0;
};

struct ProvisionDecision {
    bool ok = false;
    std::string ssid;
    std::string password;
    std::string deviceToken;
    std::string piBaseUrl;
    std::string alias;
    std::string adminToken;
    std::string cloudDeviceId;
    std::string userId;
    uint32_t snapshotIntervalSec = kDefaultSnapshotIntervalSec;
};

inline std::string trim(std::string value) {
    size_t start = 0;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

inline std::string trimTrailingSlashes(std::string value) {
    value = trim(value);
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

inline bool isValidAlias(const std::string& alias) {
    if (alias.empty() || alias.size() > 32) return false;
    for (char c : alias) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

inline bool isHttpUrl(const std::string& url) {
    return url.compare(0, 7, "http://") == 0 || url.compare(0, 8, "https://") == 0;
}

inline bool snapshotIntervalAllowed(uint32_t seconds) {
    return seconds >= kMinSnapshotIntervalSec && seconds <= kMaxSnapshotIntervalSec;
}

inline bool isRetiredModelKey(const char* name) {
    return std::strcmp(name, "openai") == 0 || std::strcmp(name, "elevenlabs") == 0 ||
           std::strcmp(name, "gemini") == 0 || std::strcmp(name, "deepseek") == 0;
}

inline bool provisioningLockedOnBoot(bool wifiConfigured, bool storedLock) {
    return wifiConfigured || storedLock;
}

inline BootButtonAction bootButtonOnRelease(uint32_t heldMs) {
    if (heldMs >= kFactoryResetHoldMs) return BootButtonAction::FactoryReset;
    if (heldMs >= kUnlockHoldMs) return BootButtonAction::UnlockProvisioning;
    return BootButtonAction::None;
}

inline bool bootButtonFactoryResetWhileHeld(uint32_t heldMs) {
    return heldMs >= kFactoryResetHoldMs;
}

inline bool constantTimeEquals(const std::string& a, const std::string& b) {
    const size_t n = a.size() > b.size() ? a.size() : b.size();
    unsigned diff = static_cast<unsigned>(a.size() ^ b.size());
    for (size_t i = 0; i < n; ++i) {
        const unsigned char ca = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char cb = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        diff |= static_cast<unsigned>(ca ^ cb);
    }
    return diff == 0;
}

inline bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Last six hex digits of the hardware id, so two boards never share a name.
inline std::string lastHwIdHex6(const std::string& hwId) {
    std::string hex;
    for (char c : hwId) {
        if (!isHexDigit(c)) continue;
        hex.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (hex.size() >= 6) return hex.substr(hex.size() - 6);
    if (!hex.empty()) return hex;
    return "camera";
}

inline std::string asciiLower(std::string value) {
    for (char& c : value) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return value;
}

inline bool hasSallyPrefix(const std::string& value) {
    return asciiLower(value).compare(0, 6, "sally-") == 0;
}

// mDNS and OTA label, without ".local". Both services use this string.
// A provisioned alias wins. "sally-" is not added again when the alias
// already starts with that prefix, compared case-insensitively. Otherwise
// the label is sally- plus the last six hex digits of the hardware id.
inline std::string mdnsHostname(const std::string& alias, const std::string& hwId) {
    const std::string loweredAlias = asciiLower(trim(alias));
    const std::string loweredId = asciiLower(trim(hwId));
    const bool useAlias = isValidAlias(loweredAlias) && loweredAlias != loweredId;
    const std::string label = useAlias ? loweredAlias : lastHwIdHex6(loweredId);
    std::string host = hasSallyPrefix(label) ? label : std::string("sally-") + label;
    if (host.size() > 63) host.resize(63);
    while (!host.empty() && host.back() == '-') host.pop_back();
    if (host.size() < std::string("sally-x").size() || host == "sally") {
        host = std::string("sally-") + lastHwIdHex6(loweredId);
    }
    return host;
}

// Scheme, userinfo, path, and query stripped. The device token is never part
// of this string.
inline std::string baseHost(const std::string& url) {
    std::string value = trim(url);
    size_t start = 0;
    if (value.compare(0, 8, "https://") == 0) start = 8;
    else if (value.compare(0, 7, "http://") == 0) start = 7;
    const size_t stop = value.find_first_of("/?#", start);
    std::string host = value.substr(start, (stop == std::string::npos ? value.size() : stop) - start);
    const size_t at = host.find('@');
    if (at != std::string::npos) host = host.substr(at + 1);
    while (!host.empty() && host.back() == '.') host.pop_back();
    return host;
}

// Exactly 32 bytes of hex. Shorter opaque strings are not sally-api device tokens.
inline bool isDeviceToken(const std::string& token) {
    if (token.size() != kDeviceTokenHexChars) return false;
    for (char c : token) {
        if (!isHexDigit(c)) return false;
    }
    return true;
}

inline bool frameFits(size_t bytes) {
    return bytes > 0 && bytes <= kMaxFrameBytes;
}

inline bool cloudProvisioned(const std::string& baseUrl, const std::string& deviceToken) {
    return !trimTrailingSlashes(baseUrl).empty() && isDeviceToken(deviceToken);
}

// Cloud provisioning disables LAN admin routes. Otherwise the admin token set
// during BLE provisioning is required. A missing token fails closed.
inline LanAdminDecision lanAdminAccess(bool provisionedToCloud, const std::string& adminToken,
                                       const std::string& presented) {
    if (provisionedToCloud) return LanAdminDecision::Disabled;
    if (adminToken.size() < kMinAdminTokenLength || !constantTimeEquals(adminToken, presented)) {
        return LanAdminDecision::Unauthorized;
    }
    return LanAdminDecision::Allow;
}

inline uint32_t withJitter(uint32_t delayMs, uint32_t cap, uint32_t jitter) {
    const uint32_t bounded = jitter % 251UL;
    if (delayMs >= cap) return cap;
    const uint32_t with = delayMs + bounded;
    return with > cap ? cap : with;
}

// Non-429 failures. First recorded failure (failures == 1) waits about 2s.
inline uint32_t failureBackoffMs(uint8_t failures, uint32_t jitter) {
    const uint8_t exponent = failures > 5 ? 5 : failures;
    uint32_t delayMs = 1000UL << exponent;
    if (delayMs > kFailureBackoffCapMs) delayMs = kFailureBackoffCapMs;
    return withJitter(delayMs, kFailureBackoffCapMs, jitter);
}

// 429. Honor a numeric Retry-After when it is longer than floorMs. Frame uploads
// pass kFrameMinGapMs so a 429 (or a short Retry-After) cannot retry inside the
// server's 30 second window. Heartbeats pass kHeartbeatRateLimitFloorMs.
inline uint32_t rateLimitBackoffMs(int retryAfterSeconds, uint8_t failures, uint32_t jitter,
                                   uint32_t floorMs = kHeartbeatRateLimitFloorMs) {
    if (retryAfterSeconds > 0) {
        uint64_t requested = static_cast<uint64_t>(retryAfterSeconds) * 1000ULL;
        if (requested < floorMs) requested = floorMs;
        if (requested >= kRateLimitBackoffCapMs) return kRateLimitBackoffCapMs;
        return static_cast<uint32_t>(requested);
    }
    const uint8_t exponent = failures > 5 ? 5 : (failures == 0 ? 0 : static_cast<uint8_t>(failures - 1));
    uint32_t delayMs = 15000UL << exponent;
    if (delayMs < floorMs) delayMs = floorMs;
    const uint32_t cap = 5UL * 60UL * 1000UL;
    if (delayMs > cap) delayMs = cap;
    return withJitter(delayMs, cap, jitter);
}

inline int parseRetryAfterSeconds(const std::string& header) {
    size_t i = 0;
    while (i < header.size() && std::isspace(static_cast<unsigned char>(header[i]))) ++i;
    if (i >= header.size() || !std::isdigit(static_cast<unsigned char>(header[i]))) return -1;
    long value = 0;
    while (i < header.size() && std::isdigit(static_cast<unsigned char>(header[i]))) {
        value = value * 10 + (header[i] - '0');
        if (value > 24L * 60L * 60L) value = 24L * 60L * 60L;
        ++i;
    }
    while (i < header.size() && std::isspace(static_cast<unsigned char>(header[i]))) ++i;
    if (i != header.size()) return -1;
    return static_cast<int>(value);
}

inline bool jsonBoolFieldTrue(const std::string& json, const char* key) {
    const std::string pattern = std::string("\"") + key + "\"";
    size_t pos = 0;
    while ((pos = json.find(pattern, pos)) != std::string::npos) {
        size_t i = pos + pattern.size();
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (i >= json.size() || json[i] != ':') {
            pos += pattern.size();
            continue;
        }
        ++i;
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (json.compare(i, 4, "true") == 0) {
            const size_t end = i + 4;
            const bool boundary = end == json.size() ||
                                  !std::isalnum(static_cast<unsigned char>(json[end]));
            if (boundary) return true;
        }
        pos += pattern.size();
    }
    return false;
}

inline bool jsonStringFieldEquals(const std::string& json, const char* key, const char* expected) {
    const std::string pattern = std::string("\"") + key + "\"";
    const std::string want = std::string("\"") + expected + "\"";
    size_t pos = 0;
    while ((pos = json.find(pattern, pos)) != std::string::npos) {
        size_t i = pos + pattern.size();
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (i >= json.size() || json[i] != ':') {
            pos += pattern.size();
            continue;
        }
        ++i;
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (json.compare(i, want.size(), want) == 0) return true;
        pos += pattern.size();
    }
    return false;
}

inline bool commandsArrayHasCapture(const std::string& json) {
    const std::string key = "\"commands\"";
    const size_t pos = json.find(key);
    if (pos == std::string::npos) return false;
    size_t i = pos + key.size();
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
    if (i >= json.size() || json[i] != ':') return false;
    ++i;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
    if (i >= json.size() || json[i] != '[') return false;
    int depth = 0;
    for (; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        else if (json[i] == ']') {
            --depth;
            if (depth == 0) break;
        }
        if (depth > 0 && json.compare(i, 9, "\"capture\"") == 0) return true;
    }
    return false;
}

// captureNow:true is the contract. A commands entry named "capture" is also
// accepted so a heartbeat that queues {"commands":["capture"]} or
// {"commands":[{"type":"capture"}]} takes the same snapshot.
inline bool heartbeatRequestsCapture(const std::string& body) {
    if (jsonBoolFieldTrue(body, "captureNow")) return true;
    if (commandsArrayHasCapture(body)) return true;
    if (jsonStringFieldEquals(body, "type", "capture")) return true;
    if (jsonStringFieldEquals(body, "name", "capture")) return true;
    if (jsonStringFieldEquals(body, "command", "capture")) return true;
    return false;
}

inline bool splitLegacyProvision(const std::string& command, LegacyFields& payload) {
    const std::string prefix = "PROVISION:";
    if (command.compare(0, prefix.size(), prefix) != 0) return false;
    const std::string fields = command.substr(prefix.size());
    int offsets[4] = {-1, -1, -1, -1};
    int start = 0;
    for (int i = 0; i < 4; ++i) {
        const size_t found = fields.find('|', static_cast<size_t>(start));
        if (found == std::string::npos) return false;
        offsets[i] = static_cast<int>(found);
        start = offsets[i] + 1;
    }
    payload.ssid = fields.substr(0, static_cast<size_t>(offsets[0]));
    payload.password = fields.substr(static_cast<size_t>(offsets[0] + 1),
                                     static_cast<size_t>(offsets[1] - offsets[0] - 1));
    payload.cloudDeviceId = fields.substr(static_cast<size_t>(offsets[1] + 1),
                                          static_cast<size_t>(offsets[2] - offsets[1] - 1));
    payload.userId = fields.substr(static_cast<size_t>(offsets[2] + 1),
                                   static_cast<size_t>(offsets[3] - offsets[2] - 1));
    const size_t tokenStart = static_cast<size_t>(offsets[3] + 1);
    const size_t extra = fields.find('|', tokenStart);
    if (extra == std::string::npos) {
        payload.deviceToken = fields.substr(tokenStart);
        payload.piBaseUrl.clear();
    } else {
        payload.deviceToken = fields.substr(tokenStart, extra - tokenStart);
        payload.piBaseUrl = fields.substr(extra + 1);
    }
    return !payload.ssid.empty();
}

inline std::string firstNonEmpty(const std::string& a, const std::string& b, const std::string& c) {
    if (!a.empty()) return a;
    if (!b.empty()) return b;
    return c;
}

inline ProvisionDecision resolveProvision(const ProvisionInput& in) {
    ProvisionDecision out;
    out.ssid = trim(in.ssid);
    out.password = in.password;
    out.deviceToken = firstNonEmpty(trim(in.deviceToken), trim(in.heartbeatToken), "");
    out.piBaseUrl = trimTrailingSlashes(
        firstNonEmpty(trim(in.piBaseUrl), trim(in.serverBaseUrl), trim(in.ingestBaseUrl)));
    out.alias = trim(in.alias);
    out.adminToken = trim(in.adminToken);
    out.cloudDeviceId = trim(in.cloudDeviceId);
    out.userId = trim(in.userId);
    out.snapshotIntervalSec = kDefaultSnapshotIntervalSec;

    if (out.ssid.empty() || out.ssid.size() > 32 || !isDeviceToken(out.deviceToken)) return out;
    if (!out.alias.empty() && !isValidAlias(out.alias)) return out;
    if (!out.piBaseUrl.empty() && !isHttpUrl(out.piBaseUrl)) return out;
    if (!out.adminToken.empty() && out.adminToken.size() < kMinAdminTokenLength) return out;
    if (in.hasSnapshotInterval) {
        if (in.snapshotIntervalSec < 0 ||
            !snapshotIntervalAllowed(static_cast<uint32_t>(in.snapshotIntervalSec))) {
            return out;
        }
        out.snapshotIntervalSec = static_cast<uint32_t>(in.snapshotIntervalSec);
    }
    out.ok = true;
    return out;
}

}  // namespace SallyPolicy

#endif
