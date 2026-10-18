#ifndef OPENAI_REALTIME_H
#define OPENAI_REALTIME_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <driver/i2s.h>
#include <esp_camera.h>
#include "config.h"
#include "tls_roots.h"

// OpenAI Realtime API Configuration
#define OPENAI_WS_HOST "api.openai.com"
#define OPENAI_WS_PORT 443
#define OPENAI_WS_PATH "/v1/realtime?model=gpt-4o-realtime-preview-2024-12-17"
#define SAMPLE_RATE_24K 24000
#define BUFFER_SIZE 4800  // 200ms at 24kHz

class OpenAIRealtime {
private:
    WebSocketsClient wsClient;
    String apiKey;
    String sessionId;
    String conversationMode;  // "chef", "service", "general"
    bool isConnected = false;
    bool isRecording = false;
    int16_t audioBuffer[BUFFER_SIZE];
    
    // System prompts for different modes
    const char* getSystemPrompt() {
        if (conversationMode == "chef") {
            return "You are a professional chef assistant. Help users with cooking, recipes, "
                   "ingredient substitutions, and culinary techniques. Be friendly, encouraging, "
                   "and provide clear step-by-step guidance. When needed, you can request to see "
                   "ingredients through the camera by calling the 'capture_image' function.";
        } else if (conversationMode == "service") {
            return "You are a helpful restaurant service assistant. Assist with food recommendations, "
                   "dietary preferences, wine pairings, and dining etiquette. Be professional yet warm. "
                   "You can request to see dishes or menus through the camera when helpful.";
        } else {
            return "You are a knowledgeable food assistant. Help with anything food-related: cooking, "
                   "nutrition, meal planning, food science, and more. Be conversational and helpful. "
                   "You can request camera access to see ingredients or dishes when relevant.";
        }
    }
    
    // WebSocket event handler
    void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
        switch(type) {
            case WStype_DISCONNECTED:
                Serial.println("[OpenAI] Disconnected");
                isConnected = false;
                break;
                
            case WStype_CONNECTED:
                Serial.println("[OpenAI] Connected!");
                isConnected = true;
                sendSessionConfig();
                break;
                
            case WStype_TEXT:
                handleServerMessage((char*)payload);
                break;
                
            case WStype_BIN:
                // Audio response from OpenAI
                handleAudioResponse(payload, length);
                break;
                
            case WStype_ERROR:
                Serial.printf("[OpenAI] Error: %s\n", payload);
                break;
                
            case WStype_PING:
                Serial.println("[OpenAI] Ping");
                break;
                
            case WStype_PONG:
                Serial.println("[OpenAI] Pong");
                break;
        }
    }
    
    // Send session configuration
    void sendSessionConfig() {
        JsonDocument doc;
        doc["type"] = "session.update";
        
        JsonObject session = doc["session"].to<JsonObject>();
        session["modalities"][0] = "text";
        session["modalities"][1] = "audio";
        session["voice"] = "alloy";  // Options: alloy, echo, shimmer
        session["instructions"] = getSystemPrompt();
        session["input_audio_format"] = "pcm16";
        session["output_audio_format"] = "pcm16";
        session["input_audio_transcription"]["model"] = "whisper-1";
        
        // Add function for camera capture (energy-saving feature!)
        JsonObject tool = session["tools"][0].to<JsonObject>();
        tool["type"] = "function";
        tool["name"] = "capture_image";
        tool["description"] = "Capture an image from the device camera to see ingredients, dishes, or food items. "
                              "Use this sparingly to save energy - only when visual information would significantly help the conversation.";
        JsonObject params = tool["parameters"].to<JsonObject>();
        params["type"] = "object";
        params["properties"]["reason"]["type"] = "string";
        params["properties"]["reason"]["description"] = "Brief reason why you need to see an image";
        
        String output;
        serializeJson(doc, output);
        wsClient.sendTXT(output);
        
        Serial.println("[OpenAI] Session configured");
        Serial.printf("Mode: %s\n", conversationMode.c_str());
    }
    
    // Handle messages from OpenAI
    void handleServerMessage(const char* payload) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.printf("[OpenAI] JSON parse error: %s\n", error.c_str());
            return;
        }
        
        String eventType = doc["type"].as<String>();
        Serial.printf("[OpenAI] Event: %s\n", eventType.c_str());
        
        if (eventType == "session.created") {
            sessionId = doc["session"]["id"].as<String>();
            Serial.printf("Session ID: %s\n", sessionId.c_str());
            
        } else if (eventType == "conversation.item.created") {
            // New conversation item
            String role = doc["item"]["role"].as<String>();
            Serial.printf("New item from: %s\n", role.c_str());
            
        } else if (eventType == "response.audio.delta") {
            // Audio chunk - already handled in handleAudioResponse
            
        } else if (eventType == "response.function_call_arguments.done") {
            // Function call - camera capture requested!
            String functionName = doc["name"].as<String>();
            if (functionName == "capture_image") {
                String reason = doc["arguments"]["reason"].as<String>();
                Serial.printf("[OpenAI] Camera requested: %s\n", reason.c_str());
                sendCameraImage(reason);
            }
            
        } else if (eventType == "error") {
            String errorMsg = doc["error"]["message"].as<String>();
            Serial.printf("[OpenAI] Error: %s\n", errorMsg.c_str());
        }
    }
    
    // Handle audio response from OpenAI
    void handleAudioResponse(uint8_t* data, size_t length) {
        // Write directly to speaker I2S
        size_t bytes_written;
        i2s_write(I2S_NUM_1, data, length, &bytes_written, portMAX_DELAY);
    }
    
    // Send camera image to OpenAI
    void sendCameraImage(String reason) {
        digitalWrite(LED_GPIO_NUM, HIGH);
        
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[OpenAI] Camera capture failed");
            digitalWrite(LED_GPIO_NUM, LOW);
            return;
        }
        
        // Base64 encode the image
        String base64Image = base64Encode(fb->buf, fb->len);
        esp_camera_fb_return(fb);
        
        digitalWrite(LED_GPIO_NUM, LOW);
        
        // Send to OpenAI
        JsonDocument doc;
        doc["type"] = "conversation.item.create";
        
        JsonObject item = doc["item"].to<JsonObject>();
        item["type"] = "message";
        item["role"] = "user";
        
        JsonObject content = item["content"][0].to<JsonObject>();
        content["type"] = "input_image";
        content["image"] = "data:image/jpeg;base64," + base64Image;
        
        String output;
        serializeJson(doc, output);
        wsClient.sendTXT(output);
        
        Serial.printf("[OpenAI] Image sent (%.1fKB) - Reason: %s\n", 
                      fb->len / 1024.0, reason.c_str());
        Serial.println("♻️ Energy-conscious capture!");
    }
    
    // Base64 encoding helper
    String base64Encode(const uint8_t* data, size_t length) {
        const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        String encoded;
        encoded.reserve(((length + 2) / 3) * 4);
        
        for (size_t i = 0; i < length; i += 3) {
            uint32_t octet_a = i < length ? data[i] : 0;
            uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;
            uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;
            uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
            
            encoded += base64_chars[(triple >> 18) & 0x3F];
            encoded += base64_chars[(triple >> 12) & 0x3F];
            encoded += base64_chars[(triple >> 6) & 0x3F];
            encoded += base64_chars[triple & 0x3F];
        }
        
        size_t padding = (3 - (length % 3)) % 3;
        for (size_t i = 0; i < padding; i++) {
            encoded[encoded.length() - 1 - i] = '=';
        }
        
        return encoded;
    }

public:
    OpenAIRealtime() {
        wsClient.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
            this->onWebSocketEvent(type, payload, length);
        });
    }
    
    bool connect(String key, String mode) {
        if (key.isEmpty()) {
            Serial.println("[OpenAI] No API key configured");
            return false;
        }
        
        apiKey = key;
        conversationMode = mode;
        
        Serial.println("[OpenAI] Connecting to Realtime API...");
        
        // Set authorization header
        String auth = "Authorization: Bearer " + apiKey;
        String protocol = "OpenAI-Beta: realtime=v1";
        
        wsClient.beginSslWithCA(OPENAI_WS_HOST, OPENAI_WS_PORT, OPENAI_WS_PATH,
                                SallyTls::kRootCaPem, protocol.c_str());
        wsClient.setExtraHeaders(auth.c_str());
        wsClient.setReconnectInterval(5000);
        
        // Wait for connection
        unsigned long startTime = millis();
        while (!isConnected && (millis() - startTime < 10000)) {
            wsClient.loop();
            delay(10);
        }
        
        return isConnected;
    }
    
    void disconnect() {
        if (isConnected) {
            wsClient.disconnect();
            isConnected = false;
            isRecording = false;
        }
    }
    
    void loop() {
        if (isConnected) {
            wsClient.loop();
            
            // Stream audio if recording
            if (isRecording) {
                streamAudioChunk();
            }
        }
    }
    
    void startRecording() {
        if (!isConnected) return;
        
        isRecording = true;
        Serial.println("[OpenAI] Recording started");
        
        // Send input audio buffer append event
        JsonDocument doc;
        doc["type"] = "input_audio_buffer.append";
        
        String output;
        serializeJson(doc, output);
        // We'll append actual audio data in streamAudioChunk()
    }
    
    void stopRecording() {
        if (!isConnected || !isRecording) return;
        
        isRecording = false;
        Serial.println("[OpenAI] Recording stopped");
        
        // Commit the audio buffer
        JsonDocument doc;
        doc["type"] = "input_audio_buffer.commit";
        String output;
        serializeJson(doc, output);
        wsClient.sendTXT(output);
        
        // Request response generation
        doc.clear();
        doc["type"] = "response.create";
        output = "";
        serializeJson(doc, output);
        wsClient.sendTXT(output);
    }
    
    void streamAudioChunk() {
        // Read from microphone
        size_t bytes_read;
        i2s_read(I2S_NUM_0, audioBuffer, BUFFER_SIZE * 2, &bytes_read, 100);
        
        if (bytes_read > 0) {
            // Upsample from 16kHz to 24kHz if needed
            // For now, send as-is (you may need to adjust mic config to 24kHz)
            
            // Base64 encode audio
            String base64Audio = base64Encode((uint8_t*)audioBuffer, bytes_read);
            
            // Send to OpenAI
            JsonDocument doc;
            doc["type"] = "input_audio_buffer.append";
            doc["audio"] = base64Audio;
            
            String output;
            serializeJson(doc, output);
            wsClient.sendTXT(output);
        }
    }
    
    bool isActive() {
        return isConnected;
    }
};

#endif