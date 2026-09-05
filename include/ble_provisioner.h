#ifndef SALLY_BLE_PROVISIONER_H
#define SALLY_BLE_PROVISIONER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>

class SallyBleProvisioner {
public:
    bool begin(Preferences& preferences, const String& canonicalDeviceId,
               const String& currentAlias, bool cameraAvailable,
               bool microphoneAvailable, bool speakerAvailable);
    void loop();

private:
    class CommandCallbacks;
    void handleCommand(const String& command);
    void sendStatus(const String& status);
    void sendDeviceInfo();
    void provisionLegacy(const String& command);
    void provisionJson(const String& json);
    void applyProvisioning(const struct SallyBleProvisioningData& data);
    bool provisioningAllowed() const;

    Preferences* _preferences = nullptr;
    NimBLECharacteristic* _statusCharacteristic = nullptr;
    String _deviceId;
    String _alias;
    bool _cameraAvailable = false;
    bool _microphoneAvailable = false;
    bool _speakerAvailable = false;
    bool _locked = false;
    bool _restartPending = false;
    uint32_t _restartAt = 0;
    uint32_t _windowStartedAt = 0;
};

#endif
