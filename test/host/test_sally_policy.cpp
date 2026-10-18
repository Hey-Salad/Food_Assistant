#include "sally_policy.h"

#include <iostream>
#include <string>

namespace {

int g_failed = 0;

void expect(bool condition, const std::string& label) {
    if (condition) return;
    std::cerr << "FAIL " << label << "\n";
    ++g_failed;
}

std::string token32() { return std::string(32, 'a'); }
std::string token64(char digit = 'a') { return std::string(SallyPolicy::kDeviceTokenHexChars, digit); }

}  // namespace

int main() {
    using namespace SallyPolicy;

    expect(kDefaultSnapshotIntervalSec == 300, "default snapshot is 5 minutes");
    expect(kHeartbeatIntervalMs == 15000, "heartbeat stays at 15s");
    expect(kBootButtonGpio == 0, "DFR1154 BOOT is GPIO0");
    expect(kUnlockHoldMs == 5000, "unlock hold is 5s");
    expect(kFactoryResetHoldMs == 12000, "factory reset hold is 12s");
    expect(bootButtonOnRelease(4999) == BootButtonAction::None, "short hold does nothing");
    expect(bootButtonOnRelease(5000) == BootButtonAction::UnlockProvisioning, "5s hold unlocks");
    expect(bootButtonOnRelease(11999) == BootButtonAction::UnlockProvisioning, "hold under 12s unlocks");
    expect(bootButtonOnRelease(12000) == BootButtonAction::FactoryReset, "12s hold factory resets");
    expect(bootButtonFactoryResetWhileHeld(12000), "factory reset can fire while still held");
    expect(!bootButtonFactoryResetWhileHeld(5000), "unlock hold is not a factory reset");

    expect(!provisioningLockedOnBoot(false, false), "fresh device stays open");
    expect(provisioningLockedOnBoot(true, false), "any Wi-Fi config locks provisioning");
    expect(provisioningLockedOnBoot(false, true), "stored lock stays locked");

    expect(isRetiredModelKey("openai"), "openai key is retired");
    expect(isRetiredModelKey("elevenlabs"), "elevenlabs key is retired");
    expect(isRetiredModelKey("gemini"), "gemini key is retired");
    expect(isRetiredModelKey("deepseek"), "deepseek key is retired");
    expect(!isRetiredModelKey("deviceToken"), "device token is not a model key");

    const std::string admin = token32();
    expect(lanAdminAccess(true, admin, admin) == LanAdminDecision::Disabled,
           "cloud provisioning disables LAN admin");
    expect(lanAdminAccess(false, "", admin) == LanAdminDecision::Unauthorized,
           "missing admin token fails closed");
    expect(lanAdminAccess(false, admin, admin) == LanAdminDecision::Allow, "matching admin token allows");
    expect(lanAdminAccess(false, admin, admin + "b") == LanAdminDecision::Unauthorized,
           "wrong admin token is rejected");
    expect(lanAdminAccess(false, "short", "short") == LanAdminDecision::Unauthorized,
           "short admin token is rejected");
    expect(constantTimeEquals(admin, admin), "equal tokens match");
    expect(!constantTimeEquals(admin, std::string(32, 'b')), "different tokens do not match");
    expect(!constantTimeEquals("abc", "abcd"), "different lengths do not match");

    expect(!heartbeatRequestsCapture("{\"captureNow\":false}"), "false does not capture");
    expect(heartbeatRequestsCapture("{\"captureNow\":true}"), "captureNow true");
    expect(heartbeatRequestsCapture("{\n  \"captureNow\": true\n}"), "captureNow with space");
    expect(!heartbeatRequestsCapture("{\"captureNow\":\"true\"}"), "string true is not a capture");
    expect(!heartbeatRequestsCapture("{\"captureNow\":trueish}"), "trueish is not true");
    expect(!heartbeatRequestsCapture("{\"notcaptureNow\":true}"), "key must be captureNow");
    expect(heartbeatRequestsCapture("{\"commands\":[\"capture\"]}"), "commands array capture");
    expect(!heartbeatRequestsCapture("{\"commands\":[\"status\"]}"), "other commands do not capture");
    expect(heartbeatRequestsCapture("{\"commands\":[{\"type\":\"capture\"}]}"), "command type capture");
    expect(heartbeatRequestsCapture("{\"commands\":[\"ignored\"],\"command\":\"capture\"}"),
           "command string capture");
    expect(!heartbeatRequestsCapture("{\"ok\":true}"), "heartbeat without capture stays idle");

    expect(parseRetryAfterSeconds("120") == 120, "numeric Retry-After");
    expect(parseRetryAfterSeconds("  15  ") == 15, "padded Retry-After");
    expect(parseRetryAfterSeconds("Wed, 21 Oct 2015 07:28:00 GMT") == -1, "HTTP-date is not seconds");
    expect(parseRetryAfterSeconds("") == -1, "empty Retry-After");
    expect(kMaxFrameBytes == 2u * 1024u * 1024u, "frame cap is 2 MiB");
    expect(kFrameMinGapMs == 30000, "frame gap is 30s");
    expect(frameFits(1), "a small jpeg fits");
    expect(frameFits(kMaxFrameBytes), "2 MiB fits");
    expect(!frameFits(kMaxFrameBytes + 1), "over 2 MiB does not fit");
    expect(!frameFits(0), "empty body is not a frame");
    expect(rateLimitBackoffMs(120, 1, 0) == 120000, "Retry-After is honored");
    expect(rateLimitBackoffMs(3600, 1, 0) == kRateLimitBackoffCapMs, "Retry-After is capped");
    expect(rateLimitBackoffMs(-1, 1, 0) >= 15000, "429 without Retry-After waits at least 15s");
    expect(rateLimitBackoffMs(-1, 1, 0) > failureBackoffMs(1, 0), "429 waits longer than a normal failure");
    expect(rateLimitBackoffMs(-1, 1, 0, kFrameMinGapMs) >= kFrameMinGapMs,
           "frame 429 waits at least 30s");
    expect(rateLimitBackoffMs(10, 1, 0, kFrameMinGapMs) == kFrameMinGapMs,
           "short Retry-After does not beat the frame gap");
    expect(rateLimitBackoffMs(45, 1, 0, kFrameMinGapMs) == 45000,
           "longer Retry-After is kept for frames");
    expect(failureBackoffMs(1, 0) == 2000, "first ordinary failure matches the previous backoff");

    ProvisionInput input;
    input.ssid = "  House ";
    input.password = "secret";
    input.deviceToken = token64();
    input.piBaseUrl = "https://sally.example/device/";
    input.alias = "sally-01";
    input.adminToken = token32();
    input.snapshotIntervalSec = 300;
    input.hasSnapshotInterval = true;
    ProvisionDecision ok = resolveProvision(input);
    expect(ok.ok, "canonical PROVISION_JSON is accepted");
    expect(ok.ssid == "House", "ssid is trimmed");
    expect(ok.piBaseUrl == "https://sally.example/device", "trailing slash is stripped");
    expect(ok.snapshotIntervalSec == 300, "snapshot interval is kept");
    expect(ok.adminToken.size() == 32, "admin token is kept");

    input.piBaseUrl.clear();
    input.serverBaseUrl = "https://api.example/device";
    ok = resolveProvision(input);
    expect(ok.ok && ok.piBaseUrl == "https://api.example/device", "server_base_url is accepted");

    input.serverBaseUrl.clear();
    input.ingestBaseUrl = "https://ingest.example/device";
    ok = resolveProvision(input);
    expect(ok.ok && ok.piBaseUrl == "https://ingest.example/device", "ingest_base_url is accepted");

    input.piBaseUrl = "https://canonical.example/device";
    ok = resolveProvision(input);
    expect(ok.piBaseUrl == "https://canonical.example/device", "pi_base_url wins over aliases");

    input.deviceToken.clear();
    input.heartbeatToken = token64();
    input.piBaseUrl = "https://canonical.example/device";
    ok = resolveProvision(input);
    expect(ok.ok && ok.deviceToken == input.heartbeatToken, "heartbeat_token alias");

    input.deviceToken = "too-short";
    input.heartbeatToken.clear();
    expect(!resolveProvision(input).ok, "short device token is rejected");
    input.deviceToken = token64('g');
    expect(!resolveProvision(input).ok, "non-hex device token is rejected");
    input.deviceToken = token64().substr(0, 63);
    expect(!resolveProvision(input).ok, "63 hex digits are rejected");
    input.deviceToken = token64() + "ab";
    expect(!resolveProvision(input).ok, "66 hex digits are rejected");
    input.deviceToken = token64('A');
    expect(resolveProvision(input).ok, "uppercase hex device token is accepted");

    input.deviceToken = token64();
    input.piBaseUrl = "ftp://nope";
    expect(!resolveProvision(input).ok, "non-http URL is rejected");

    input.piBaseUrl = "https://ok.example";
    input.hasSnapshotInterval = true;
    input.snapshotIntervalSec = 1;
    expect(!resolveProvision(input).ok, "1 second snapshot interval is rejected");
    input.snapshotIntervalSec = 86400;
    expect(resolveProvision(input).ok, "24h snapshot interval is accepted");
    input.hasSnapshotInterval = false;
    expect(resolveProvision(input).snapshotIntervalSec == 300, "omitted interval defaults to 300");

    input.alias = "Sally";
    expect(!resolveProvision(input).ok, "uppercase alias is rejected");
    input.alias.clear();
    input.adminToken = "short";
    expect(!resolveProvision(input).ok, "short admin token is rejected");
    input.adminToken.clear();
    expect(resolveProvision(input).ok, "admin token may be omitted");
    input.ssid = std::string(33, 'n');
    expect(!resolveProvision(input).ok, "ssid longer than 32 is rejected");
    input.ssid.clear();
    expect(!resolveProvision(input).ok, "ssid is required");

    LegacyFields legacy;
    expect(splitLegacyProvision(
               "PROVISION:net|p@ss|cloud-id|user-1|" + token64() + "|https://pi.local", legacy),
           "legacy provision parses");
    expect(legacy.ssid == "net" && legacy.password == "p@ss" && legacy.cloudDeviceId == "cloud-id",
           "legacy fields");
    expect(legacy.piBaseUrl == "https://pi.local", "legacy URL");
    expect(!splitLegacyProvision("PROVISION:only-one", legacy), "legacy requires four separators");

    expect(isDeviceToken(token64()), "64 hex digits is a device token");
    expect(isDeviceToken(token64('F')), "uppercase hex is a device token");
    expect(!isDeviceToken(token32()), "32 characters is not a 32-byte token");
    expect(!isDeviceToken(token64('z')), "non-hex is not a device token");
    expect(cloudProvisioned("https://sally.example/device", token64()), "cloud when url and token exist");
    expect(!cloudProvisioned("", token64()), "wifi-only is not cloud provisioned");
    expect(!cloudProvisioned("https://sally.example", token32()), "short token is not cloud provisioned");

    if (g_failed) {
        std::cerr << g_failed << " failed\n";
        return 1;
    }
    std::cout << "sally policy tests passed\n";
    return 0;
}
