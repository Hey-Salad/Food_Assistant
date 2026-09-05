#ifndef SALLY_SPATIAL_CLIENT_H
#define SALLY_SPATIAL_CLIENT_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SallySpatialClient {
public:
    void begin(Preferences& preferences, SemaphoreHandle_t cameraMutex, bool cameraAvailable,
               bool microphoneAvailable, bool speakerAvailable);
    void loop();
    bool isConfigured() const;
    const String& deviceId() const;
    const String& alias() const;

private:
    bool postHeartbeat();
    bool postFrame();
    bool beginRequest(class HTTPClient& http, const String& url);
    void addCommonHeaders(class HTTPClient& http);
    void recordAttempt(bool successful, uint32_t now);

    String _deviceId;
    String _alias;
    String _piBaseUrl;
    String _deviceToken;
    SemaphoreHandle_t _cameraMutex = nullptr;
    bool _cameraAvailable = false;
    bool _microphoneAvailable = false;
    bool _speakerAvailable = false;
    uint32_t _frameSequence = 0;
    uint32_t _droppedFrames = 0;
    uint32_t _lastFrameAttempt = 0;
    uint32_t _lastHeartbeatAttempt = 0;
    uint32_t _retryAfter = 0;
    uint8_t _consecutiveFailures = 0;
};

#endif
