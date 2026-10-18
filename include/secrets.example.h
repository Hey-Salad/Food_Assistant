// Copy to include/secrets.h and fill in. secrets.h is gitignored.
// These are only a fallback: anything already provisioned into NVS wins, so a
// device set up through the portal or over BLE is unaffected.
// Do not put OpenAI, ElevenLabs, Gemini, DeepSeek, or other model keys here.
// Those keys are not stored on the camera; inference runs server-side.
#pragma once
#define FALLBACK_WIFI_SSID     ""
#define FALLBACK_WIFI_PASSWORD ""
#define CAM_OTA_PASSWORD       "pick-something"
