#ifndef SALLY_BLE_PROVISIONING_PROTOCOL_H
#define SALLY_BLE_PROVISIONING_PROTOCOL_H

#include <Arduino.h>

namespace SallyBleProtocol {

constexpr const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr const char* COMMAND_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
constexpr const char* STATUS_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";
constexpr uint32_t PROVISIONING_WINDOW_MS = 10UL * 60UL * 1000UL;

struct ProvisioningPayload {
    String ssid;
    String password;
    String cloudDeviceId;
    String alias;
    String userId;
    String deviceToken;
    String piBaseUrl;
};

inline bool splitLegacyProvision(const String& command, ProvisioningPayload& payload) {
    if (!command.startsWith("PROVISION:")) return false;
    String fields = command.substring(10);
    int offsets[5] = {-1, -1, -1, -1, -1};
    int start = 0;
    for (int i = 0; i < 4; ++i) {
        offsets[i] = fields.indexOf('|', start);
        if (offsets[i] < 0) return false;
        start = offsets[i] + 1;
    }
    payload.ssid = fields.substring(0, offsets[0]);
    payload.password = fields.substring(offsets[0] + 1, offsets[1]);
    payload.cloudDeviceId = fields.substring(offsets[1] + 1, offsets[2]);
    payload.userId = fields.substring(offsets[2] + 1, offsets[3]);
    offsets[4] = fields.indexOf('|', offsets[3] + 1);
    if (offsets[4] < 0) {
        payload.deviceToken = fields.substring(offsets[3] + 1);
    } else {
        payload.deviceToken = fields.substring(offsets[3] + 1, offsets[4]);
        payload.piBaseUrl = fields.substring(offsets[4] + 1);
    }
    return !payload.ssid.isEmpty();
}

inline bool isProvisioningSecretKey(const String& key) {
    return key == "password" || key == "device_token" || key == "heartbeat_token";
}

}  // namespace SallyBleProtocol

#endif
