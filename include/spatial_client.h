#ifndef SALLY_SPATIAL_CLIENT_H
#define SALLY_SPATIAL_CLIENT_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SallyUplinkResult {
    int status = -1;
    int retryAfterSec = -1;
    bool captureNow = false;
    bool oversized = false;
};

class SallySpatialClient {
public:
    void begin(Preferences& preferences, SemaphoreHandle_t cameraMutex, bool cameraAvailable,
               bool microphoneAvailable, bool speakerAvailable);
    void loop();
    bool isConfigured() const;
    const String& deviceId() const;
    const String& alias() const;
    uint32_t snapshotIntervalSec() const;

private:
    SallyUplinkResult postHeartbeat();
    SallyUplinkResult postFrame();
    bool beginRequest(class HTTPClient& http, const String& url);
    void addCommonHeaders(class HTTPClient& http);
    void noteResult(const SallyUplinkResult& result, uint32_t now, uint8_t& failures,
                    uint32_t& retryAfter, uint32_t rateLimitFloorMs);
    static int readRetryAfter(class HTTPClient& http);

    String _deviceId;
    String _alias;
    String _piBaseUrl;
    String _deviceToken;
    SemaphoreHandle_t _cameraMutex = nullptr;
    bool _cameraAvailable = false;
    bool _microphoneAvailable = false;
    bool _speakerAvailable = false;
    uint32_t _snapshotIntervalMs = 300000;
    uint32_t _frameSequence = 0;
    uint32_t _droppedFrames = 0;
    uint32_t _lastFrameAttempt = 0;
    uint32_t _lastHeartbeatAttempt = 0;
    uint32_t _heartbeatRetryAfter = 0;
    uint32_t _frameRetryAfter = 0;
    uint8_t _heartbeatFailures = 0;
    uint8_t _frameFailures = 0;
    bool _captureNow = false;
    bool _heartbeatStarted = false;
    bool _frameStarted = false;
};

#endif
