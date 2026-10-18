#ifndef SALLY_BLE_PROVISIONING_PROTOCOL_H
#define SALLY_BLE_PROVISIONING_PROTOCOL_H

#include <Arduino.h>

#include "sally_policy.h"

namespace SallyBleProtocol {

constexpr const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr const char* COMMAND_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
constexpr const char* STATUS_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";
constexpr uint32_t PROVISIONING_WINDOW_MS = SallyPolicy::kProvisioningWindowMs;

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
    SallyPolicy::LegacyFields fields;
    if (!SallyPolicy::splitLegacyProvision(std::string(command.c_str()), fields)) return false;
    payload.ssid = fields.ssid.c_str();
    payload.password = fields.password.c_str();
    payload.cloudDeviceId = fields.cloudDeviceId.c_str();
    payload.userId = fields.userId.c_str();
    payload.deviceToken = fields.deviceToken.c_str();
    payload.piBaseUrl = fields.piBaseUrl.c_str();
    return true;
}

inline bool isProvisioningSecretKey(const String& key) {
    return key == "password" || key == "device_token" || key == "heartbeat_token";
}

}  // namespace SallyBleProtocol

#endif
