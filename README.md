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
- OpenAI API key (optional for calls)
- ElevenLabs API key (optional for calls)

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

Copyright (C) 2025-2026 HeySalad Inc.

Licensed under the GNU Affero General Public License v3.0 only (AGPL-3.0-only). See [LICENSE](LICENSE) and [NOTICE](NOTICE). If you run a modified version of this software to provide a network service, the AGPL requires you to offer its users the corresponding source code. Versions published before this change remain under their previous licence.

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