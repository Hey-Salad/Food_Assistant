# DFRobot DFR1154 Complete Pinout Reference

## 🎥 Camera Pins (OV3660)

### I2C Communication (SCCB)
| Function | GPIO | Description |
|----------|------|-------------|
| SIOD (SDA) | 8 | I2C Data for camera control |
| SIOC (SCL) | 9 | I2C Clock for camera control |

### Camera Control
| Function | GPIO | Description |
|----------|------|-------------|
| XCLK | 5 | Master clock (20MHz) |
| PCLK | 15 | Pixel clock |
| VSYNC | 1 | Vertical sync |
| HREF | 2 | Horizontal reference |
| PWDN | -1 | Power down (not used) |
| RESET | -1 | Reset (not used) |

### Data Lines (Y2-Y9)
| Function | GPIO | Description |
|----------|------|-------------|
| Y2 (D0) | 16 | Data bit 0 (LSB) |
| Y3 (D1) | 18 | Data bit 1 |
| Y4 (D2) | 21 | Data bit 2 |
| Y5 (D3) | 17 | Data bit 3 |
| Y6 (D4) | 14 | Data bit 4 |
| Y7 (D5) | 7 | Data bit 5 |
| Y8 (D6) | 6 | Data bit 6 |
| Y9 (D7) | 4 | Data bit 7 (MSB) |

## 💾 SD Card (SPI)

| Function | GPIO | Description |
|----------|------|-------------|
| CS | 10 | Chip Select |
| MOSI | 11 | Master Out Slave In |
| MISO | 13 | Master In Slave Out |
| SCK | 12 | Serial Clock |

## 🎤 Audio

### Microphone (I2S PDM)
| Function | GPIO | Description |
|----------|------|-------------|
| DATA | 39 | PDM data input |
| CLOCK | 38 | PDM clock |

### Speaker (I2S via MAX98357)
| Function | GPIO | Description |
|----------|------|-------------|
| BCLK | 45 | Bit clock |
| LRC | 46 | Left/Right clock |
| DOUT | 42 | Data output |

## 💡 LEDs & Sensors

| Function | GPIO | Description |
|----------|------|-------------|
| LED | 3 | Status LED (red) |
| IR LED | 47 | Infrared illumination |
| Light Sensor | I2C | LTR-308 (shares I2C with camera) |

## 🔌 External Connectors

### Gravity Connector (UART)
| Function | GPIO | Description |
|----------|------|-------------|
| TX | 43 | UART transmit |
| RX | 44 | UART receive |
| + | - | 3.3-5V power |
| - | - | GND |

### Buttons
| Function | GPIO | Description |
|----------|------|-------------|
| BOOT | 0 | Boot mode button |
| RST | EN | Reset button |

## ⚠️ Reserved Pins (DO NOT USE)

### PSRAM Pins (Octal Mode)
The ESP32-S3R8 uses **8MB PSRAM in Octal SPI mode**, which occupies:
- **GPIO 35** - PSRAM D4
- **GPIO 36** - PSRAM D5  
- **GPIO 37** - PSRAM D6

**These pins are NOT available for external use!**

### Other Reserved
- **GPIO 19, 20** - USB (if using USB CDC)
- **GPIO 26-32** - Internal PSRAM/Flash

## 🔍 Pin Conflict Detection

### Safe to Use
These GPIOs are available for your projects:
- None directly exposed - all used by onboard peripherals

### Accessible via Gravity
- GPIO 43 (TX)
- GPIO 44 (RX)

### Use with Caution
- GPIO 0 (BOOT) - Can be used but affects boot mode
- GPIO 3 (LED) - Can repurpose but you'll lose LED indicator

## 📊 Pin Priority for Debugging

If camera init fails, verify these in order:

1. **I2C Communication (Critical)**
   - SIOD: GPIO 8
   - SIOC: GPIO 9
   - These MUST be correct or camera won't be detected

2. **Clock Signals**
   - XCLK: GPIO 5 (must output 20MHz)
   - PCLK: GPIO 15
   - VSYNC: GPIO 1
   - HREF: GPIO 2

3. **Data Lines** (all 8 required)
   - Y2-Y9: GPIO 16, 18, 21, 17, 14, 7, 6, 4

## 🧪 Testing Individual Components

### Test Camera I2C
```cpp
Wire.begin(8, 9);  // SDA, SCL
Wire.beginTransmission(0x3C);  // OV3660 address
byte error = Wire.endTransmission();
if (error == 0) {
    Serial.println("Camera detected on I2C!");
}
```

### Test SD Card
```cpp
SPI.begin(12, 13, 11, 10);  // SCK, MISO, MOSI, CS
if (SD.begin(10)) {
    Serial.println("SD card detected!");
}
```

### Test LED
```cpp
pinMode(3, OUTPUT);
digitalWrite(3, HIGH);  // Should light up
delay(1000);
digitalWrite(3, LOW);
```

## 🎯 Common Mistakes

### ❌ Wrong Pin Numbers
```cpp
// WRONG - AI Thinker ESP32-CAM pins
#define SIOD_GPIO_NUM 26  // Don't use this!
#define SIOC_GPIO_NUM 27  // Don't use this!

// CORRECT - DFR1154 pins
#define SIOD_GPIO_NUM 8
#define SIOC_GPIO_NUM 9
```

### ❌ Wrong PSRAM Mode
```ini
# WRONG
board_build.psram_type = qio

# CORRECT - DFR1154 uses Octal PSRAM
board_build.psram_type = opi
```

### ❌ Wrong XCLK Frequency
```cpp
// WRONG - Too fast for OV3660
config.xclk_freq_hz = 24000000;

// CORRECT - OV3660 optimal frequency
config.xclk_freq_hz = 20000000;
```

## 📐 Visual Pin Mapping

```
DFRobot ESP32-S3 AI CAM (DFR1154)
┌─────────────────────────────────┐
│  [OV3660 Camera]                │
│                                 │
│  IR LED (47)    LED (3)         │
│                                 │
│  [Microphone]   [Speaker Port]  │
│   DATA: 39                      │
│   CLK:  38      45,46,42        │
│                                 │
│  [SD Card Slot]                 │
│   CS:   10                      │
│   MOSI: 11                      │
│   MISO: 13                      │
│   SCK:  12                      │
│                                 │
│  [Gravity Connector]            │
│   +  -  44  43                  │
│                                 │
│  [BOOT]  [RST]  [USB-C]         │
└─────────────────────────────────┘
```

## 🔧 Verification Checklist

Before uploading code, verify:

- [ ] Camera I2C pins: GPIO 8 (SDA), GPIO 9 (SCL)
- [ ] Camera XCLK: GPIO 5
- [ ] All 8 data lines configured correctly
- [ ] PSRAM mode set to "opi" (Octal)
- [ ] SD Card SPI pins: 10, 11, 12, 13
- [ ] Not using GPIO 35, 36, 37 (PSRAM reserved)
- [ ] XCLK frequency: 20MHz
- [ ] Frame buffer location: PSRAM
- [ ] Board type: esp32-s3-devkitc-1

---

**This pinout is specific to DFRobot DFR1154. Do not use pinouts from other ESP32-CAM boards!**