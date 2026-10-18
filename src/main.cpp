#if 0  // Superseded duplicate implementation retained from origin/main.
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n=== HeySalad Starting ===");
    
    pinMode(LED_GPIO_NUM, OUTPUT);
    randomSeed(analogRead(0));
    
    // Initialize authentication
    auth.begin();
    
    preferences.begin("heysalad", true);
    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    openaiKey = preferences.getString("openai", "");
    elevenlabsKey = preferences.getString("elevenlabs", "");
    geminiKey = preferences.getString("gemini", "");
    deepseekKey = preferences.getString("deepseek", "");
    isConfigured = preferences.getBool("configured", false);
    preferences.end();
    
    if (wifiSSID.isEmpty() && strlen(FALLBACK_WIFI_SSID) > 0) {
        // Provisioning over the portal or BLE still wins; this only saves the
        // AP dance on a device that has never been set up.
        wifiSSID = FALLBACK_WIFI_SSID;
        wifiPassword = FALLBACK_WIFI_PASSWORD;
        preferences.begin("heysalad", false);
        preferences.putString("ssid", wifiSSID);
        preferences.putString("password", wifiPassword);
        preferences.end();
        Serial.println("WiFi: using built-in fallback credentials");
    }

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
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        }
    } else {
        WiFi.softAP("HeySalad-Setup");
        Serial.println("AP: HeySalad-Setup @ 192.168.4.1");
    }
    
    cameraOK = initCamera();
    audioOK = initAudio();
    
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    
    // Main page (requires auth)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isConfigured) {
            request->send(200, "text/html", setupHTML);
            return;
        }
        
        String token = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
        if (token.isEmpty() && request->hasParam("token")) {
            token = request->getParam("token")->value();
        }
        
        if (auth.verifySession(token)) {
            request->send(200, "text/html", mainHTML);
        } else {
            request->send(200, "text/html", loginHTML);
        }
    });
    
    // Login endpoint
    server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            deserializeJson(doc, data);
            String password = doc["password"].as<String>();
            
            String token = auth.login(password);
            if (!token.isEmpty()) {
                String json = "{\"token\":\"" + token + "\",\"ok\":true}";
                request->send(200, "application/json", json);
            } else {
                request->send(401, "application/json", "{\"ok\":false,\"error\":\"Invalid password\"}");
            }
        });
    
    // Logout endpoint
    server.on("/logout", HTTP_POST, [](AsyncWebServerRequest *request) {
        String token = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
        auth.logout(token);
        request->send(200, "application/json", "{\"ok\":true}");
    });
    
    // Check if using default password
    server.on("/is_default_password", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool isDefault = !auth.isSetupComplete();
        String json = "{\"isDefault\":" + String(isDefault ? "true" : "false") + "}";
        request->send(200, "application/json", json);
    });
    
    // Change password
    server.on("/change_password", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String token = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
            if (!auth.verifySession(token)) {
                request->send(401, "application/json", "{\"ok\":false,\"error\":\"Unauthorized\"}");
                return;
            }
            
            JsonDocument doc;
            deserializeJson(doc, data);
            String oldPwd = doc["old"].as<String>();
            String newPwd = doc["new"].as<String>();
            
            if (auth.changePassword(oldPwd, newPwd)) {
                request->send(200, "application/json", "{\"ok\":true}");
            } else {
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid old password\"}");
            }
        });
    
    server.on("/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "image/svg+xml", logoSVG);
    });
    
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
        String token = request->hasParam("token") ? request->getParam("token")->value() : "";
        if (!auth.verifySession(token)) {
            request->send(401, "text/plain", "Unauthorized");
            return;
        }
        
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
    
    server.on("/get_keys", HTTP_GET, [](AsyncWebServerRequest *request) {
        String token = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
        if (!auth.verifySession(token)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        
        String json = "{";
        json += "\"openai\":\"" + (openaiKey.isEmpty() ? "" : "***") + "\",";
        json += "\"elevenlabs\":\"" + (elevenlabsKey.isEmpty() ? "" : "***") + "\",";
        json += "\"gemini\":\"" + (geminiKey.isEmpty() ? "" : "***") + "\",";
        json += "\"deepseek\":\"" + (deepseekKey.isEmpty() ? "" : "***") + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/save_keys", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String token = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
            if (!auth.verifySession(token)) {
                request->send(401, "application/json", "{\"ok\":false}");
                return;
            }
            
            JsonDocument doc;
            deserializeJson(doc, data);
            
            preferences.begin("heysalad", false);
            if (doc["openai"].as<String>() != "***") preferences.putString("openai", doc["openai"].as<String>());
            if (doc["elevenlabs"].as<String>() != "***") preferences.putString("elevenlabs", doc["elevenlabs"].as<String>());
            if (doc["gemini"].as<String>() != "***") preferences.putString("gemini", doc["gemini"].as<String>());
            if (doc["deepseek"].as<String>() != "***") preferences.putString("deepseek", doc["deepseek"].as<String>());
            preferences.end();
            
            openaiKey = preferences.getString("openai", "");
            elevenlabsKey = preferences.getString("elevenlabs", "");
            geminiKey = preferences.getString("gemini", "");
            deepseekKey = preferences.getString("deepseek", "");
            
            request->send(200, "application/json", "{\"ok\":true}");
        });
    
    server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request) {
        String token = request->getHeader("Authorization") ? request->getHeader("Authorization")->value() : "";
        if (!auth.verifySession(token)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        
        String json = "{";
        json += "\"camera\":" + String(cameraOK ? "true" : "false") + ",";
        json += "\"audio\":" + String(audioOK ? "true" : "false") + ",";
        json += "\"psram\":\"" + String(ESP.getFreePsram() / 1024) + "K\",";
        json += "\"heap\":\"" + String(ESP.getFreeHeap() / 1024) + "K\",";
        json += "\"temp\":\"" + String(temperatureRead(), 0) + "°C\",";
        json += "\"wifi\":\"" + String(WiFi.RSSI()) + "dBm\"";
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
    Serial.printf("Sessions: %d active\n", auth.getActiveSessionCount());
    
    if (!auth.isSetupComplete()) {
        Serial.println("\n⚠️  SECURITY WARNING ⚠️");
        Serial.println("Using default password: admin123");
        Serial.println("Please change password on first login!");
    }
}

void loop() {
    if (otaReady) ArduinoOTA.handle();
    ws.cleanupClients();
    
    // Process OpenAI WebSocket if active
    if (openaiClient.isActive()) {
        openaiClient.loop();
    }
    
    delay(10);
}const char* mainHTML = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>HeySalad</title>
<style>@import url('https://fonts.googleapis.com/css2?family=Figtree:wght@400;600&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Figtree',sans-serif;background:white;min-height:100vh;padding:20px}.header{text-align:center;margin-bottom:20px;position:relative}.logo-container{display:inline-block}.logo-container svg{width:200px;height:auto}.tagline{color:#666;font-size:12px;margin-top:8px;letter-spacing:0.5px}.settings-icon{position:absolute;top:0;right:20px;cursor:pointer;font-size:28px;color:#ed4c4c;transition:transform 0.3s}.settings-icon:hover{transform:rotate(90deg)}.logout-btn{position:absolute;top:0;left:20px;padding:8px 16px;background:#dc3545;color:white;border:none;border-radius:8px;cursor:pointer;font-size:12px;font-weight:600}.nav{display:flex;gap:10px;justify-content:center;margin-bottom:20px}.nav button{padding:10px 20px;background:white;border:2px solid #ed4c4c;border-radius:10px;color:#ed4c4c;cursor:pointer;font-weight:600;font-size:14px}.nav button.active{background:#ed4c4c;color:white}.page{display:none}.page.active{display:block}.container{max-width:600px;margin:0 auto;background:white;padding:40px;border-radius:20px;box-shadow:0 10px 40px rgba(0,0,0,0.1)}.status{text-align:center;padding:15px;border-radius:10px;margin-bottom:20px;font-weight:600;background:#f0f0f0;color:#666}.status.calling{background:#d4edda;color:#155724;animation:pulse 1.5s infinite}.status.energy{background:#fff3cd;color:#856404}@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.7}}.call-button{width:200px;height:200px;border-radius:50%;border:none;background:linear-gradient(135deg,#28a745 0%,#1e7e34 100%);color:white;font-size:24px;font-weight:700;cursor:pointer;margin:20px auto;display:flex;flex-direction:column;align-items:center;justify-content:center;box-shadow:0 10px 30px rgba(40,167,69,0.4);transition:all 0.3s}.call-button:hover{transform:scale(1.05)}.call-button.active{background:linear-gradient(135deg,#dc3545 0%,#c82333 100%);box-shadow:0 10px 30px rgba(220,53,69,0.4)}.camera-view{text-align:center;margin-top:20px}.camera-view img{max-width:100%;border-radius:15px;box-shadow:0 5px 20px rgba(0,0,0,0.1)}.actions{display:flex;gap:10px;justify-content:center;margin-top:20px;flex-wrap:wrap}.actions button{padding:12px 24px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-weight:600}select{width:100%;padding:12px;border:2px solid #ed4c4c;border-radius:10px;font-size:16px;margin-bottom:15px;font-weight:600;color:#495057;background:white;cursor:pointer}.settings-modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:1000;align-items:center;justify-content:center}.settings-modal.active{display:flex}.settings-content{background:white;padding:40px;border-radius:20px;max-width:600px;width:90%;max-height:80vh;overflow-y:auto}.settings-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:30px}.close-btn{font-size:32px;cursor:pointer;color:#666;line-height:1}.close-btn:hover{color:#ed4c4c}.form-group{margin-bottom:20px}label{display:block;margin-bottom:8px;font-weight:600;color:#495057}input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:10px;font-size:14px}input:focus{outline:none;border-color:#ed4c4c}.save-btn{width:100%;padding:15px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-size:16px;font-weight:600;margin-top:20px}.metric-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin-top:20px}.metric-card{background:#f8f9fa;padding:15px;border-radius:10px;text-align:center}.metric-label{font-size:12px;color:#6c757d;margin-bottom:5px}.metric-value{font-size:24px;font-weight:700;color:#ed4c4c}.energy-badge{display:inline-block;padding:4px 12px;background:#28a745;color:white;border-radius:20px;font-size:11px;font-weight:600;margin-left:8px}h2{text-align:center;color:#ed4c4c;margin-bottom:20px;font-size:28px}h3{color:#495057;margin-bottom:15px;margin-top:25px;font-size:18px}.info-text{background:#e7f3ff;border-left:4px solid:#2196f3;padding:12px;border-radius:8px;margin:15px 0;font-size:14px;color:#1565c0}</style></head><body>
<div class="header"><button class="logout-btn" onclick="logout()">Logout</button><div class="logo-container" id="logoContainer"></div><div class="tagline">HeySalad ® Food Assistant</div><div class="settings-icon" onclick="openSettings()">⚙️</div></div>
<div class="nav"><button onclick="showPage('call')" class="active" id="nav-call">Call</button><button onclick="showPage('camera')" id="nav-camera">Camera</button></div>
<div class="page active" id="page-call"><div class="container"><h2>AI Assistant Call</h2><select id="provider"><option value="openai">OpenAI Realtime<span class="energy-badge">♻️ Energy Smart</span></option><option value="elevenlabs">ElevenLabs (Coming Soon)</option><option value="gemini">Google Gemini (Coming Soon)</option><option value="deepseek">DeepSeek (Coming Soon)</option></select><select id="mode"><option value="chef">Chef Mode 👨‍🍳</option><option value="service">Service Mode 🛎️</option><option value="general">General Assistant 💬</option></select><div class="info-text">♻️ <strong>Energy Conscious:</strong> Camera only activates when AI requests to see ingredients. This saves power and helps the environment!</div><div class="status" id="call-status">Select options and tap to start</div><button class="call-button" id="call-btn"><div style="font-size:64px;margin-bottom:10px">📞</div><div id="call-btn-text">Start Call</div></button></div></div>
<div class="page" id="page-camera"><div class="container"><h2>Camera</h2><div class="camera-view"><img id="cameraImg" src="/capture" style="width:100%"></div><div class="actions"><button onclick="refreshCamera()">Refresh</button></div></div></div>
<div class="settings-modal" id="settingsModal"><div class="settings-content"><div class="settings-header"><h2 style="margin:0">Settings</h2><span class="close-btn" onclick="closeSettings()">×</span></div><h3>🔐 Change Password</h3><div class="form-group"><label>Current Password</label><input type="password" id="old-password"></div><div class="form-group"><label>New Password</label><input type="password" id="new-password"></div><div class="form-group"><label>Confirm New Password</label><input type="password" id="confirm-password"></div><button class="save-btn" onclick="changePassword()">🔒 Update Password</button><h3>🔑 API Keys</h3><div class="form-group"><label>OpenAI API Key</label><input type="password" id="openai-key" placeholder="sk-proj-..."></div><div class="form-group"><label>ElevenLabs API Key</label><input type="password" id="elevenlabs-key" placeholder="el_..."></div><div class="form-group"><label>Google Gemini API Key</label><input type="password" id="gemini-key" placeholder="AI..."></div><div class="form-group"><label>DeepSeek API Key</label><input type="password" id="deepseek-key" placeholder="sk-..."></div><button class="save-btn" onclick="saveSettings()">💾 Save Credentials</button><h3>📊 System Status</h3><div class="metric-grid"><div class="metric-card"><div class="metric-label">Camera</div><div class="metric-value" id="status-camera">-</div></div><div class="metric-card"><div class="metric-label">Audio</div><div class="metric-value" id="status-audio">-</div></div><div class="metric-card"><div class="metric-label">PSRAM</div><div class="metric-value" id="status-psram">-</div></div><div class="metric-card"><div class="metric-label">Heap Free</div><div class="metric-value" id="status-heap">-</div></div><div class="metric-card"><div class="metric-label">Temperature</div><div class="metric-value" id="status-temp">-</div></div><div class="metric-card"><div class="metric-label">WiFi Signal</div><div class="metric-value" id="status-wifi">-</div></div></div></div></div>
<script>
let sessionToken=localStorage.getItem('heysalad_token')||'';
fetch('/logo.svg').then(r=>r.text()).then(svg=>document.getElementById('logoContainer').innerHTML=svg);
let ws;let isInCall=false;
const callBtn=document.getElementById('call-btn');const callBtnText=document.getElementById('call-btn-text');
function showPage(page){document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));document.querySelectorAll('.nav button').forEach(b=>b.classList.remove('active'));document.getElementById('page-'+page).classList.add('active');document.getElementById('nav-'+page).classList.add('active')}
function openSettings(){document.getElementById('settingsModal').classList.add('active');loadSettings();updateMetrics()}
function closeSettings(){document.getElementById('settingsModal').classList.remove('active')}
function logout(){localStorage.removeItem('heysalad_token');fetch('/logout',{method:'POST',headers:{'Authorization':sessionToken}}).then(()=>window.location.reload())}
function changePassword(){const oldPwd=document.getElementById('old-password').value;const newPwd=document.getElementById('new-password').value;const confirmPwd=document.getElementById('confirm-password').value;if(newPwd!==confirmPwd){alert('❌ Passwords do not match');return}if(newPwd.length<6){alert('❌ Password must be at least 6 characters');return}fetch('/change_password',{method:'POST',headers:{'Content-Type':'application/json','Authorization':sessionToken},body:JSON.stringify({old:oldPwd,new:newPwd})}).then(r=>r.json()).then(data=>{if(data.ok){alert('✅ Password changed successfully! Please login again.');logout()}else{alert('❌ '+data.error)}})}
function loadSettings(){fetch('/get_keys',{headers:{'Authorization':sessionToken}}).then(r=>r.json()).then(data=>{document.getElementById('openai-key').value=data.openai||'';document.getElementById('elevenlabs-key').value=data.elevenlabs||'';document.getElementById('gemini-key').value=data.gemini||'';document.getElementById('deepseek-key').value=data.deepseek||''})}
function saveSettings(){const keys={openai:document.getElementById('openai-key').value,elevenlabs:document.getElementById('elevenlabs-key').value,gemini:document.getElementById('gemini-key').value,deepseek:document.getElementById('deepseek-key').value};fetch('/save_keys',{method:'POST',headers:{'Content-Type':'application/json','Authorization':sessionToken},body:JSON.stringify(keys)}).then(r=>r.json()).then(data=>{alert('✅ Credentials saved!');closeSettings()})}
function updateMetrics(){fetch('/metrics',{headers:{'Authorization':sessionToken}}).then(r=>r.json()).then(data=>{document.getElementById('status-camera').textContent=data.camera?'✓':'✗';document.getElementById('status-audio').textContent=data.audio?'✓':'✗';document.getElementById('status-psram').textContent=data.psram;document.getElementById('status-heap').textContent=data.heap;document.getElementById('status-temp').textContent=data.temp;document.getElementById('status-wifi').textContent=data.wifi})}
function connect(){ws=new WebSocket('ws://'+location.hostname+'/ws');ws.onopen=()=>{console.log('Connected');ws.send(JSON.stringify({type:'auth',token:sessionToken}))};ws.onmessage=(event)=>{const data=JSON.parse(event.data);if(data.type==='call_status'){document.getElementById('call-status').textContent=data.text;document.getElementById('call-status').className='status '+(data.calling?'calling':'')+(data.energy?' energy':'')}};ws.onclose=()=>setTimeout(connect,2000)}
callBtn.onclick=(e)=>{e.preventDefault();const provider=document.getElementById('provider').value;const mode=document.getElementById('mode').value;if(provider!=='openai'){alert('⚠️ Only OpenAI Realtime is currently available');return}if(!isInCall){isInCall=true;callBtn.classList.add('active');callBtnText.textContent='End Call';ws.send(JSON.stringify({type:'call_start',provider:provider,mode:mode}))}else{isInCall=false;callBtn.classList.remove('active');callBtnText.textContent='Start Call';ws.send(JSON.stringify({type:'call_end'}))}};
function refreshCamera(){document.getElementById('cameraImg').src='/capture?'+Date.now()+'&token='+sessionToken}
setInterval(updateMetrics,5000);
connect()
</script></body></html>)rawliteral";

#endif

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
#include "auth.h"
#include "openai_realtime.h"
#include "spatial_client.h"
#include "spatial_protocol.h"
#include "sally_policy.h"
#include "ble_provisioner.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "secrets.h"

// Night vision. The DFR1154 carries an IR LED here; without it a capture taken
// after the door shuts is a black frame, because the fridge's own lamp is out.
#define IR_LED_PIN 47

static bool otaReady = false;

static void setIRLED(bool on) {
  digitalWrite(IR_LED_PIN, on ? HIGH : LOW);
}

Preferences preferences;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AuthManager auth;
OpenAIRealtime openaiClient;
SallySpatialClient spatialClient;
SallyBleProvisioner bleProvisioner;
SemaphoreHandle_t cameraMutex = nullptr;

String wifiSSID = "";
String wifiPassword = "";
String adminToken = "";
bool isConfigured = false;
bool cloudProvisioned = false;
bool cameraOK = false;
bool audioOK = false;

const char* mainHTML = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>HeySalad</title>
<style>@import url('https://fonts.googleapis.com/css2?family=Figtree:wght@400;600&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Figtree',sans-serif;background:white;min-height:100vh;padding:20px}.header{text-align:center;margin-bottom:20px;position:relative}.logo-container{display:inline-block}.logo-container svg{width:200px;height:auto}.tagline{color:#666;font-size:12px;margin-top:8px;letter-spacing:0.5px}.settings-icon{position:absolute;top:0;right:20px;cursor:pointer;font-size:28px;color:#ed4c4c;transition:transform 0.3s}.settings-icon:hover{transform:rotate(90deg)}.nav{display:flex;gap:10px;justify-content:center;margin-bottom:20px}.nav button{padding:10px 20px;background:white;border:2px solid #ed4c4c;border-radius:10px;color:#ed4c4c;cursor:pointer;font-weight:600;font-size:14px}.nav button.active{645:#ed4c4c;color:white}.page{display:none}.page.active{display:block}.container{max-width:600px;margin:0 auto;background:white;padding:40px;border-radius:20px;box-shadow:0 10px 40px rgba(0,0,0,0.1)}.status{text-align:center;padding:15px;border-radius:10px;margin-bottom:20px;font-weight:600;background:#f0f0f0;color:#666}.status.calling{background:#d4edda;color:#155724;animation:pulse 1.5s infinite}@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.7}}.call-button{width:200px;height:200px;border-radius:50%;border:none;background:linear-gradient(135deg,#28a745 0%,#1e7e34 100%);color:white;font-size:24px;font-weight:700;cursor:pointer;margin:20px auto;display:flex;flex-direction:column;align-items:center;justify-content:center;box-shadow:0 10px 30px rgba(40,167,69,0.4);transition:all 0.3s}.call-button:hover{transform:scale(1.05)}.call-button.active{background:linear-gradient(135deg,#dc3545 0%,#c82333 100%);box-shadow:0 10px 30px rgba(220,53,69,0.4)}.camera-view{text-align:center;margin-top:20px}.camera-view img{max-width:100%;border-radius:15px;box-shadow:0 5px 20px rgba(0,0,0,0.1)}.actions{display:flex;gap:10px;justify-content:center;margin-top:20px;flex-wrap:wrap}.actions button{padding:12px 24px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-weight:600}select{width:100%;padding:12px;border:2px solid #ed4c4c;border-radius:10px;font-size:16px;margin-bottom:15px;font-weight:600;color:#495057;background:white;cursor:pointer}.settings-modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:1000;align-items:center;justify-content:center}.settings-modal.active{display:flex}.settings-content{background:white;padding:40px;border-radius:20px;max-width:600px;width:90%;max-height:80vh;overflow-y:auto}.settings-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:30px}.close-btn{font-size:32px;cursor:pointer;color:#666;line-height:1}.close-btn:hover{color:#ed4c4c}.form-group{margin-bottom:20px}label{display:block;margin-bottom:8px;font-weight:600;color:#495057}input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:10px;font-size:14px}input:focus{outline:none;border-color:#ed4c4c}.save-btn{width:100%;padding:15px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-size:16px;font-weight:600;margin-top:20px}.metric-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin-top:20px}.metric-card{background:#f8f9fa;padding:15px;border-radius:10px;text-align:center}.metric-label{font-size:12px;color:#6c757d;margin-bottom:5px}.metric-value{font-size:24px;font-weight:700;color:#ed4c4c}h2{text-align:center;color:#ed4c4c;margin-bottom:20px;font-size:28px}h3{color:#495057;margin-bottom:15px;margin-top:25px;font-size:18px}</style></head><body>
<div class="header"><div class="logo-container" id="logoContainer"></div><div class="tagline">HeySalad ® Food Assistant</div><div class="settings-icon" onclick="openSettings()">⚙️</div></div>
<div class="nav"><button onclick="showPage('call')" class="active" id="nav-call">Call</button><button onclick="showPage('camera')" id="nav-camera">Camera</button></div>
<div class="page active" id="page-call"><div class="container"><h2>AI Assistant Call</h2><select id="provider"><option value="openai">OpenAI Realtime</option><option value="elevenlabs">ElevenLabs</option><option value="gemini">Google Gemini</option><option value="deepseek">DeepSeek</option></select><select id="mode"><option value="chef">Chef Mode 👨‍🍳</option><option value="service">Service Mode 🛎️</option><option value="general">General Assistant 💬</option></select><div class="status" id="call-status">Select options and tap to start</div><button class="call-button" id="call-btn"><div style="font-size:64px;margin-bottom:10px">📞</div><div id="call-btn-text">Start Call</div></button></div></div>
<div class="page" id="page-camera"><div class="container"><h2>Camera</h2><p>Snapshots upload to Sally. This camera does not serve an open capture URL or store model API keys.</p></div></div>
<div class="settings-modal" id="settingsModal"><div class="settings-content"><div class="settings-header"><h2 style="margin:0">Settings</h2><span class="close-btn" onclick="closeSettings()">×</span></div><h3>Model keys</h3><p>OpenAI, ElevenLabs, Gemini, and DeepSeek keys are not stored on this camera. Inference runs server-side through Sally.</p><h3>📊 System Status</h3><div class="metric-grid"><div class="metric-card"><div class="metric-label">Camera</div><div class="metric-value" id="status-camera">-</div></div><div class="metric-card"><div class="metric-label">Audio</div><div class="metric-value" id="status-audio">-</div></div><div class="metric-card"><div class="metric-label">PSRAM</div><div class="metric-value" id="status-psram">-</div></div><div class="metric-card"><div class="metric-label">Heap Free</div><div class="metric-value" id="status-heap">-</div></div><div class="metric-card"><div class="metric-label">Temperature</div><div class="metric-value" id="status-temp">-</div></div><div class="metric-card"><div class="metric-label">WiFi Signal</div><div class="metric-value" id="status-wifi">-</div></div></div></div></div>
<script>
fetch('/logo.svg').then(r=>r.text()).then(svg=>document.getElementById('logoContainer').innerHTML=svg);
let ws;let isInCall=false;
const callBtn=document.getElementById('call-btn');const callBtnText=document.getElementById('call-btn-text');
function showPage(page){document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));document.querySelectorAll('.nav button').forEach(b=>b.classList.remove('active'));document.getElementById('page-'+page).classList.add('active');document.getElementById('nav-'+page).classList.add('active')}
function openSettings(){document.getElementById('settingsModal').classList.add('active');updateMetrics()}
function closeSettings(){document.getElementById('settingsModal').classList.remove('active')}
function updateMetrics(){fetch('/metrics').then(r=>r.json()).then(data=>{document.getElementById('status-camera').textContent=data.camera?'✓':'✗';document.getElementById('status-audio').textContent=data.audio?'✓':'✗';document.getElementById('status-psram').textContent=data.psram;document.getElementById('status-heap').textContent=data.heap;document.getElementById('status-temp').textContent=data.temp;document.getElementById('status-wifi').textContent=data.wifi})}
function connect(){ws=new WebSocket('ws://'+location.hostname+'/ws');ws.onopen=()=>console.log('Connected');ws.onmessage=(event)=>{const data=JSON.parse(event.data);if(data.type==='call_status'){document.getElementById('call-status').textContent=data.text;document.getElementById('call-status').className='status '+(data.calling?'calling':'')}};ws.onclose=()=>setTimeout(connect,2000)}
callBtn.onclick=(e)=>{e.preventDefault();const provider=document.getElementById('provider').value;const mode=document.getElementById('mode').value;if(!isInCall){isInCall=true;callBtn.classList.add('active');callBtnText.textContent='End Call';ws.send(JSON.stringify({type:'call_start',provider:provider,mode:mode}))}else{isInCall=false;callBtn.classList.remove('active');callBtnText.textContent='Start Call';ws.send(JSON.stringify({type:'call_end'}))}};
setInterval(updateMetrics,5000);
connect()
</script></body></html>)rawliteral";

const char* setupHTML = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>HeySalad Setup</title>
<style>@import url('https://fonts.googleapis.com/css2?family=Figtree:wght@400;600&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Figtree',sans-serif;background:#ffd0cd;padding:20px;display:flex;align-items:center;justify-content:center;min-height:100vh}.container{background:white;padding:40px;border-radius:20px;max-width:500px;width:100%;box-shadow:0 10px 40px rgba(0,0,0,0.2)}h1{color:#ed4c4c;text-align:center;margin-bottom:30px}.form-group{margin-bottom:20px}label{display:block;margin-bottom:8px;font-weight:600}input{width:100%;padding:12px;border:2px solid #ffd0cd;border-radius:10px;font-size:16px}button{width:100%;padding:15px;background:#ed4c4c;color:white;border:none;border-radius:10px;cursor:pointer;font-size:16px;font-weight:600}</style></head><body>
<div class="container"><h1>🍅 HeySalad Setup</h1><form id="form"><div class="form-group"><label>WiFi SSID</label><input type="text" id="ssid" required></div><div class="form-group"><label>WiFi Password</label><input type="password" id="password" required></div><button type="submit">Save & Restart</button></form></div>
<script>document.getElementById('form').onsubmit=async(e)=>{e.preventDefault();await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:document.getElementById('ssid').value,password:document.getElementById('password').value})});alert('Saved! Restarting...');setTimeout(()=>window.location.reload(),2000)}</script></body></html>)rawliteral";

// WebSocket client sessions
std::map<uint32_t, String> wsClientTokens;

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        JsonDocument doc;
        deserializeJson(doc, data);
        String msgType = doc["type"].as<String>();
        
        // Authentication
        if (msgType == "auth") {
            String token = doc["token"].as<String>();
            if (auth.verifySession(token)) {
                wsClientTokens[client->id()] = token;
                Serial.printf("[WS] Client %u authenticated\n", client->id());
            } else {
                client->text("{\"type\":\"error\",\"message\":\"Invalid session\"}");
                client->close();
            }
            return;
        }
        
        // Check if client is authenticated
        if (wsClientTokens.find(client->id()) == wsClientTokens.end()) {
            client->text("{\"type\":\"error\",\"message\":\"Not authenticated\"}");
            return;
        }
        
        if (msgType == "call_start") {
            String provider = doc["provider"].as<String>();
            String mode = doc["mode"].as<String>();
            digitalWrite(LED_GPIO_NUM, HIGH);
            Serial.printf("[Call] Started - Provider: %s, Mode: %s\n", provider.c_str(), mode.c_str());
            
            client->text("{\"type\":\"call_status\",\"text\":\"Connecting to " + provider + "...\",\"calling\":true}");
            
            if (provider == "openai" || provider == "elevenlabs" || provider == "gemini" || provider == "deepseek") {
                client->text("{\"type\":\"call_status\",\"text\":\"Model API keys are not stored on this camera. Voice runs through Sally.\",\"calling\":false}");
                digitalWrite(LED_GPIO_NUM, LOW);
            }
            
        } else if (msgType == "call_end") {
            digitalWrite(LED_GPIO_NUM, LOW);
            Serial.println("[Call] Ended");
            
            if (openaiClient.isActive()) {
                openaiClient.stopRecording();
                delay(500);
                openaiClient.disconnect();
            }
            
            client->text("{\"type\":\"call_status\",\"text\":\"Call ended\",\"calling\":false}");
            delay(1000);
            client->text("{\"type\":\"call_status\",\"text\":\"Select options and tap to start\",\"calling\":false}");
        }
    } else if (type == WS_EVT_DISCONNECT) {
        wsClientTokens.erase(client->id());
        Serial.printf("[WS] Client %u disconnected\n", client->id());
    }
}

static std::string presentedLanToken(AsyncWebServerRequest* request) {
    String presented;
    if (request->hasHeader("Authorization")) {
        presented = request->header("Authorization");
        if (presented.startsWith("Bearer ")) presented = presented.substring(7);
        presented.trim();
    } else if (request->hasParam("token")) {
        presented = request->getParam("token")->value();
        presented.trim();
    }
    return std::string(presented.c_str());
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n=== HeySalad Starting ===");
    Serial.printf("Firmware: %s\n", SallySpatialProtocol::FIRMWARE_VERSION);
    
    pinMode(LED_GPIO_NUM, OUTPUT);
    pinMode(IR_LED_PIN, OUTPUT);
    setIRLED(false);
    
    preferences.begin("heysalad", false);
    const char* retiredKeys[] = {"openai", "elevenlabs", "gemini", "deepseek"};
    bool removedModelKeys = false;
    for (const char* name : retiredKeys) {
        if (preferences.isKey(name)) {
            preferences.remove(name);
            removedModelKeys = true;
        }
    }
    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    adminToken = preferences.getString("adminToken", "");
    isConfigured = preferences.getBool("configured", false);
    preferences.end();
    if (removedModelKeys) {
        Serial.println("Removed stored model API keys. Inference runs server-side via sally-api.");
    }
    
    if (wifiSSID.isEmpty() && strlen(FALLBACK_WIFI_SSID) > 0) {
        // Provisioning over the portal or BLE still wins; this only saves the
        // AP dance on a device that has never been set up.
        wifiSSID = FALLBACK_WIFI_SSID;
        wifiPassword = FALLBACK_WIFI_PASSWORD;
        preferences.begin("heysalad", false);
        preferences.putString("ssid", wifiSSID);
        preferences.putString("password", wifiPassword);
        preferences.putBool("bleProvLocked", true);
        preferences.end();
        Serial.println("WiFi: using built-in fallback credentials");
    }

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
    
    cameraMutex = xSemaphoreCreateMutex();
    cameraOK = initCamera();
    audioOK = initAudio();
    if (audioOK) {
        playBeep(880, 180, 3);
    }
    spatialClient.begin(preferences, cameraMutex, cameraOK, audioOK, audioOK);
    cloudProvisioned = spatialClient.isConfigured();
    const std::string mdnsName = SallyPolicy::mdnsHostname(
        std::string(spatialClient.alias().c_str()), std::string(spatialClient.deviceId().c_str()));
    Serial.printf("mDNS: %s.local\n", mdnsName.c_str());
    if (WiFi.status() == WL_CONNECTED) {
        MDNS.begin(mdnsName.c_str());
        MDNS.addService("http", "tcp", 80);
        ArduinoOTA.setHostname(mdnsName.c_str());
        ArduinoOTA.setPassword(CAM_OTA_PASSWORD);
        ArduinoOTA.begin();
        otaReady = true;
        Serial.printf("OTA: %s.local\n", mdnsName.c_str());
    }
    bleProvisioner.begin(preferences, spatialClient.deviceId(), spatialClient.alias(),
                         cameraOK, audioOK, audioOK);
    
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", isConfigured ? mainHTML : setupHTML);
    });
    
    server.on("/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "image/svg+xml", logoSVG);
    });
    
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest *request) {
        const SallyPolicy::LanAdminDecision decision = SallyPolicy::lanAdminAccess(
            cloudProvisioned, std::string(adminToken.c_str()), presentedLanToken(request));
        if (decision == SallyPolicy::LanAdminDecision::Disabled) {
            request->send(404, "application/json", "{\"error\":\"disabled\"}");
            return;
        }
        if (decision != SallyPolicy::LanAdminDecision::Allow) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (!cameraMutex || xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            request->send(503, "text/plain", "Camera busy");
            return;
        }
        const bool useIR = request->hasParam("ir") &&
                           request->getParam("ir")->value() != "0";
        if (useIR) {
            setIRLED(true);
            // Auto-exposure is still metered for darkness for a frame or two
            // after the LED comes on, so drop those rather than return one.
            delay(120);
            for (int i = 0; i < 2; i++) {
                camera_fb_t *stale = esp_camera_fb_get();
                if (stale) esp_camera_fb_return(stale);
            }
        }
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            if (useIR) setIRLED(false);
            xSemaphoreGive(cameraMutex);
            request->send(500, "text/plain", "Camera failed");
            return;
        }
        // The response only REFERENCES this buffer and send() is asynchronous, so
        // handing back the frame buffer here let the driver reuse it mid-transmit
        // and every JPEG arrived truncated. Copy into PSRAM, release the camera
        // immediately so it is not held for the length of a transfer, and free
        // the copy once the client has actually gone.
        const size_t len = fb->len;
        uint8_t *copy = (uint8_t *)ps_malloc(len);
        if (copy) memcpy(copy, fb->buf, len);
        esp_camera_fb_return(fb);
        if (useIR) setIRLED(false);
        xSemaphoreGive(cameraMutex);

        if (!copy) {
            request->send(500, "text/plain", "Out of memory");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse_P(
            200, "image/jpeg", copy, len
        );
        request->onDisconnect([copy]() { free(copy); });
        request->send(response);
    });

    // Lets the IR illuminator be checked on its own, without taking a picture.
    server.on("/ir", HTTP_GET, [](AsyncWebServerRequest *request) {
        const SallyPolicy::LanAdminDecision decision = SallyPolicy::lanAdminAccess(
            cloudProvisioned, std::string(adminToken.c_str()), presentedLanToken(request));
        if (decision == SallyPolicy::LanAdminDecision::Disabled) {
            request->send(404, "application/json", "{\"error\":\"disabled\"}");
            return;
        }
        if (decision != SallyPolicy::LanAdminDecision::Allow) {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        const bool on = request->hasParam("on") &&
                        request->getParam("on")->value() != "0";
        setIRLED(on);
        request->send(200, "application/json", on ? "{\"ir\":true}" : "{\"ir\":false}");
    });
    
    server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"camera\":" + String(cameraOK ? "true" : "false") + ",";
        json += "\"audio\":" + String(audioOK ? "true" : "false") + ",";
        json += "\"psram\":\"" + String(ESP.getFreePsram() / 1024) + "K\",";
        json += "\"heap\":\"" + String(ESP.getFreeHeap() / 1024) + "K\",";
        json += "\"temp\":\"" + String(temperatureRead(), 0) + "°C\",";
        json += "\"wifi\":\"" + String(WiFi.RSSI()) + "dBm\"";
        json += "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (isConfigured || !wifiSSID.isEmpty()) {
                request->send(403, "application/json", "{\"ok\":false,\"error\":\"already_configured\"}");
                return;
            }
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
                return;
            }

            String alias = doc["sally_alias"] | "";
            String piBaseUrl = SallySpatialProtocol::trimTrailingSlashes(doc["pi_base_url"] | "");
            if (piBaseUrl.isEmpty()) piBaseUrl = SallySpatialProtocol::trimTrailingSlashes(doc["server_base_url"] | "");
            String deviceToken = doc["device_token"] | "";
            String portalAdmin = doc["admin_token"] | "";
            alias.trim();
            deviceToken.trim();
            portalAdmin.trim();
            const bool snapshotPresent = !doc["snapshot_interval_sec"].isNull();
            const long snapshotSec = snapshotPresent ? doc["snapshot_interval_sec"].as<long>() : 0;
            if ((!alias.isEmpty() && !SallySpatialProtocol::isValidAlias(alias)) ||
                (!piBaseUrl.isEmpty() && !piBaseUrl.startsWith("http://") && !piBaseUrl.startsWith("https://")) ||
                (!deviceToken.isEmpty() &&
                 !SallyPolicy::isDeviceToken(std::string(deviceToken.c_str()))) ||
                (!portalAdmin.isEmpty() && portalAdmin.length() < SallyPolicy::kMinAdminTokenLength) ||
                (snapshotPresent && (snapshotSec < 0 ||
                 !SallyPolicy::snapshotIntervalAllowed(static_cast<uint32_t>(snapshotSec))))) {
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_spatial_configuration\"}");
                return;
            }
            
            preferences.begin("heysalad", false);
            preferences.putString("ssid", doc["ssid"].as<String>());
            preferences.putString("password", doc["password"].as<String>());
            if (!alias.isEmpty()) preferences.putString("sallyAlias", alias);
            if (!piBaseUrl.isEmpty()) preferences.putString("piBaseUrl", piBaseUrl);
            if (!deviceToken.isEmpty()) preferences.putString("deviceToken", deviceToken);
            if (!portalAdmin.isEmpty()) preferences.putString("adminToken", portalAdmin);
            if (snapshotPresent) preferences.putUInt("snapshotSec", static_cast<uint32_t>(snapshotSec));
            preferences.putBool("configured", true);
            preferences.putBool("bleProvLocked", true);
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
    if (otaReady) ArduinoOTA.handle();
    ws.cleanupClients();
    spatialClient.loop();
    bleProvisioner.loop();
    delay(10);
}
