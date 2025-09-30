#ifndef AUTH_H
#define AUTH_H

#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <map>

class AuthManager {
private:
    Preferences prefs;
    std::map<String, unsigned long> activeSessions;
    const unsigned long SESSION_TIMEOUT = 86400000; // 24 hours
    
    // SHA256 hash function
    String hashPassword(String password, String salt) {
        String input = password + salt;
        byte hash[32];
        
        mbedtls_md_context_t ctx;
        mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
        
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
        mbedtls_md_finish(&ctx, hash);
        mbedtls_md_free(&ctx);
        
        // Convert to hex string
        String hashStr;
        for (int i = 0; i < 32; i++) {
            char hex[3];
            sprintf(hex, "%02x", hash[i]);
            hashStr += hex;
        }
        
        return hashStr;
    }
    
    // Generate random salt
    String generateSalt() {
        String salt;
        for (int i = 0; i < 16; i++) {
            salt += String(random(0, 256), HEX);
        }
        return salt;
    }
    
    // Generate session token
    String generateSessionToken() {
        String token;
        for (int i = 0; i < 32; i++) {
            token += String(random(0, 256), HEX);
        }
        return token;
    }

public:
    AuthManager() {}
    
    // Initialize - create default admin account if none exists
    void begin() {
        prefs.begin("auth", false);
        
        if (!prefs.isKey("password_hash")) {
            // First time setup - create default password "admin123"
            String salt = generateSalt();
            String hash = hashPassword("admin123", salt);
            
            prefs.putString("password_hash", hash);
            prefs.putString("salt", salt);
            prefs.putBool("setup_complete", false);
            
            Serial.println("[Auth] Default credentials created");
            Serial.println("       Username: admin");
            Serial.println("       Password: admin123");
            Serial.println("       PLEASE CHANGE ON FIRST LOGIN!");
        }
        
        prefs.end();
    }
    
    // Verify login credentials
    String login(String password) {
        prefs.begin("auth", true);
        String storedHash = prefs.getString("password_hash", "");
        String salt = prefs.getString("salt", "");
        prefs.end();
        
        String inputHash = hashPassword(password, salt);
        
        if (inputHash == storedHash) {
            // Generate session token
            String token = generateSessionToken();
            activeSessions[token] = millis();
            
            Serial.println("[Auth] Login successful");
            return token;
        }
        
        Serial.println("[Auth] Login failed");
        return "";
    }
    
    // Verify session token
    bool verifySession(String token) {
        if (token.isEmpty()) return false;
        
        auto it = activeSessions.find(token);
        if (it == activeSessions.end()) return false;
        
        // Check if session expired
        if (millis() - it->second > SESSION_TIMEOUT) {
            activeSessions.erase(it);
            return false;
        }
        
        // Refresh session
        it->second = millis();
        return true;
    }
    
    // Logout
    void logout(String token) {
        activeSessions.erase(token);
        Serial.println("[Auth] Logout successful");
    }
    
    // Change password
    bool changePassword(String oldPassword, String newPassword) {
        prefs.begin("auth", true);
        String storedHash = prefs.getString("password_hash", "");
        String salt = prefs.getString("salt", "");
        prefs.end();
        
        // Verify old password
        String oldHash = hashPassword(oldPassword, salt);
        if (oldHash != storedHash) {
            Serial.println("[Auth] Old password incorrect");
            return false;
        }
        
        // Set new password
        String newSalt = generateSalt();
        String newHash = hashPassword(newPassword, newSalt);
        
        prefs.begin("auth", false);
        prefs.putString("password_hash", newHash);
        prefs.putString("salt", newSalt);
        prefs.putBool("setup_complete", true);
        prefs.end();
        
        // Invalidate all existing sessions
        activeSessions.clear();
        
        Serial.println("[Auth] Password changed successfully");
        return true;
    }
    
    // Check if initial setup is complete
    bool isSetupComplete() {
        prefs.begin("auth", true);
        bool complete = prefs.getBool("setup_complete", false);
        prefs.end();
        return complete;
    }
    
    // Get active session count
    int getActiveSessionCount() {
        // Clean expired sessions
        unsigned long now = millis();
        for (auto it = activeSessions.begin(); it != activeSessions.end();) {
            if (now - it->second > SESSION_TIMEOUT) {
                it = activeSessions.erase(it);
            } else {
                ++it;
            }
        }
        return activeSessions.size();
    }
};

#endif