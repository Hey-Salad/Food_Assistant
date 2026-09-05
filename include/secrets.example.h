// Copy to include/secrets.h and fill in. secrets.h is gitignored.
// These are only a fallback: anything already provisioned into NVS wins, so a
// device set up through the portal or over BLE is unaffected.
#pragma once
#define FALLBACK_WIFI_SSID     ""
#define FALLBACK_WIFI_PASSWORD ""
#define CAM_OTA_PASSWORD       "pick-something"
