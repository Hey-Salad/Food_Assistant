#include "spatial_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_system.h>

#include "sally_policy.h"
#include "spatial_protocol.h"
#include "uplink_tls.h"

namespace {
String efuseDeviceId() {
    const uint64_t mac = ESP.getEfuseMac();
    char value[19];
    snprintf(value, sizeof(value), "sally-%04x%08x",
             static_cast<uint16_t>(mac >> 32), static_cast<uint32_t>(mac));
    return String(value);
}

bool due(uint32_t now, uint32_t last, uint32_t interval, bool started) {
    if (!started) return true;
    return static_cast<int32_t>(now - last) >= static_cast<int32_t>(interval);
}

bool blocked(uint32_t now, uint32_t retryAfter) {
    return retryAfter != 0 && static_cast<int32_t>(now - retryAfter) < 0;
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
    uint32_t snapshotSec = preferences.getUInt("snapshotSec", SallyPolicy::kDefaultSnapshotIntervalSec);
    preferences.end();
    if (!SallyPolicy::snapshotIntervalAllowed(snapshotSec)) {
        snapshotSec = SallyPolicy::kDefaultSnapshotIntervalSec;
    }
    _snapshotIntervalMs = snapshotSec * 1000UL;

    _alias = SallySpatialProtocol::isValidAlias(configuredAlias) ? configuredAlias : _deviceId;
    Serial.printf("Sally Spatial device: %s (alias: %s)\n", _deviceId.c_str(), _alias.c_str());
    Serial.printf("Sally Spatial uplink: %s, snapshot every %u s, heartbeat every %u s\n",
                  isConfigured() ? "configured" : "disabled",
                  snapshotIntervalSec(),
                  SallySpatialProtocol::HEARTBEAT_INTERVAL_MS / 1000);
}

bool SallySpatialClient::isConfigured() const {
    return SallyPolicy::cloudProvisioned(std::string(_piBaseUrl.c_str()),
                                         std::string(_deviceToken.c_str()));
}

const String& SallySpatialClient::deviceId() const { return _deviceId; }
const String& SallySpatialClient::alias() const { return _alias; }
uint32_t SallySpatialClient::snapshotIntervalSec() const { return _snapshotIntervalMs / 1000UL; }

bool SallySpatialClient::beginRequest(HTTPClient& http, const String& url) {
    if (url.startsWith("https://")) {
        static WiFiClientSecure tlsClient;
        static bool tlsReady = false;
        if (!tlsReady) {
            const bool usedBundle = SallyTls::configureClient(tlsClient);
            Serial.printf("TLS verification: %s\n", SallyTls::verificationLabel(usedBundle));
            tlsReady = true;
        }
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

int SallySpatialClient::readRetryAfter(HTTPClient& http) {
    return SallyPolicy::parseRetryAfterSeconds(std::string(http.header("Retry-After").c_str()));
}

SallyUplinkResult SallySpatialClient::postHeartbeat() {
    SallyUplinkResult result;
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
    doc["snapshot_interval_sec"] = snapshotIntervalSec();

    String payload;
    serializeJson(doc, payload);
    HTTPClient http;
    const String url = _piBaseUrl + "/v1/devices/" + _deviceId + "/heartbeat";
    if (!beginRequest(http, url)) return result;
    const char* headerKeys[] = {"Retry-After"};
    http.collectHeaders(headerKeys, 1);
    addCommonHeaders(http);
    http.addHeader("Content-Type", "application/json");
    result.status = http.POST(reinterpret_cast<uint8_t*>(payload.begin()), payload.length());
    if (result.status == 429) {
        result.retryAfterSec = readRetryAfter(http);
    } else if (result.status >= 200 && result.status < 300) {
        String body = http.getString();
        if (body.length() > 4096) body.remove(4096);
        result.captureNow = SallyPolicy::heartbeatRequestsCapture(std::string(body.c_str()));
    }
    http.end();
    return result;
}

SallyUplinkResult SallySpatialClient::postFrame() {
    SallyUplinkResult result;
    if (!_cameraAvailable || !_cameraMutex ||
        xSemaphoreTake(_cameraMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ++_droppedFrames;
        return result;
    }

    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) {
        xSemaphoreGive(_cameraMutex);
        ++_droppedFrames;
        return result;
    }
    if (!SallyPolicy::frameFits(frame->len)) {
        Serial.printf("Frame %u bytes exceeds %u byte limit, not uploaded\n",
                      static_cast<unsigned>(frame->len), SallyPolicy::kMaxFrameBytes);
        esp_camera_fb_return(frame);
        xSemaphoreGive(_cameraMutex);
        ++_droppedFrames;
        result.oversized = true;
        return result;
    }

    HTTPClient http;
    const String url = _piBaseUrl + "/v1/devices/" + _deviceId + "/frames";
    if (beginRequest(http, url)) {
        const char* headerKeys[] = {"Retry-After"};
        http.collectHeaders(headerKeys, 1);
        addCommonHeaders(http);
        http.addHeader("Content-Type", "image/jpeg");
        http.addHeader("X-Frame-Seq", String(_frameSequence + 1));
        result.status = http.POST(frame->buf, frame->len);
        if (result.status == 429) result.retryAfterSec = readRetryAfter(http);
        http.end();
    }

    esp_camera_fb_return(frame);
    xSemaphoreGive(_cameraMutex);
    if (result.status >= 200 && result.status < 300) ++_frameSequence;
    else ++_droppedFrames;
    return result;
}

void SallySpatialClient::noteResult(const SallyUplinkResult& result, uint32_t now, uint8_t& failures,
                                    uint32_t& retryAfter, uint32_t rateLimitFloorMs) {
    if (result.oversized) {
        retryAfter = now + SallyPolicy::kFrameMinGapMs;
        Serial.printf("Oversized frame dropped, next try in %u ms\n", SallyPolicy::kFrameMinGapMs);
        return;
    }
    if (result.status >= 200 && result.status < 300) {
        failures = 0;
        retryAfter = 0;
        return;
    }
    if (failures < 255) ++failures;
    const uint32_t delay = result.status == 429
                               ? SallyPolicy::rateLimitBackoffMs(result.retryAfterSec, failures,
                                                                 esp_random(), rateLimitFloorMs)
                               : SallyPolicy::failureBackoffMs(failures, esp_random());
    retryAfter = now + delay;
    Serial.printf("Uplink HTTP %d, next try in %u ms\n", result.status, delay);
}

void SallySpatialClient::loop() {
    if (!isConfigured() || WiFi.status() != WL_CONNECTED) return;
    const uint32_t now = millis();

    if (due(now, _lastHeartbeatAttempt, SallySpatialProtocol::HEARTBEAT_INTERVAL_MS, _heartbeatStarted) &&
        !blocked(now, _heartbeatRetryAfter)) {
        _heartbeatStarted = true;
        _lastHeartbeatAttempt = now;
        const SallyUplinkResult result = postHeartbeat();
        noteResult(result, millis(), _heartbeatFailures, _heartbeatRetryAfter,
                   SallyPolicy::kHeartbeatRateLimitFloorMs);
        if (result.captureNow && result.status >= 200 && result.status < 300) _captureNow = true;
    }

    const bool frameDue = due(now, _lastFrameAttempt, _snapshotIntervalMs, _frameStarted);
    if ((_captureNow || frameDue) && !blocked(now, _frameRetryAfter)) {
        const bool onDemand = _captureNow;
        _captureNow = false;
        _frameStarted = true;
        _lastFrameAttempt = now;
        const SallyUplinkResult result = postFrame();
        const uint32_t finished = millis();
        noteResult(result, finished, _frameFailures, _frameRetryAfter, SallyPolicy::kFrameMinGapMs);
        if (result.status >= 200 && result.status < 300) {
            // The server 429s a second frame inside 30s, including captureNow.
            _frameRetryAfter = finished + SallyPolicy::kFrameMinGapMs;
        }
        if (onDemand && !result.oversized && !(result.status >= 200 && result.status < 300)) {
            _captureNow = true;
        }
    }
}
