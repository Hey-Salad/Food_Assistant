#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_camera.h>

#include "config.h"
#include "camera.h"
#include "logo.h"
#include "audio.h"

Preferences preferences;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String wifiSSID = "";
String wifiPassword = "";
bool isConfigured = false;
bool cameraOK = false;
bool audioOK = false;

const char* mainHTML = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>HeySalad</title>
<style>@import url('https://fonts.googleapis.com/css2?family=Figtree:wght@400;600&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Figtree',sans-serif;background:white;min-height:100vh;padding:20px}.header{text-align:center;margin-bottom:20px}.logo-container{display:inline-block}.logo-container svg{width:200px;height:auto}.tagline{color:#666;font-size:12px;margin-top:8px;letter-spacing:0.5px}.nav{display:flex;gap:10px;justify-content:center;margin-bottom:20px}.nav button{padding:10px 20px;background:white;border:2px solid #ed4c4c;border-radius:10px;color:#ed4c4c;cursor:pointer;font-weight:600;font-size:14px}.nav button.active{background:#ed4c4c;color:white}.page{display:none}.page.active{display:block}.container{max-width:600px;margin:0 auto;background:white;padding:40px;border-radius:20px;box-shadow:0 10px 40px rgba(0,0,0,0.1)}.status{text-align:center;padding:15px;border-radius:10px;margin-bottom:20px;font-weight:600;background:#f0f0f0;color:#666}.status.listening{background:#d4edda;color:#155724;animation:pulse 1.5s infinite}@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.7}}.talk-button{width:200px;height:200px;border-radius:50%;border:none;background:linear-gradient(135deg,#ed4c4c 0%,#d43939 100%);color:white;font-size:24px;font-weight:700;cursor:pointer;margin:20px auto;display:flex;flex-direction:column;align-items:center;justify-content:center;box-shadow:0 10px 30px rgba(237,76,76,0.4)}.talk-button:active{transform:scale(0.95)}.talk-button.active{background:linear-gradient(135deg,#28a745 0%,#1e7e34 100%)}.camera-view{text-align:center;margin-top:20px}.camera-view img{max-width:100%;border-radius:15px;box-shadow:0 5px 20px rgba(0,0,0,0.1)}.actions{display:flex;gap:10px;justify-content:center;margin-top:20px}.actions button{padding:12px 24px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-weight:600}.info-box{padding:15px;background:#f8f9fa;border-radius:10px;margin-bottom:15px}.info-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #dee2e6}.info-row:last-child{border-bottom:none}.info-label{font-weight:600;color:#495057}.info-value{color:#6c757d}.status-badge{padding:4px 12px;border-radius:20px;font-size:12px;font-weight:600}.status-ok{background:#d4edda;color:#155724}.status-fail{background:#f8d7da;color:#721c24}h2{text-align:center;color:#ed4c4c;margin-bottom:20px;font-size:28px}</style></head><body>
<div class="header"><div class="logo-container" id="logoContainer"></div><div class="tagline">HeySalad ® Food AI Assistant</div></div>
<div class="nav"><button onclick="showPage('home')" class="active" id="nav-home">Voice</button><button onclick="showPage('camera')" id="nav-camera">Camera</button><button onclick="showPage('settings')" id="nav-settings">Settings</button></div>
<div class="page active" id="page-home"><div class="container"><div class="status" id="status">Ready</div><button class="talk-button" id="btn"><div style="font-size:48px;margin-bottom:10px">🎤</div><div>Hold to Talk</div></button></div></div>
<div class="page" id="page-camera"><div class="container"><h2>Camera</h2><div class="camera-view"><img id="cameraImg" src="/capture" style="width:100%"></div><div class="actions"><button onclick="refreshCamera()">Refresh</button></div></div></div>
<div class="page" id="page-settings"><div class="container"><h2>Status</h2><div class="info-box"><div class="info-row"><span class="info-label">Camera</span><span class="status-badge" id="cameraStatus">Loading...</span></div><div class="info-row"><span class="info-label">Audio</span><span class="status-badge" id="audioStatus">Loading...</span></div><div class="info-row"><span class="info-label">PSRAM</span><span class="info-value" id="psramStatus">Loading...</span></div><div class="info-row"><span class="info-label">IP</span><span class="info-value" id="wifiIP">Loading...</span></div></div><div class="actions"><button onclick="testAudio()">Test Audio</button></div></div></div>
<script>
fetch('/logo.svg').then(r=>r.text()).then(svg=>document.getElementById('logoContainer').innerHTML=svg);
let ws;let isHolding=false;const btn=document.getElementById('btn');
function showPage(page){document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));document.querySelectorAll('.nav button').forEach(b=>b.classList.remove('active'));document.getElementById('page-'+page).classList.add('active');document.getElementById('nav-'+page).classList.add('active')}
function connect(){ws=new WebSocket('ws://'+location.hostname+'/ws');ws.onopen=()=>console.log('Connected');ws.onmessage=(event)=>{const data=JSON.parse(event.data);if(data.type==='status'){document.getElementById('status').textContent=data.text;document.getElementById('status').className='status '+(data.listening?'listening':'')}};ws.onclose=()=>setTimeout(connect,2000)}
btn.onmousedown=btn.ontouchstart=(e)=>{e.preventDefault();if(isHolding)return;isHolding=true;btn.classList.add('active');ws.send(JSON.stringify({type:'start'}))};
btn.onmouseup=btn.ontouchend=(e)=>{e.preventDefault();if(!isHolding)return;isHolding=false;btn.classList.remove('active');ws.send(JSON.stringify({type:'stop'}))};
function refreshCamera(){document.getElementById('cameraImg').src='/capture?'+Date.now()}
function testAudio(){fetch('/test_audio').then(r=>r.text()).then(msg=>alert(msg))}
fetch('/status').then(r=>r.json()).then(data=>{document.getElementById('cameraStatus').textContent=data.camera?'OK':'FAIL';document.getElementById('cameraStatus').className='status-badge '+(data.camera?'status-ok':'status-fail');document.getElementById('audioStatus').textContent=data.audio?'OK':'FAIL';document.getElementById('audioStatus').className='status-badge '+(data.audio?'status-ok':'status-fail');document.getElementById('psramStatus').textContent=data.psram;document.getElementById('wifiIP').textContent=data.ip});
connect()
</script></body></html>)rawliteral";

const char* setupHTML = R"(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Setup</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:#ffd0cd;padding:20px;display:flex;align-items:center;justify-content:center;min-height:100vh}.container{background:white;padding:40px;border-radius:20px;max-width:500px;width:100%}h1{color:#ed4c4c;text-align:center;margin-bottom:30px}.form-group{margin-bottom:20px}label{display:block;margin-bottom:8px;font-weight:600}input{width:100%;padding:12px;border:2px solid #ffd0cd;border-radius:10px;font-size:16px}button{width:100%;padding:15px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-size:16px;font-weight:600}</style></head><body><div class="container"><h1>HeySalad Setup</h1><form id="form"><div class="form-group"><label>WiFi SSID</label><input type="text" id="ssid" required></div><div class="form-group"><label>WiFi Password</label><input type="password" id="password" required></div><button type="submit">Save & Restart</button></form></div><script>document.getElementById('form').onsubmit=async(e)=>{e.preventDefault();await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:document.getElementById('ssid').value,password:document.getElementById('password').value})});alert('Saved! Restarting...')}</script></body></html>)";

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        JsonDocument doc;
        deserializeJson(doc, data);
        String msgType = doc["type"].as<String>();
        
        if (msgType == "start") {
            digitalWrite(LED_GPIO_NUM, HIGH);
            Serial.println("Voice started");
            client->text("{\"type\":\"status\",\"text\":\"Listening...\",\"listening\":true}");
            
            // TODO: Start recording audio here
            
        } else if (msgType == "stop") {
            digitalWrite(LED_GPIO_NUM, LOW);
            Serial.println("Voice stopped");
            client->text("{\"type\":\"status\",\"text\":\"Processing...\",\"listening\":false}");
            
            // TODO: Stop recording and process audio here
            
            delay(1000);
            client->text("{\"type\":\"status\",\"text\":\"Ready\",\"listening\":false}");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n=== HeySalad Starting ===");
    
    pinMode(LED_GPIO_NUM, OUTPUT);
    
    preferences.begin("heysalad", true);
    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    isConfigured = preferences.getBool("configured", false);
    preferences.end();
    
    if (!wifiSSID.isEmpty()) {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        Serial.print("Connecting to WiFi");
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nWiFi: %s\n", WiFi.localIP().toString().c_str());
        }
    } else {
        WiFi.softAP("HeySalad-Setup");
        Serial.println("AP: HeySalad-Setup @ 192.168.4.1");
    }
    
    cameraOK = initCamera();
    audioOK = initAudio();
    
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", isConfigured ? mainHTML : setupHTML);
    });
    
    server.on("/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "image/svg+xml", logoSVG);
    });
    
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            request->send(500, "text/plain", "Camera failed");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse_P(
            200, "image/jpeg", fb->buf, fb->len
        );
        request->send(response);
        esp_camera_fb_return(fb);
    });
    
    server.on("/test_audio", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Testing audio... Check serial monitor");
        testAudioLoopback(3);  // 3 second test
    });
    
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"camera\":" + String(cameraOK ? "true" : "false") + ",";
        json += "\"audio\":" + String(audioOK ? "true" : "false") + ",";
        json += "\"psram\":\"" + String(ESP.getFreePsram() / 1024) + " KB\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            deserializeJson(doc, data);
            
            preferences.begin("heysalad", false);
            preferences.putString("ssid", doc["ssid"].as<String>());
            preferences.putString("password", doc["password"].as<String>());
            preferences.putBool("configured", true);
            preferences.end();
            
            request->send(200, "application/json", "{\"ok\":true}");
            delay(1000);
            ESP.restart();
        });
    
    server.begin();
    Serial.println("Server started");
    Serial.printf("Camera: %s | Audio: %s\n", 
                  cameraOK ? "OK" : "FAIL",
                  audioOK ? "OK" : "FAIL");
}

void loop() {
    ws.cleanupClients();
    delay(10);
}