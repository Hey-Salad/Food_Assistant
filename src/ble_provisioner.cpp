#include "ble_provisioner.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "ble_provisioning_protocol.h"
#include "spatial_protocol.h"

struct SallyBleProvisioningData {
    String ssid;
    String password;
    String cloudDeviceId;
    String alias;
    String userId;
    String deviceToken;
    String piBaseUrl;
};

class SallyBleProvisioner::CommandCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit CommandCallbacks(SallyBleProvisioner* owner) : _owner(owner) {}
    void onWrite(NimBLECharacteristic* characteristic) override {
        const std::string value = characteristic->getValue();
        if (!value.empty()) _owner->handleCommand(String(value.c_str()));
    }
private:
    SallyBleProvisioner* _owner;
};

bool SallyBleProvisioner::begin(Preferences& preferences, const String& canonicalDeviceId,
                                const String& currentAlias, bool cameraAvailable,
                                bool microphoneAvailable, bool speakerAvailable) {
    _preferences = &preferences;
    _deviceId = canonicalDeviceId;
    _alias = currentAlias;
    _cameraAvailable = cameraAvailable;
    _microphoneAvailable = microphoneAvailable;
    _speakerAvailable = speakerAvailable;
    _windowStartedAt = millis();

    preferences.begin("heysalad", true);
    _locked = preferences.getBool("bleProvLocked", false);
    preferences.end();

    const String suffix = _deviceId.substring(_deviceId.length() > 6 ? _deviceId.length() - 6 : 0);
    const String advertisedName = "Sally-Camera-" + suffix;
    NimBLEDevice::init(advertisedName.c_str());
    NimBLEDevice::setMTU(512);
    NimBLEDevice::setSecurityAuth(true, false, true);  // Bonding + LE Secure Connections.
    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService(SallyBleProtocol::SERVICE_UUID);
    NimBLECharacteristic* command = service->createCharacteristic(
        SallyBleProtocol::COMMAND_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
    _statusCharacteristic = service->createCharacteristic(
        SallyBleProtocol::STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC);
    command->setCallbacks(new CommandCallbacks(this));
    _statusCharacteristic->setValue(_locked ? "PROVISION_LOCKED" : "PROVISION_READY");
    service->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SallyBleProtocol::SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();
    Serial.printf("BLE provisioning: %s as %s\n", _locked ? "locked" : "available", advertisedName.c_str());
    return true;
}

bool SallyBleProvisioner::provisioningAllowed() const {
    return !_locked && millis() - _windowStartedAt <= SallyBleProtocol::PROVISIONING_WINDOW_MS;
}

void SallyBleProvisioner::sendStatus(const String& status) {
    if (!_statusCharacteristic) return;
    _statusCharacteristic->setValue(status.c_str());
    _statusCharacteristic->notify();
}

void SallyBleProvisioner::sendDeviceInfo() {
    JsonDocument doc;
    doc["device_id"] = _deviceId;
    doc["firmware"] = SallySpatialProtocol::FIRMWARE_VERSION;
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi_ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
    doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    doc["ws_connected"] = false;
    doc["streaming"] = false;
    doc["night_vision"] = false;
    doc["frames_sent"] = 0;
    doc["uptime_sec"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cloud_heartbeat"] = false;
    String response;
    serializeJson(doc, response);
    sendStatus(response);
}

void SallyBleProvisioner::handleCommand(const String& command) {
    if (command == "DEVICE_INFO") { sendDeviceInfo(); return; }
    if (command == "STATUS") { sendDeviceInfo(); return; }
    if (command == "VERSION") {
        sendStatus("VERSION:" + String(SallySpatialProtocol::FIRMWARE_VERSION));
        return;
    }
    if (command.startsWith("PROVISION_JSON:")) {
        provisionJson(command.substring(15));
        return;
    }
    if (command.startsWith("PROVISION:")) {
        provisionLegacy(command);
        return;
    }
    sendStatus("ERROR:UNKNOWN_COMMAND");
}

void SallyBleProvisioner::provisionLegacy(const String& command) {
    SallyBleProtocol::ProvisioningPayload parsed;
    if (!SallyBleProtocol::splitLegacyProvision(command, parsed)) {
        sendStatus("PROVISION_BAD_FORMAT");
        return;
    }
    SallyBleProvisioningData data;
    data.ssid = parsed.ssid;
    data.password = parsed.password;
    data.cloudDeviceId = parsed.cloudDeviceId;
    data.userId = parsed.userId;
    data.deviceToken = parsed.deviceToken;
    data.piBaseUrl = parsed.piBaseUrl;
    applyProvisioning(data);
}

void SallyBleProvisioner::provisionJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        sendStatus("PROVISION_BAD_FORMAT");
        return;
    }
    SallyBleProvisioningData data;
    data.ssid = doc["ssid"] | "";
    data.password = doc["password"] | "";
    data.cloudDeviceId = doc["device_id"] | "";
    data.alias = doc["alias"] | "";
    data.userId = doc["user_id"] | "";
    data.deviceToken = doc["device_token"] | "";
    if (data.deviceToken.isEmpty()) data.deviceToken = doc["heartbeat_token"] | "";
    data.piBaseUrl = SallySpatialProtocol::trimTrailingSlashes(doc["pi_base_url"] | "");
    applyProvisioning(data);
}

void SallyBleProvisioner::applyProvisioning(const SallyBleProvisioningData& data) {
    if (!provisioningAllowed()) {
        sendStatus(_locked ? "PROVISION_LOCKED" : "PROVISION_WINDOW_CLOSED");
        return;
    }
    if (data.ssid.isEmpty() || data.deviceToken.length() < 32 ||
        (!data.alias.isEmpty() && !SallySpatialProtocol::isValidAlias(data.alias)) ||
        (!data.piBaseUrl.isEmpty() && !data.piBaseUrl.startsWith("http://") &&
         !data.piBaseUrl.startsWith("https://"))) {
        sendStatus("PROVISION_BAD_FORMAT");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(data.ssid.c_str(), data.password.c_str());
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(250);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        sendStatus("PROVISION_WIFI_FAILED");
        return;
    }

    _preferences->begin("heysalad", false);
    _preferences->putString("ssid", data.ssid);
    _preferences->putString("password", data.password);
    if (!data.cloudDeviceId.isEmpty()) _preferences->putString("cloudDeviceId", data.cloudDeviceId);
    if (!data.alias.isEmpty()) _preferences->putString("sallyAlias", data.alias);
    if (!data.userId.isEmpty()) _preferences->putString("userId", data.userId);
    _preferences->putString("deviceToken", data.deviceToken);
    if (!data.piBaseUrl.isEmpty()) _preferences->putString("piBaseUrl", data.piBaseUrl);
    _preferences->putBool("configured", true);
    _preferences->putBool("bleProvLocked", true);
    _preferences->end();
    _locked = true;
    sendStatus("PROVISION_OK");
    _restartPending = true;
    _restartAt = millis() + 1500;
}

void SallyBleProvisioner::loop() {
    if (_restartPending && static_cast<int32_t>(millis() - _restartAt) >= 0) ESP.restart();
}
