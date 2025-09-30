/*
 * HeySalad Hardware Test Script
 * 
 * Upload this to test all hardware components individually
 * before using the full application.
 * 
 * To use: Replace main.cpp with this file temporarily,
 * upload, and check Serial Monitor at 115200 baud
 */

#include <Arduino.h>
#include <esp_camera.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

// DFRobot DFR1154 Pin Definitions
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      5
#define SIOD_GPIO_NUM      8
#define SIOC_GPIO_NUM      9
#define Y9_GPIO_NUM        4
#define Y8_GPIO_NUM        6
#define Y7_GPIO_NUM        7
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       17
#define Y4_GPIO_NUM       21
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       16
#define VSYNC_GPIO_NUM     1
#define HREF_GPIO_NUM      2
#define PCLK_GPIO_NUM     15

#define LED_GPIO_NUM       3
#define IR_LED_GPIO_NUM   47
#define SD_CARD_CS        10

bool testsPassed = 0;
bool testsFailed = 0;

void printHeader(const char* test) {
    Serial.println();
    Serial.println("========================================");
    Serial.print("TEST: ");
    Serial.println(test);
    Serial.println("========================================");
}

void printResult(bool passed) {
    if (passed) {
        Serial.println("✓ PASSED");
        testsPassed++;
    } else {
        Serial.println("✗ FAILED");
        testsFailed++;
    }
}

// Test 1: LED Indicators
bool testLEDs() {
    printHeader("LED Indicators");
    
    pinMode(LED_GPIO_NUM, OUTPUT);
    pinMode(IR_LED_GPIO_NUM, OUTPUT);
    
    Serial.println("Testing Status LED (GPIO 3)...");
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_GPIO_NUM, HIGH);
        Serial.print("  LED ON... ");
        delay(300);
        digitalWrite(LED_GPIO_NUM, LOW);
        Serial.println("LED OFF");
        delay(300);
    }
    
    Serial.println("Testing IR LED (GPIO 47)...");
    digitalWrite(IR_LED_GPIO_NUM, HIGH);
    Serial.println("  IR LED ON (not visible to eye)");
    delay(1000);
    digitalWrite(IR_LED_GPIO_NUM, LOW);
    Serial.println("  IR LED OFF");
    
    Serial.println("Did you see the red LED blink 3 times? (Can't see IR)");
    return true;  // Manual verification
}

// Test 2: I2C Bus (Camera Communication)
bool testI2C() {
    printHeader("I2C Bus (Camera Communication)");
    
    Wire.begin(SIOD_GPIO_NUM, SIOC_GPIO_NUM);
    Serial.println("Scanning I2C bus...");
    
    bool deviceFound = false;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("  Device found at 0x%02X\n", address);
            if (address == 0x3C) {
                Serial.println("  ✓ OV3660 camera detected!");
                deviceFound = true;
            }
        }
    }
    
    if (!deviceFound) {
        Serial.println("  ✗ No camera found on I2C bus");
        Serial.println("  Check SIOD (GPIO 8) and SIOC (GPIO 9) connections");
    }
    
    return deviceFound;
}

// Test 3: Camera Initialization
bool testCamera() {
    printHeader("Camera Initialization");
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;  // Smaller for testing
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;
    
    Serial.println("Initializing camera...");
    esp_err_t err = esp_camera_init(&config);
    
    if (err != ESP_OK) {
        Serial.printf("  ✗ Camera init failed: 0x%x\n", err);
        Serial.println("\n  Error codes:");
        Serial.println("  0x105 = Camera not found (I2C issue)");
        Serial.println("  0x20011 = PSRAM issue");
        Serial.println("  Check pin configuration!");
        return false;
    }
    
    Serial.println("  ✓ Camera initialized!");
    
    // Get sensor info
    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        Serial.println("  ✓ OV3660 sensor confirmed");
        Serial.printf("  Sensor ID: 0x%04X\n", s->id.PID);
        
        // Apply optimal settings
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
        Serial.println("  ✓ OV3660 settings applied");
    }
    
    return true;
}

// Test 4: Camera Capture
bool testCapture() {
    printHeader("Camera Photo Capture");
    
    Serial.println("Capturing test photo...");
    digitalWrite(LED_GPIO_NUM, HIGH);
    delay(100);
    
    camera_fb_t *fb = esp_camera_fb_get();
    digitalWrite(LED_GPIO_NUM, LOW);
    
    if (!fb) {
        Serial.println("  ✗ Failed to capture photo");
        return false;
    }
    
    Serial.printf("  ✓ Photo captured successfully!\n");
    Serial.printf("  Size: %d bytes\n", fb->len);
    Serial.printf("  Width: %d px\n", fb->width);
    Serial.printf("  Height: %d px\n", fb->height);
    Serial.printf("  Format: %s\n", fb->format == PIXFORMAT_JPEG ? "JPEG" : "Other");
    
    esp_camera_fb_return(fb);
    return true;
}

// Test 5: SD Card
bool testSDCard() {
    printHeader("SD Card");
    
    Serial.println("Initializing SD card...");
    SPI.begin(12, 13, 11, SD_CARD_CS);
    
    if (!SD.begin(SD_CARD_CS)) {
        Serial.println("  ✗ SD card mount failed");
        Serial.println("  This is OK if no SD card inserted");
        Serial.println("  Device will work without SD card");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("  ✗ No SD card detected");
        return false;
    }
    
    Serial.print("  ✓ SD Card Type: ");
    switch (cardType) {
        case CARD_MMC: Serial.println("MMC"); break;
        case CARD_SD: Serial.println("SDSC"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default: Serial.println("UNKNOWN"); break;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("  Card Size: %llu MB\n", cardSize);
    
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
    Serial.printf("  Used: %llu MB\n", usedBytes);
    
    // Test write
    Serial.println("\n  Testing write capability...");
    File testFile = SD.open("/test.txt", FILE_WRITE);
    if (testFile) {
        testFile.println("HeySalad Hardware Test");
        testFile.close();
        Serial.println("  ✓ Write test successful");
        SD.remove("/test.txt");
    } else {
        Serial.println("  ✗ Write test failed");
        return false;
    }
    
    return true;
}

// Test 6: PSRAM
bool testPSRAM() {
    printHeader("PSRAM (8MB Octal)");
    
    Serial.printf("  Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("  Free PSRAM: %d bytes\n", ESP.getFreePsram());
    
    if (ESP.getPsramSize() == 0) {
        Serial.println("  ✗ PSRAM not detected!");
        Serial.println("  Check platformio.ini has:");
        Serial.println("    board_build.psram_type = opi");
        return false;
    }
    
    if (ESP.getPsramSize() < 8000000) {
        Serial.println("  ⚠ PSRAM size less than 8MB");
        Serial.println("  Expected 8MB for DFR1154");
    } else {
        Serial.println("  ✓ 8MB PSRAM detected correctly");
    }
    
    return true;
}

// Test 7: Memory Test
bool testMemory() {
    printHeader("Memory Allocation Test");
    
    Serial.println("Testing large memory allocation (for camera buffer)...");
    
    size_t testSize = 300000;  // ~300KB (typical UXGA JPEG)
    uint8_t* testBuffer = (uint8_t*)ps_malloc(testSize);
    
    if (!testBuffer) {
        Serial.println("  ✗ Failed to allocate 300KB from PSRAM");
        Serial.println("  Camera may fail at high resolutions");
        return false;
    }
    
    Serial.printf("  ✓ Successfully allocated %d bytes\n", testSize);
    free(testBuffer);
    
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║  HeySalad Hardware Test Suite         ║");
    Serial.println("║  DFRobot ESP32-S3 AI Camera (DFR1154) ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    
    // Run all tests
    printResult(testLEDs());
    delay(500);
    
    printResult(testPSRAM());
    delay(500);
    
    printResult(testMemory());
    delay(500);
    
    printResult(testI2C());
    delay(500);
    
    printResult(testCamera());
    delay(500);
    
    printResult(testCapture());
    delay(500);
    
    printResult(testSDCard());
    
    // Summary
    Serial.println();
    Serial.println("========================================");
    Serial.println("           TEST SUMMARY");
    Serial.println("========================================");
    Serial.printf("Tests Passed: %d\n", testsPassed);
    Serial.printf("Tests Failed: %d\n", testsFailed);
    Serial.println("========================================");
    
    if (testsFailed == 0) {
        Serial.println();
        Serial.println("🎉 ALL TESTS PASSED! 🎉");
        Serial.println("Your hardware is working perfectly!");
        Serial.println("Ready to upload the full HeySalad app!");
        
        // Celebrate with LED
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_GPIO_NUM, HIGH);
            delay(100);
            digitalWrite(LED_GPIO_NUM, LOW);
            delay(100);
        }
    } else {
        Serial.println();
        Serial.println("⚠️ SOME TESTS FAILED ⚠️");
        Serial.println("Check the errors above and:");
        Serial.println("1. Verify pin configuration");
        Serial.println("2. Check platformio.ini settings");
        Serial.println("3. Ensure proper power supply");
        Serial.println("4. Try re-uploading the code");
        
        // Error indication
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_GPIO_NUM, HIGH);
            delay(500);
            digitalWrite(LED_GPIO_NUM, LOW);
            delay(500);
        }
    }
    
    Serial.println("\nTest complete. Reset board to run again.");
}

void loop() {
    // Nothing to do
    delay(1000);
}