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
  "pi_base_url": "http://raspbx.local:8080",
  "device_token": "redacted"
}
```

The token is stored in ESP32 Preferences and is never returned by an API or
written to serial output. Use a different random token for every device.

## Pi endpoints

### Frame ingest

`POST /v1/devices/{device_id}/frames`

- Body: raw JPEG (`Content-Type: image/jpeg`)
- `Authorization: Bearer <device token>`
- `X-Device-ID`, `X-Device-Alias`, `X-Firmware-Version`
- `X-Frame-Seq`, `X-Uptime-Ms`, `X-RSSI`

Frames are attempted at 1 FPS. Network operations time out after three
seconds. Failures use exponential backoff capped at 30 seconds.

### Heartbeat

`POST /v1/devices/{device_id}/heartbeat` every 15 seconds with JSON health,
capabilities, frame counters, uptime, memory, and Wi-Fi RSSI. It uses the same
authorization and identity headers.

## Security boundary

- Device tokens must be device-scoped, revocable, stored hashed on the Pi,
  compared in constant time, and rate limited.
- The Pi must reject a token used with a different path/header device ID.
- Limit JPEG body size and JSON body size before reading request bodies.
- LAN HTTP is suitable only for a trusted isolated prototype network. Use
  HTTPS with a pinned CA/certificate before traffic crosses an untrusted
  network. The firmware deliberately does not disable TLS verification.
- Never provision Anthropic or ElevenLabs API keys on the ESP32. The Pi owns
  those integrations in later milestones.
- Camera capture is serialized with a FreeRTOS mutex so dashboard snapshots
  and Pi uploads cannot call the camera driver concurrently.

## M1 acceptance target

Four uniquely identified devices must remain online for 60 minutes, update a
2x2 dashboard at least once per second, reconnect after Pi/network restart,
and report last-seen time, RSSI, memory, frame sequence, and dropped frames.

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

For complete Pi provisioning, use:

```text
PROVISION_JSON:{"ssid":"...","password":"...","device_id":"...","alias":"sally-01","user_id":"...","device_token":"...","pi_base_url":"http://raspbx.local:8080"}
```

BLE provisioning is available only for ten minutes after boot on an
unprovisioned device. A successful Wi-Fi check stores the configuration,
permanently locks BLE reprovisioning, acknowledges `PROVISION_OK`, and restarts.
Reopening provisioning requires a deliberate physical Preferences erase/reset
procedure; no remote unlock command exists.

Command writes and status reads require an encrypted, bonded LE Secure
Connections link. The board has no trustworthy display for numeric comparison,
so this is still a proximity/bootstrap control rather than strong mutual
attestation; the limited window and permanent lock are essential safeguards.

The current iOS pipe-delimited command cannot safely represent an SSID or
password containing `|`, and it does not pass the `heartbeatUrl`, a Pi base
URL, or a Sally alias. The app should migrate to `PROVISION_JSON` and pass its
returned heartbeat URL or an independently configured Pi URL.
