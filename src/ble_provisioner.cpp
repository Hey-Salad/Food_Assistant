#include "ble_provisioner.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "ble_provisioning_protocol.h"
#include "config.h"
#include "sally_policy.h"
#include "spatial_protocol.h"

struct SallyBleProvisioningData {
    String ssid;
    String password;
    String cloudDeviceId;
    String alias;
    String userId;
    String deviceToken;
    String piBaseUrl;
    String adminToken;
    bool hasSnapshotInterval = false;
    int64_t snapshotIntervalSec = 0;
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

namespace {
String jsonString(JsonDocument& doc, const char* key) {
    if (doc[key].isNull()) return "";
    return String(doc[key].as<const char*>() ? doc[key].as<const char*>() : "");
}
}

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
    pinMode(SallyPolicy::kBootButtonGpio, INPUT_PULLUP);

    preferences.begin("heysalad", false);
    _wifiConfigured = preferences.getString("ssid", "").length() > 0;
    _storedLock = preferences.getBool("bleProvLocked", false);
    if (_wifiConfigured && !_storedLock) {
        preferences.putBool("bleProvLocked", true);
        _storedLock = true;
    }
    preferences.end();
    _locked = SallyPolicy::provisioningLockedOnBoot(_wifiConfigured, _storedLock);

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
    if (_locked) {
        Serial.println("Hold BOOT (GPIO0) for 5s to unlock provisioning, or 12s to factory reset.");
    }
    return true;
}

bool SallyBleProvisioner::provisioningAllowed() const {
    if (_locked) return false;
    return millis() - _windowStartedAt <= SallyBleProtocol::PROVISIONING_WINDOW_MS;
}

void SallyBleProvisioner::sendStatus(const String& status) {
    if (!_statusCharacteristic) return;
    _statusCharacteristic->setValue(status.c_str());
    _statusCharacteristic->notify();
}

void SallyBleProvisioner::sendDeviceInfo() {
    _preferences->begin("heysalad", true);
    const String baseUrl = _preferences->getString("piBaseUrl", "");
    const String token = _preferences->getString("deviceToken", "");
    const uint32_t snapshotSec = _preferences->getUInt("snapshotSec", SallyPolicy::kDefaultSnapshotIntervalSec);
    _preferences->end();

    JsonDocument doc;
    doc["device_id"] = _deviceId;
    doc["firmware"] = SallySpatialProtocol::FIRMWARE_VERSION;
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi_ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
    doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    doc["provisioned"] = _wifiConfigured;
    doc["cloud"] = SallyPolicy::cloudProvisioned(std::string(baseUrl.c_str()), std::string(token.c_str()));
    doc["ble_locked"] = _locked;
    doc["provisioning_open"] = provisioningAllowed();
    doc["snapshot_interval_sec"] = snapshotSec;
    doc["ws_connected"] = false;
    doc["streaming"] = false;
    doc["night_vision"] = false;
    doc["frames_sent"] = 0;
    doc["uptime_sec"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cloud_heartbeat"] = doc["cloud"];
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
    data.ssid = jsonString(doc, "ssid");
    data.password = jsonString(doc, "password");
    data.cloudDeviceId = jsonString(doc, "device_id");
    data.alias = jsonString(doc, "alias");
    data.userId = jsonString(doc, "user_id");
    data.deviceToken = jsonString(doc, "device_token");
    if (data.deviceToken.isEmpty()) data.deviceToken = jsonString(doc, "heartbeat_token");
    data.piBaseUrl = jsonString(doc, "pi_base_url");
    if (data.piBaseUrl.isEmpty()) data.piBaseUrl = jsonString(doc, "server_base_url");
    if (data.piBaseUrl.isEmpty()) data.piBaseUrl = jsonString(doc, "ingest_base_url");
    data.adminToken = jsonString(doc, "admin_token");
    if (!doc["snapshot_interval_sec"].isNull()) {
        if (!doc["snapshot_interval_sec"].is<int>() && !doc["snapshot_interval_sec"].is<long>()) {
            sendStatus("PROVISION_BAD_FORMAT");
            return;
        }
        data.hasSnapshotInterval = true;
        data.snapshotIntervalSec = doc["snapshot_interval_sec"].as<long>();
    }
    applyProvisioning(data);
}

void SallyBleProvisioner::applyProvisioning(const SallyBleProvisioningData& data) {
    if (!provisioningAllowed()) {
        sendStatus(_locked ? "PROVISION_LOCKED" : "PROVISION_WINDOW_CLOSED");
        return;
    }
    SallyPolicy::ProvisionInput input;
    input.ssid = data.ssid.c_str();
    input.password = data.password.c_str();
    input.deviceToken = data.deviceToken.c_str();
    input.piBaseUrl = data.piBaseUrl.c_str();
    input.alias = data.alias.c_str();
    input.adminToken = data.adminToken.c_str();
    input.cloudDeviceId = data.cloudDeviceId.c_str();
    input.userId = data.userId.c_str();
    input.hasSnapshotInterval = data.hasSnapshotInterval;
    input.snapshotIntervalSec = data.snapshotIntervalSec;
    const SallyPolicy::ProvisionDecision decision = SallyPolicy::resolveProvision(input);
    if (!decision.ok) {
        sendStatus("PROVISION_BAD_FORMAT");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(decision.ssid.c_str(), decision.password.c_str());
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(250);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        sendStatus("PROVISION_WIFI_FAILED");
        return;
    }

    _preferences->begin("heysalad", false);
    _preferences->putString("ssid", decision.ssid.c_str());
    _preferences->putString("password", decision.password.c_str());
    if (!decision.cloudDeviceId.empty()) _preferences->putString("cloudDeviceId", decision.cloudDeviceId.c_str());
    if (!decision.alias.empty()) _preferences->putString("sallyAlias", decision.alias.c_str());
    if (!decision.userId.empty()) _preferences->putString("userId", decision.userId.c_str());
    _preferences->putString("deviceToken", decision.deviceToken.c_str());
    if (!decision.piBaseUrl.empty()) _preferences->putString("piBaseUrl", decision.piBaseUrl.c_str());
    else _preferences->remove("piBaseUrl");
    if (!decision.adminToken.empty()) _preferences->putString("adminToken", decision.adminToken.c_str());
    else _preferences->remove("adminToken");
    _preferences->putUInt("snapshotSec", decision.snapshotIntervalSec);
    _preferences->putBool("configured", true);
    _preferences->putBool("bleProvLocked", true);
    _preferences->end();
    _locked = true;
    _wifiConfigured = true;
    _storedLock = true;
    sendStatus("PROVISION_OK");
    _restartPending = true;
    _restartAt = millis() + 1500;
}

void SallyBleProvisioner::unlockFromButton() {
    _locked = false;
    _windowStartedAt = millis();
    Serial.println("BOOT held 5s: provisioning unlocked for 10 minutes");
    sendStatus("PROVISION_READY");
}

void SallyBleProvisioner::factoryReset() {
    Serial.println("BOOT held 12s: factory reset, clearing Wi-Fi and cloud settings");
    _preferences->begin("heysalad", false);
    _preferences->clear();
    _preferences->end();
    delay(200);
    ESP.restart();
}

void SallyBleProvisioner::pollButton() {
    const bool pressed = digitalRead(SallyPolicy::kBootButtonGpio) == LOW;
    const uint32_t now = millis();
    if (pressed) {
        if (_buttonDownSince == 0) _buttonDownSince = now;
        const uint32_t held = now - _buttonDownSince;
        if (held > 1000) digitalWrite(LED_GPIO_NUM, HIGH);
        if (!_factoryResetStarted && SallyPolicy::bootButtonFactoryResetWhileHeld(held)) {
            _factoryResetStarted = true;
            factoryReset();
        }
        return;
    }
    if (_buttonDownSince == 0) return;
    const uint32_t held = now - _buttonDownSince;
    _buttonDownSince = 0;
    digitalWrite(LED_GPIO_NUM, LOW);
    if (_factoryResetStarted) return;
    if (SallyPolicy::bootButtonOnRelease(held) == SallyPolicy::BootButtonAction::UnlockProvisioning) {
        unlockFromButton();
    }
}

void SallyBleProvisioner::loop() {
    pollButton();
    if (_restartPending && static_cast<int32_t>(millis() - _restartAt) >= 0) ESP.restart();
}
