# HeySalad Quick Start Guide
## Get your smart kitchen assistant running in 5 minutes! 🚀

---

## 📦 What You Need

✅ DFRobot ESP32-S3 AI Camera Module (DFR1154)  
✅ MicroSD card (optional, but recommended)  
✅ USB-C cable  
✅ Computer with VS Code + PlatformIO  
✅ OpenAI API key ([Get one here](https://platform.openai.com/api-keys))

---

## 🏗️ Step 1: Create Project (1 min)

### In VS Code:
1. Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac)
2. Type: `PlatformIO: New Project`
3. Name: `heysalad-kitchen-assistant`
4. Board: `Espressif ESP32-S3-DevKitC-1`
5. Framework: `Arduino`
6. Click **Finish**

---

## 📁 Step 2: Add Files (1 min)

### Copy these files into your project:

```
heysalad-kitchen-assistant/
├── platformio.ini          ← Replace with provided file
├── huge_app.csv            ← Add to root
├── src/
│   └── main.cpp            ← Replace with provided file
└── include/
    └── base64.h            ← Add this file
```

### File Locations:
- `platformio.ini` → Root folder (replace existing)
- `huge_app.csv` → Root folder (new file)
- `main.cpp` → `src/` folder (replace existing)
- `base64.h` → `include/` folder (new file)

---

## 🔌 Step 3: Connect Hardware (30 sec)

1. **Insert MicroSD card** into the DFR1154 slot (optional)
2. **Connect USB-C cable** from computer to DFR1154
3. Board should show up as a USB device

---

## ⬆️ Step 4: First Upload (2 min)

### Upload Procedure:

1. Click **Upload** button in PlatformIO (→ icon at bottom)
2. **If upload fails:**
   - Hold **BOOT button** on board
   - Press **RST button** briefly  
   - Release **BOOT button**
   - Click **Upload** again

3. Wait for "SUCCESS" message

---

## 📱 Step 5: Setup WiFi (1 min)

### After upload completes:

1. **On your phone/computer:**
   - Look for WiFi network: `HeySalad-Setup`
   - Connect to it (no password needed)

2. **Open browser:**
   - Go to: `http://192.168.4.1`

3. **Fill in the form:**
   ```
   WiFi Name: YourHomeWiFi
   WiFi Password: YourPassword
   OpenAI API Key: sk-proj-xxxx...
   ElevenLabs: (leave blank for now)
   ```

4. Click **Save & Connect**

5. Device will restart (takes ~10 seconds)

---

## 🎉 Step 6: Use HeySalad!

### Find Your Device:

**Option A - Check Serial Monitor:**
1. In VS Code, click **Serial Monitor** icon
2. Set to **115200 baud**
3. Press RST button on board
4. Look for: `Connected! IP: 192.168.x.x`

**Option B - Check Router:**
1. Log into your WiFi router
2. Look for device named "ESP32-S3" or similar
3. Note the IP address

### Access the App:

1. Open browser to: `http://[DEVICE_IP]`
2. You'll see the HeySalad interface!

### Try It Out:

1. Point camera at some food/ingredients
2. Click **📸 Capture Photo**
3. Click **🤖 Get Recipe Ideas**
4. Wait ~5-10 seconds
5. Get AI-powered recipe suggestions! 🍅

---

## 🔧 Troubleshooting

### ❌ Camera Init Failed

**LED blinking rapidly on startup?**

✅ **Solution:** Code is already configured correctly for DFR1154!  
If still failing:
- Try different USB cable
- Check USB port has enough power
- Press RST button to restart

---

### ❌ Can't Find HeySalad-Setup WiFi

✅ **Solutions:**
- Wait 20-30 seconds after upload
- Press RST button on board
- Re-upload the code
- Check Serial Monitor for errors

---

### ❌ OpenAI API Error

✅ **Check:**
- API key is correct (starts with `sk-proj-`)
- You have credits in OpenAI account
- Device has internet connection

---

### ❌ SD Card Not Working

✅ **Try:**
- Format as FAT32
- Use smaller card (32GB max)
- Make sure it's fully inserted
- Device works without SD card!

---

## 💡 Pro Tips

### Better Photos
- Use good lighting
- Get close to ingredients
- Keep camera steady
- Clean the lens if blurry

### Faster Analysis
- Start with smaller images:
  ```cpp
  config.frame_size = FRAMESIZE_SVGA;  // In main.cpp
  ```

### Save API Costs
- Test with local ingredients first
- One analysis ≈ $0.01-0.03
- Photos saved to SD for review

### Night Vision
- Enable IR LED for dark environments
- See PINOUT.md for instructions

---

## 🎯 What's Next?

✨ **Working perfectly?** Try these enhancements:

1. **Add Custom Prompts**
   - Edit the OpenAI prompt in `main.cpp`
   - Ask for specific cuisines
   - Request dietary preferences

2. **Voice Output**
   - Add ElevenLabs API key
   - Get spoken recipe instructions

3. **Multiple Photos**
   - Browse SD card for past photos
   - Build recipe history

---

## 📚 Full Documentation

- **README.md** - Complete guide
- **PINOUT.md** - Hardware reference  
- **DFRobot Wiki** - Official docs

---

## 🆘 Still Stuck?

### Check Serial Monitor
```
115200 baud
Shows all debug info
```

### Verify Pin Configuration
```cpp
Camera pins in main.cpp match DFR1154:
SIOD: 8, SIOC: 9, XCLK: 5
```

### Common Upload Issues
1. **"Failed to connect"**
   - Hold BOOT, press RST, release BOOT
   - Try again

2. **"Port not found"**
   - Check USB cable (must be data cable)
   - Try different USB port

3. **"Compilation error"**
   - Make sure all 5 files are copied
   - Check base64.h is in include/ folder

---

## ✅ Success Checklist

Before asking for help, verify:

- [ ] All 5 files copied to correct locations
- [ ] platformio.ini has `psram_type = opi`
- [ ] USB cable supports data (not just power)
- [ ] Held BOOT during first upload
- [ ] WiFi is 2.4GHz (not 5GHz)
- [ ] OpenAI API key is valid
- [ ] Checked Serial Monitor at 115200 baud
- [ ] Device shows IP address after connecting
- [ ] Camera LED flashes when capturing

---

## 🎊 Success Looks Like This:

### Serial Monitor Output:
```
=================================
HeySalad Smart Kitchen Assistant
=================================

Initializing SD card...
SD Card Type: SDHC
SD Card Size: 16384MB
SD card ready

Initializing camera...
OV3660 detected, applying settings...
Camera initialized successfully!

Connecting to WiFi: YourNetwork
..........
Connected! IP: 192.168.1.100

System ready!

=================================
Access HeySalad at: http://192.168.1.100
=================================
```

### When Working:
- ✅ LED flashes during photo capture
- ✅ Photos display in browser
- ✅ Recipe suggestions appear after ~5-10 sec
- ✅ Photos saved to SD card
- ✅ No errors in Serial Monitor

---

## 🚀 You're All Set!

Your HeySalad Smart Kitchen Assistant is ready to help you love your food! 🍅

**Next Steps:**
- Try different ingredients
- Experiment with recipe suggestions
- Share your creations
- Customize the AI prompts

---

**Need help?** Check the full README.md for advanced troubleshooting!