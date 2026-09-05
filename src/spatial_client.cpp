#include "spatial_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_camera.h>
#include <esp_system.h>

#include "spatial_protocol.h"

namespace {
String efuseDeviceId() {
    const uint64_t mac = ESP.getEfuseMac();
    char value[19];
    snprintf(value, sizeof(value), "sally-%04x%08x",
             static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
    return String(value);
}
}

void SallySpatialClient::begin(Preferences& preferences, SemaphoreHandle_t cameraMutex,
                               bool cameraAvailable, bool microphoneAvailable,
                               bool speakerAvailable) {
    _deviceId = efuseDeviceId();
    _cameraMutex = cameraMutex;
    _cameraAvailable = cameraAvailable;
    _microphoneAvailable = microphoneAvailable;
    _speakerAvailable = speakerAvailable;

    preferences.begin("heysalad", true);
    String configuredAlias = preferences.getString("sallyAlias", "");
    _piBaseUrl = SallySpatialProtocol::trimTrailingSlashes(
        preferences.getString("piBaseUrl", ""));
    _deviceToken = preferences.getString("deviceToken", "");
    preferences.end();

    _alias = SallySpatialProtocol::isValidAlias(configuredAlias) ? configuredAlias : _deviceId;
    Serial.printf("Sally Spatial device: %s (alias: %s)\n", _deviceId.c_str(), _alias.c_str());
    Serial.printf("Sally Spatial uplink: %s\n", isConfigured() ? "configured" : "disabled");
}

bool SallySpatialClient::isConfigured() const {
    return !_piBaseUrl.isEmpty() && !_deviceToken.isEmpty();
}

const String& SallySpatialClient::deviceId() const { return _deviceId; }
const String& SallySpatialClient::alias() const { return _alias; }

bool SallySpatialClient::beginRequest(HTTPClient& http, const String& url) {
    if (url.startsWith("https://")) {
        // M1: certificate validation deferred; uplink trust rests on the bearer token.
        static WiFiClientSecure tlsClient;
        tlsClient.setInsecure();
        http.setConnectTimeout(SallySpatialProtocol::HTTPS_TIMEOUT_MS);
        http.setTimeout(SallySpatialProtocol::HTTPS_TIMEOUT_MS);
        return http.begin(tlsClient, url);
    }
    http.setConnectTimeout(SallySpatialProtocol::HTTP_TIMEOUT_MS);
    http.setTimeout(SallySpatialProtocol::HTTP_TIMEOUT_MS);
    return http.begin(url);
}

void SallySpatialClient::addCommonHeaders(HTTPClient& http) {
    http.addHeader("Authorization", "Bearer " + _deviceToken);
    http.addHeader("X-Device-ID", _deviceId);
    http.addHeader("X-Device-Alias", _alias);
    http.addHeader("X-Firmware-Version", SallySpatialProtocol::FIRMWARE_VERSION);
    http.addHeader("X-Uptime-Ms", String(millis()));
    http.addHeader("X-RSSI", String(WiFi.RSSI()));
}

bool SallySpatialClient::postHeartbeat() {
    JsonDocument doc;
    doc["device_id"] = _deviceId;
    doc["alias"] = _alias;
    doc["firmware"] = SallySpatialProtocol::FIRMWARE_VERSION;
    doc["uptime_ms"] = millis();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["rssi"] = WiFi.RSSI();
    doc["camera_ok"] = _cameraAvailable;
    doc["mic_ok"] = _microphoneAvailable;
    doc["speaker_ok"] = _speakerAvailable;
    doc["frame_seq"] = _frameSequence;
    doc["dropped_frames"] = _droppedFrames;

    String payload;
    serializeJson(doc, payload);
    HTTPClient http;
    const String url = _piBaseUrl + "/v1/devices/" + _deviceId + "/heartbeat";
    if (!beginRequest(http, url)) return false;
    addCommonHeaders(http);
    http.addHeader("Content-Type", "application/json");
    const int status = http.POST(reinterpret_cast<uint8_t*>(payload.begin()), payload.length());
    http.end();
    return status >= 200 && status < 300;
}

bool SallySpatialClient::postFrame() {
    if (!_cameraAvailable || !_cameraMutex ||
        xSemaphoreTake(_cameraMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ++_droppedFrames;
        return false;
    }

    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) {
        xSemaphoreGive(_cameraMutex);
        ++_droppedFrames;
        return false;
    }

    HTTPClient http;
    const String url = _piBaseUrl + "/v1/devices/" + _deviceId + "/frames";
    bool successful = false;
    if (beginRequest(http, url)) {
        addCommonHeaders(http);
        http.addHeader("Content-Type", "image/jpeg");
        http.addHeader("X-Frame-Seq", String(_frameSequence + 1));
        const int status = http.POST(frame->buf, frame->len);
        successful = status >= 200 && status < 300;
        http.end();
    }

    esp_camera_fb_return(frame);
    xSemaphoreGive(_cameraMutex);
    if (successful) ++_frameSequence;
    else ++_droppedFrames;
    return successful;
}

void SallySpatialClient::recordAttempt(bool successful, uint32_t now) {
    if (successful) {
        _consecutiveFailures = 0;
        _retryAfter = 0;
        return;
    }
    if (_consecutiveFailures < 255) ++_consecutiveFailures;
    _retryAfter = now + SallySpatialProtocol::nextBackoffMs(_consecutiveFailures, esp_random());
}

void SallySpatialClient::loop() {
    if (!isConfigured() || WiFi.status() != WL_CONNECTED) return;
    const uint32_t now = millis();
    if (_retryAfter != 0 && static_cast<int32_t>(now - _retryAfter) < 0) return;

    if (now - _lastHeartbeatAttempt >= SallySpatialProtocol::HEARTBEAT_INTERVAL_MS) {
        _lastHeartbeatAttempt = now;
        recordAttempt(postHeartbeat(), now);
        return;
    }
    if (now - _lastFrameAttempt >= SallySpatialProtocol::FRAME_INTERVAL_MS) {
        _lastFrameAttempt = now;
        recordAttempt(postFrame(), now);
    }
}
