# 🍅 HeySalad - Food Assistant

**An ESP32-S3 powered voice-activated food assistant with camera, audio, and AI integration.**

![HeySalad Logo](data/assets/logo.png)

## 🌟 Features

- 🎤 **Voice Recording** - Push-to-talk interface
- 📞 **AI Voice Calls** - OpenAI Realtime API & ElevenLabs Conversational AI
- 📸 **Camera Integration** - OV3660 camera for food recognition
- 🔊 **Audio I/O** - PDM microphone + I2S speaker
- 📱 **Web Interface** - Modern, responsive UI
- ⚙️ **Easy Setup** - WiFi configuration portal

## Sally Camera provisioning

This branch (`sally-spatial-m1.2.0`) is the DFR1154 fridge camera. Model API keys are not stored on the device. OpenAI, ElevenLabs, Gemini, and DeepSeek inference runs server-side through sally-api. `GET /get_keys` and `POST /save_keys` are not registered (they 404). Any of those keys left in NVS from older firmware is erased on boot.

The server base is `https://<sally-api>/device`. The dev base is `https://sally-api-dev.heysalad-o.workers.dev/device`. Store that string as `pi_base_url` (no path past `/device`). The firmware appends `/v1/devices/{hwId}/heartbeat` and `/v1/devices/{hwId}/frames`.

Both requests send `Authorization: Bearer <device_token>` and `X-Device-ID` set to the same hwId used in the path. The token is 32 bytes, encoded as 64 hexadecimal characters. Camera 1's hwId is `sally-a8c12904a7ac`.

`POST /device/v1/devices/{hwId}/heartbeat` is JSON every **15 seconds**. `postHeartbeat()` reads the response body. `{"captureNow": true}` takes and uploads one JPEG immediately. A command named `"capture"` does the same.

`POST /device/v1/devices/{hwId}/frames` is `image/jpeg`, at most **2 MiB**. A larger capture is dropped on the camera and is not posted. Scheduled snapshots are every **5 minutes** by default (`snapshot_interval_sec`, 30–86400); the first goes out when the uplink comes up. A second frame within **30 seconds** is HTTP 429, so a frame 429 waits at least 30 seconds (longer when `Retry-After` says so) and a successful upload also waits 30 seconds before another frame, including `captureNow`. That backoff does not slow the heartbeat. HTTPS uploads verify the server certificate with the ESP32 x509 bundle when that symbol is linked, otherwise with ISRG Root X1, ISRG Root X2, and Google Trust Services roots R1–R4.

The hardware id is whatever `DEVICE_INFO` returns (`sally-` plus the eFuse MAC, byte order unchanged). Register that string. Do not recompute it.

BLE provisioning locks once any Wi-Fi configuration exists, including the `include/secrets.h` fallback. It does not open again on reboot. After the firmware is running, hold **BOOT** (GPIO0, the button beside RST) for 5 seconds to open a 10-minute window, or 12 seconds to factory-reset Wi-Fi and cloud settings. Holding BOOT while pressing RST enters the ROM bootloader; that is the flash gesture, not the unlock gesture.

### `PROVISION_JSON` schema

Write this to the command characteristic (`beb5483e-36e1-4688-b7f5-ea07361b26a8`) as UTF-8. The prefix is exactly `PROVISION_JSON:` with no space, then one JSON object. Status notifications come back on `beb5483e-36e1-4688-b7f5-ea07361b26a9`. Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`.

```json
{
  "ssid": "required string, 1-32 characters after trimming",
  "password": "required string, empty allowed for an open network",
  "device_token": "required, exactly 64 hexadecimal characters (32 bytes). Example: 64 chars of 0-9a-f",
  "pi_base_url": "absolute http:// or https:// server base, no trailing slash needed. Production: https://<sally-api>/device. Dev: https://sally-api-dev.heysalad-o.workers.dev/device",
  "device_id": "optional cloud id, stored separately from the hardware id",
  "alias": "optional, lowercase letters, digits, and hyphen, 1-32 characters",
  "user_id": "optional string",
  "admin_token": "optional, at least 32 characters. Gates LAN /capture and /ir only when the camera is not provisioned to sally-api. Omit it to keep those routes closed.",
  "snapshot_interval_sec": "optional integer, 30 through 86400, default 300"
}
```

`pi_base_url` may be omitted only when the camera should not upload. For sally-mobile it is required and is the sally-api device base (`https://<sally-api>/device`, or `https://sally-api-dev.heysalad-o.workers.dev/device` on dev). The firmware appends `/v1/devices/{hwId}/heartbeat` and `/v1/devices/{hwId}/frames`. Accepted aliases, used only when the canonical field is empty: `server_base_url` and `ingest_base_url` for `pi_base_url`, and `heartbeat_token` for `device_token`. `device_token` and `heartbeat_token` must be 64 hexadecimal characters.

Status text: `PROVISION_OK`, `PROVISION_BAD_FORMAT`, `PROVISION_WIFI_FAILED`, `PROVISION_LOCKED`, `PROVISION_WINDOW_CLOSED`. `DEVICE_INFO` and `STATUS` return JSON including `device_id`, `firmware`, `provisioned`, `cloud`, `rssi`, `ble_locked`, and `snapshot_interval_sec`. They never return the Wi-Fi password, device token, or admin token.

Build and flash (do not flash from CI):

```bash
pio run -e heysalad-dfr1154
pio run -e heysalad-dfr1154 -t upload --upload-port /dev/cu.usbmodem101
```

`include/secrets.h` is gitignored. Copy `include/secrets.example.h` locally. Do not commit it.

## 🛠️ Hardware

**Board:** DFRobot ESP32-S3 AI Camera Module (DFR1154)

**Features:**
- ESP32-S3 with 8MB PSRAM (Octal)
- OV3660 Camera
- PDM Microphone
- I2S Speaker Output
- SD Card Slot
- USB-C

## 🚀 Quick Start

### Prerequisites

- PlatformIO IDE (VS Code extension)
- USB-C cable
- WiFi network (2.4GHz)
- Model API keys are not put on the camera. See Sally Camera provisioning.

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/heysalad-food-assistant.git
   cd heysalad-food-assistant
   ```

2. **Open in VS Code**
   - Open folder in VS Code with PlatformIO extension

3. **Build & Upload**
   - Click Upload button in PlatformIO
   - If upload fails: Hold BOOT, press RST, release BOOT, try again

4. **Configure WiFi**
   - Connect to `HeySalad-Setup` WiFi network
   - Open browser to `http://192.168.4.1`
   - Enter your WiFi credentials
   - Device will restart and connect

5. **Access the Interface**
   - Check Serial Monitor (115200 baud) for IP address
   - Open browser to `http://[DEVICE_IP]`

## 📖 Documentation

- **[Quick Start Guide](docs/QUICKSTART.md)** - Get running in 5 minutes
- **[Pin Reference](docs/PINOUT.md)** - Complete hardware pinout
- **[Project Summary](docs/PROJECT_SUMMARY.md)** - Technical overview

## 🎯 Usage

### Voice Mode
- Press and hold the microphone button
- Speak your question or command
- Release to process

### Call Mode
1. Navigate to **Call** tab
2. Select provider (OpenAI or ElevenLabs)
3. Tap **Start Call**
4. Have a conversation with AI
5. Tap **End Call** when done

### Camera
- View live camera feed
- Capture photos
- Photos saved to SD card (if inserted)

## 🔧 Configuration

Edit `include/config.h` for hardware settings:
- Camera pins
- LED configuration
- Audio settings

## 🧪 Hardware Testing

Run the hardware test suite:

```bash
# Temporarily rename main.cpp
mv src/main.cpp src/main.cpp.bak

# Copy test file
cp test/test_hardware.cpp src/main.cpp

# Upload and check Serial Monitor
pio run -t upload && pio device monitor

# Restore main.cpp
mv src/main.cpp.bak src/main.cpp
```

## 📡 API Integration

### OpenAI Realtime API
Configure in your application for voice calls with GPT-4 audio.

### ElevenLabs Conversational AI
Use your agent ID for natural voice conversations.

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## 📝 License

MIT License - see [LICENSE](LICENSE) file

## 🙏 Acknowledgments

- DFRobot for the ESP32-S3 AI Camera module
- Espressif for ESP32-S3 platform
- OpenAI for Realtime API
- ElevenLabs for Conversational AI

## 📧 Support

- 📖 Check [docs/](docs/) folder
- 🐛 [Open an issue](https://github.com/yourusername/heysalad-food-assistant/issues)
- 💬 [Discussions](https://github.com/yourusername/heysalad-food-assistant/discussions)

## 🔗 Links

- [DFRobot DFR1154 Wiki](https://wiki.dfrobot.com/SKU_DFR1154)
- [ESP32-S3 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [PlatformIO](https://platformio.org/)

---

**Made with ❤️ for food lovers everywhere**

*HeySalad ® Food Assistant*

## 🏢 Company Information

**HeySalad OÜ**  
Reg: 17327633  
Pärnu mnt 139b  
11317 Tallinn  
Estonia