# Sally Spatial M1 firmware protocol

M1 connects each DFR1154 to a Raspberry Pi without placing Anthropic,
ElevenLabs, or other cloud-model credentials on the device.

## Identity and provisioning

Every board derives an immutable ID from its ESP32-S3 eFuse MAC:
`sally-<12 lowercase hexadecimal characters>`. A human alias such as
`sally-01` through `sally-04` may be provisioned independently.

The existing first-boot `/save` setup request accepts these additional JSON
fields. Once configured, this endpoint rejects further writes; reprovisioning
requires an explicit device reset/Preferences erase rather than accepting a
new token over the network.

```json
{
  "ssid": "example",
  "password": "redacted",
  "sally_alias": "sally-01",
  "pi_base_url": "https://sally-api-dev.heysalad-o.workers.dev/device",
  "device_token": "64 hexadecimal characters"
}
```

The token is stored in ESP32 Preferences and is never returned by an API or
written to serial output. Use a different random token for every device.

## Pi endpoints

### Frame ingest

`POST https://<sally-api>/device/v1/devices/{hwId}/frames`

Dev: `POST https://sally-api-dev.heysalad-o.workers.dev/device/v1/devices/{hwId}/frames`

- Body: raw JPEG (`Content-Type: image/jpeg`), maximum 2 MiB. Larger captures
  are dropped and not posted.
- `Authorization: Bearer <device token>` where the token is 32 bytes of hex
  (64 hexadecimal characters)
- `X-Device-ID` equal to the path `{hwId}`
- Also sent, and ignored by the server if unused: `X-Device-Alias`,
  `X-Firmware-Version`, `X-Frame-Seq`, `X-Uptime-Ms`, `X-RSSI`

Camera 1's hwId is `sally-a8c12904a7ac`.

Frames are attempted every 5 minutes by default (`snapshot_interval_sec` in
provisioning, allowed range 30–86400 seconds). The first snapshot is sent
once the uplink is up, then on that interval. A heartbeat response with
`captureNow: true`, or a command named `capture`, uploads one snapshot
immediately. A second frame within 30 seconds receives HTTP 429, so the
firmware waits at least 30 seconds after a frame 429 and after a successful
frame upload. A numeric `Retry-After` longer than 30 seconds is honored, up
to 15 minutes. Ordinary failures use exponential backoff capped at 30
seconds. That backoff applies only to the call that failed. The heartbeat
interval stays 15 seconds.

### Heartbeat

`POST https://<sally-api>/device/v1/devices/{hwId}/heartbeat` every 15 seconds
with JSON health, capabilities, frame counters, uptime, memory, and Wi-Fi
RSSI. `postHeartbeat()` reads the JSON body. `captureNow: true` queues a
frame upload. It uses the same bearer token and `X-Device-ID`.

## Security boundary

- Device tokens must be device-scoped, revocable, stored hashed on the Pi,
  compared in constant time, and rate limited.
- The Pi must reject a token used with a different path/header device ID.
- Limit JPEG body size and JSON body size before reading request bodies.
- LAN HTTP is suitable only for a trusted isolated prototype network. HTTPS
  uploads verify the peer certificate. The firmware uses the ESP32 Arduino
  x509 bundle when `_binary_x509_crt_bundle_start` is linked, and otherwise
  a built-in bundle of ISRG Root X1, ISRG Root X2, and Google Trust Services
  roots R1–R4 (the public roots Cloudflare and Vercel present). It does not
  call `setInsecure()`.
- OpenAI, ElevenLabs, Gemini, and DeepSeek keys are not stored on the ESP32.
  `GET /get_keys` and `POST /save_keys` are not registered. Older values in
  NVS are erased on boot. sally-api owns model inference.
- Camera capture is serialized with a FreeRTOS mutex so dashboard snapshots
  and Pi uploads cannot call the camera driver concurrently.

## M1 acceptance target

Four uniquely identified devices must remain online for 60 minutes, publish a
snapshot on the configured interval (default 5 minutes) and when a heartbeat
asks for one, reconnect after Pi/network restart, and report last-seen time,
RSSI, memory, frame sequence, and dropped frames.

## BLE compatibility with Sally iOS

Firmware advertises `Sally-Camera-<last six device hex characters>` and the
exact UUIDs used by `services/openClawBleService.ts`:

- Service: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- Command/write: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- Status/read+notify: `beb5483e-36e1-4688-b7f5-ea07361b26a9`

Current app commands `DEVICE_INFO`, `VERSION`, and the five-field
`PROVISION:ssid|password|cloudDeviceId|userId|heartbeatToken` are supported.
The immutable eFuse ID remains the canonical hardware identity; the cloud ID
is stored separately.

For complete provisioning, use the `PROVISION_JSON` object documented in the
README. The canonical fields are `ssid`, `password`, `device_token` (exactly
64 hexadecimal characters, a 32-byte token), and `pi_base_url` (the sally-api
device base: `https://<sally-api>/device`, or
`https://sally-api-dev.heysalad-o.workers.dev/device` on dev). Optional fields
are `device_id`, `alias`, `user_id`, `admin_token`, and
`snapshot_interval_sec` (default 300). `server_base_url` and
`ingest_base_url` are accepted aliases of `pi_base_url`. `heartbeat_token`
is an accepted alias of `device_token` and has the same 64-hex rule.

```text
PROVISION_JSON:{"ssid":"...","password":"...","device_id":"...","alias":"sally-01","user_id":"...","device_token":"...","pi_base_url":"https://sally-api.example/device","snapshot_interval_sec":300}
```

`DEVICE_INFO` and `STATUS` include `provisioned`, `cloud`, `rssi`,
`ble_locked`, and `snapshot_interval_sec`.

BLE provisioning is available for ten minutes after boot only when the device
has no Wi-Fi configuration. A saved SSID locks it, including Wi-Fi that came
from the `secrets.h` fallback. A successful Wi-Fi check stores the
configuration, locks BLE reprovisioning, acknowledges `PROVISION_OK`, and
restarts. Reopening requires holding the DFR1154 BOOT button (GPIO0) for 5
seconds while the firmware is running, which opens a new 10-minute window, or
holding it for 12 seconds, which erases the `heysalad` preferences and
restarts. There is no remote unlock command. Holding BOOT while pressing RST
enters the ROM bootloader and is not the unlock gesture.

Command writes and status reads require an encrypted, bonded LE Secure
Connections link. The board has no trustworthy display for numeric comparison,
so this is still a proximity/bootstrap control rather than strong mutual
attestation; the limited window and permanent lock are essential safeguards.

The current iOS pipe-delimited command cannot safely represent an SSID or
password containing `|`, and it does not pass the `heartbeatUrl`, a Pi base
URL, or a Sally alias. The app should migrate to `PROVISION_JSON` and pass its
returned heartbeat URL or an independently configured Pi URL.
