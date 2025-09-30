#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>
#include <driver/i2s.h>  // Legacy driver - always available in Arduino
#include "config.h"

// Audio configuration
#define SAMPLE_RATE       16000
#define MIC_DATA_PIN      39
#define MIC_CLOCK_PIN     38
#define SPK_BCLK_PIN      45
#define SPK_LRC_PIN       46
#define SPK_DOUT_PIN      42

bool audioInitialized = false;

bool initAudio() {
    Serial.println("Initializing audio...");
    
    // Microphone config (PDM)
    i2s_config_t i2s_mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t mic_pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_CLOCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_DATA_PIN
    };
    
    if (i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL) != ESP_OK) {
        Serial.println("Failed to install I2S microphone driver");
        return false;
    }
    
    if (i2s_set_pin(I2S_NUM_0, &mic_pin_config) != ESP_OK) {
        Serial.println("Failed to set I2S microphone pins");
        return false;
    }
    Serial.println("Microphone OK");
    
    // Speaker config (Standard I2S)
    i2s_config_t i2s_spk_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t spk_pin_config = {
        .bck_io_num = SPK_BCLK_PIN,
        .ws_io_num = SPK_LRC_PIN,
        .data_out_num = SPK_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    if (i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL) != ESP_OK) {
        Serial.println("Failed to install I2S speaker driver");
        return false;
    }
    
    if (i2s_set_pin(I2S_NUM_1, &spk_pin_config) != ESP_OK) {
        Serial.println("Failed to set I2S speaker pins");
        return false;
    }
    Serial.println("Speaker OK");
    
    audioInitialized = true;
    return true;
}

void testAudioLoopback(int seconds = 5) {
    if (!audioInitialized) {
        Serial.println("Audio not initialized!");
        return;
    }
    
    Serial.println("\n=== Audio Loopback Test ===");
    Serial.printf("Recording %d seconds...\n", seconds);
    
    size_t buffer_size = SAMPLE_RATE * 2 * seconds;
    int16_t* buffer = (int16_t*)malloc(buffer_size);
    if (!buffer) {
        Serial.println("Failed to allocate buffer!");
        return;
    }
    
    // Record
    digitalWrite(LED_GPIO_NUM, HIGH);
    size_t bytes_read = 0;
    i2s_read(I2S_NUM_0, buffer, buffer_size, &bytes_read, portMAX_DELAY);
    digitalWrite(LED_GPIO_NUM, LOW);
    
    Serial.printf("Recorded %d bytes\n", bytes_read);
    
    // Playback
    if (bytes_read > 0) {
        Serial.println("Playing back...");
        delay(500);
        
        digitalWrite(LED_GPIO_NUM, HIGH);
        size_t bytes_written = 0;
        i2s_write(I2S_NUM_1, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        digitalWrite(LED_GPIO_NUM, LOW);
        
        Serial.println("Playback complete!");
    }
    
    free(buffer);
    Serial.println("Test complete!");
}

#endif