#ifndef ELEVENLABS_CONFIG_H
#define ELEVENLABS_CONFIG_H

// ElevenLabs Conversational AI Configuration
// Get your API key from: https://elevenlabs.io/app/conversational-ai

struct ElevenLabsConfig {
    String apiKey = "";
    String agentId = "";  // Your agent ID from ElevenLabs
    
    // WebSocket endpoint
    const char* wsHost = "api.elevenlabs.io";
    const int wsPort = 443;
    const char* wsPath = "/v1/convai/conversation";
    
    // Audio settings
    const int sampleRate = 16000;
    const int bitDepth = 16;
    const int channels = 1;
};

#endif