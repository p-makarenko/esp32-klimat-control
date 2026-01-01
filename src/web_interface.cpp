#include "web_interface.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "learning_system.h"
#include <ArduinoJson.h>
#include "advanced_climate_logic.h"
#include "data_storage.h"

extern int historyIndex;
extern bool historyInitialized;

// ============================================================================
// РРќРР¦РРђР›РР—РђР¦РРЇ WI-FI Р Р’Р•Р‘-РЎР•Р Р’Р•Р Рђ
// ============================================================================

void initWiFi() {
    Serial.println("\n=== РќРђРЎРўР РћР™РљРђ WI-FI ===");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);  // РџРѕР»РЅРѕРµ РѕС‚РєР»СЋС‡РµРЅРёРµ
    delay(100);
    
    // РЎРїРёСЃРѕРє СЃРµС‚РµР№ РґР»СЏ РїРѕРґРєР»СЋС‡РµРЅРёСЏ
    struct WiFiNetwork {
        const char* ssid;
        const char* password;
    };
    
    WiFiNetwork networks[] = {
        {"Redmi Note 14", "12345678"},     // РћСЃРЅРѕРІРЅР°СЏ СЃРµС‚СЊ
        {"Redmi Note 9", "1234567890"},    // Р РµР·РµСЂРІРЅР°СЏ СЃРµС‚СЊ
    };
    
    // РЎРЅР°С‡Р°Р»Р° СЃРєР°РЅРёСЂСѓРµРј СЃРµС‚Рё
    Serial.println("вЏі РџРѕРёСЃРє РґРѕСЃС‚СѓРїРЅС‹С… СЃРµС‚РµР№...");
    
    int n = WiFi.scanNetworks();
    Serial.printf("РќР°Р№РґРµРЅРѕ %d СЃРµС‚РµР№:\n", n);
    
    for (int i = 0; i < n; i++) {
        Serial.printf("  %d: %s (%d dBm) %s\n", 
            i + 1, 
            WiFi.SSID(i).c_str(), 
            WiFi.RSSI(i),
            (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "РѕС‚РєСЂС‹С‚Р°СЏ" : "Р·Р°С‰РёС‰РµРЅРЅР°СЏ");
    }
    
    bool connected = false;
    String connectedSSID = "";
    
    // РџСЂРѕР±СѓРµРј РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє СЃРѕС…СЂР°РЅРµРЅРЅС‹Рј РЅР°СЃС‚СЂРѕР№РєР°Рј
    preferences.begin("wifi", true);
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    preferences.end();
    
    if (savedSSID.length() > 0 && savedPassword.length() > 0) {
        // РџСЂРѕРІРµСЂСЏРµРј, РµСЃС‚СЊ Р»Рё СЃРѕС…СЂР°РЅРµРЅРЅР°СЏ СЃРµС‚СЊ РІ СЃРїРёСЃРєРµ РґРѕСЃС‚СѓРїРЅС‹С…
        bool savedNetworkAvailable = false;
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == savedSSID) {
                savedNetworkAvailable = true;
                Serial.printf("\nрџ”Ќ РЎРѕС…СЂР°РЅРµРЅРЅР°СЏ СЃРµС‚СЊ РЅР°Р№РґРµРЅР°: %s (СЃРёРіРЅР°Р»: %d dBm)\n", 
                    savedSSID.c_str(), WiFi.RSSI(i));
                break;
            }
        }
        
        if (savedNetworkAvailable) {
            Serial.printf("РџСЂРѕР±СѓСЋ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє СЃРѕС…СЂР°РЅРµРЅРЅРѕР№ СЃРµС‚Рё: %s\n", savedSSID.c_str());
            
            WiFi.disconnect(true);
            delay(100);
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
            
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                Serial.print(".");
                attempts++;
                
                if (attempts % 5 == 0) {
                    Serial.printf(" [СЃС‚Р°С‚СѓСЃ: %d]", WiFi.status());
                }
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                connectedSSID = savedSSID;
                Serial.printf("\nвњ“ РЈСЃРїРµС€РЅРѕ РїРѕРґРєР»СЋС‡РµРЅРѕ Рє СЃРѕС…СЂР°РЅРµРЅРЅРѕР№ СЃРµС‚Рё: %s\n", savedSSID.c_str());
            } else {
                Serial.printf("\nвќЊ РќРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє СЃРѕС…СЂР°РЅРµРЅРЅРѕР№ СЃРµС‚Рё: %s\n", savedSSID.c_str());
                WiFi.disconnect(true);
                delay(500);
            }
        } else {
            Serial.printf("РЎРѕС…СЂР°РЅРµРЅРЅР°СЏ СЃРµС‚СЊ %s РЅРµ РЅР°Р№РґРµРЅР°\n", savedSSID.c_str());
        }
    }
    
    // Р•СЃР»Рё РЅРµ РїРѕРґРєР»СЋС‡РёР»РёСЃСЊ Рє СЃРѕС…СЂР°РЅРµРЅРЅРѕР№, РїСЂРѕР±СѓРµРј РёР·РІРµСЃС‚РЅС‹Рµ СЃРµС‚Рё
    if (!connected) {
        for (int i = 0; i < sizeof(networks)/sizeof(networks[0]); i++) {
            // РџСЂРѕРІРµСЂСЏРµРј, РµСЃС‚СЊ Р»Рё СЃРµС‚СЊ РІ СЃРїРёСЃРєРµ РґРѕСЃС‚СѓРїРЅС‹С…
            bool networkFound = false;
            int rssi = 0;
            
            for (int j = 0; j < n; j++) {
                if (WiFi.SSID(j) == networks[i].ssid) {
                    networkFound = true;
                    rssi = WiFi.RSSI(j);
                    break;
                }
            }
            
            if (networkFound && networks[i].password != NULL && strlen(networks[i].password) > 0) {
                Serial.printf("\nрџ”Ќ РќР°Р№РґРµРЅР° СЃРµС‚СЊ: %s (СЃРёРіРЅР°Р»: %d dBm)\n", 
                    networks[i].ssid, rssi);
                Serial.printf("РџСЂРѕР±СѓСЋ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє: %s\n", networks[i].ssid);
                
                WiFi.disconnect(true);
                delay(100);
                WiFi.begin(networks[i].ssid, networks[i].password);
                
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    delay(500);
                    Serial.print(".");
                    attempts++;
                    
                    if (attempts % 5 == 0) {
                        Serial.printf(" [СЃС‚Р°С‚СѓСЃ: %d]", WiFi.status());
                    }
                }
                
                if (WiFi.status() == WL_CONNECTED) {
                    connected = true;
                    connectedSSID = networks[i].ssid;
                    Serial.printf("\nвњ“ РЈСЃРїРµС€РЅРѕ РїРѕРґРєР»СЋС‡РµРЅРѕ Рє: %s\n", networks[i].ssid);
                    
                    // РЎРѕС…СЂР°РЅСЏРµРј СѓСЃРїРµС€РЅС‹Рµ РЅР°СЃС‚СЂРѕР№РєРё
                    preferences.begin("wifi", false);
                    preferences.putString("ssid", networks[i].ssid);
                    preferences.putString("password", networks[i].password);
                    preferences.end();
                    
                    Serial.printf("РќР°СЃС‚СЂРѕР№РєРё СЃРѕС…СЂР°РЅРµРЅС‹ РґР»СЏ СЃРµС‚Рё: %s\n", networks[i].ssid);
                    break;
                } else {
                    Serial.printf("\nвќЊ РќРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє: %s\n", networks[i].ssid);
                    WiFi.disconnect(true);
                    delay(500);
                }
            } else if (networkFound) {
                Serial.printf("РЎРµС‚СЊ %s РЅР°Р№РґРµРЅР°, РЅРѕ РїР°СЂРѕР»СЊ РЅРµ СѓРєР°Р·Р°РЅ\n", networks[i].ssid);
            } else {
                Serial.printf("РЎРµС‚СЊ %s РЅРµ РЅР°Р№РґРµРЅР°\n", networks[i].ssid);
            }
        }
    }
    
    // Р•СЃР»Рё РїРѕРґРєР»СЋС‡РёР»РёСЃСЊ - РїРѕРєР°Р·С‹РІР°РµРј РёРЅС„РѕСЂРјР°С†РёСЋ
    if (connected) {
        Serial.print("вњ“ Wi-Fi РїРѕРґРєР»СЋС‡РµРЅРѕ! ");
        Serial.print("SSID: ");
        Serial.print(connectedSSID);
        Serial.print(" | IP Р°РґСЂРµСЃ: ");
        Serial.print(WiFi.localIP());
        Serial.print(" | РЎРёРіРЅР°Р»: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        
        // РџСЂРѕРІРµСЂСЏРµРј, РїСЂР°РІРёР»СЊРЅС‹Р№ Р»Рё РїР°СЂРѕР»СЊ (РїРѕ СЃРёР»Рµ СЃРёРіРЅР°Р»Р°)
        if (WiFi.RSSI() < -80) {
            Serial.println("вљ  РЎР»Р°Р±С‹Р№ СЃРёРіРЅР°Р» Wi-Fi!");
        }
    } else {
        // Р•СЃР»Рё РЅРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ РЅРё Рє РѕРґРЅРѕР№ СЃРµС‚Рё - Р·Р°РїСѓСЃРєР°РµРј С‚РѕС‡РєСѓ РґРѕСЃС‚СѓРїР°
        Serial.println("\nвљ  РќРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ РЅРё Рє РѕРґРЅРѕР№ РёР·РІРµСЃС‚РЅРѕР№ СЃРµС‚Рё");
        Serial.println("Р—Р°РїСѓСЃРєР°СЋ С‚РѕС‡РєСѓ РґРѕСЃС‚СѓРїР°...");
        
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ClimateControl", "12345678");
        
        Serial.print("вњ… РўРѕС‡РєР° РґРѕСЃС‚СѓРїР° Р·Р°РїСѓС‰РµРЅР°. IP: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("   SSID: ClimateControl");
        Serial.println("   РџР°СЂРѕР»СЊ: 12345678");
    }
    
    // РўРµСЃС‚РёСЂСѓРµРј РїРѕРґРєР»СЋС‡РµРЅРёРµ СЃ РїРѕРјРѕС‰СЊСЋ РїСЂРѕСЃС‚РѕР№ РїСЂРѕРІРµСЂРєРё
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("РџСЂРѕРІРµСЂРєР° РїРѕРґРєР»СЋС‡РµРЅРёСЏ... ");
        
        // РџСЂРѕСЃС‚Р°СЏ РїСЂРѕРІРµСЂРєР° - РµСЃР»Рё Сѓ РЅР°СЃ РµСЃС‚СЊ IP Р°РґСЂРµСЃ
        if (WiFi.localIP() != IPAddress(0,0,0,0)) {
            Serial.println("вњ“ Р›РѕРєР°Р»СЊРЅР°СЏ СЃРµС‚СЊ РґРѕСЃС‚СѓРїРЅР°");
            // Р”РѕРїРѕР»РЅРёС‚РµР»СЊРЅР°СЏ РїСЂРѕРІРµСЂРєР° NTP
            configTime(0, 0, "pool.ntp.org");
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 5000)) {
                Serial.println("вњ“ РРЅС‚РµСЂРЅРµС‚ РґРѕСЃС‚СѓРїРµРЅ (NTP СЃРёРЅС…СЂРѕРЅРёР·РёСЂРѕРІР°РЅ)");
            } else {
                Serial.println("вљ  РўРѕР»СЊРєРѕ Р»РѕРєР°Р»СЊРЅР°СЏ СЃРµС‚СЊ (РЅРµС‚ РґРѕСЃС‚СѓРїР° Рє РёРЅС‚РµСЂРЅРµС‚Сѓ)");
            }
        } else {
            Serial.println("вљ  РќРµС‚ СЃРµС‚РµРІРѕРіРѕ РїРѕРґРєР»СЋС‡РµРЅРёСЏ");
        }
    }
    
    // Р Р•Р“РРЎРўР РђР¦РРЇ РњРђР РЁР РЈРўРћР’
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/control", HTTP_GET, handleControlPage);
    server.on("/command", HTTP_POST, handleWebCommand);
    server.on("/settings", HTTP_GET, handleSettingsPage);
    server.on("/settings", HTTP_POST, handleSaveSettings);
    server.on("/wifi-settings", HTTP_GET, handleWiFiSettingsPage);
    server.on("/save-wifi", HTTP_POST, handleSaveWiFiSettings);
    server.on("/scan-wifi", HTTP_GET, handleScanWiFi);
    server.on("/learning", HTTP_GET, handleLearningPage);
    server.on("/learning/api", HTTP_POST, handleLearningAPI);
    server.on("/time", HTTP_GET, handleTimePage);
    server.on("/wifi", HTTP_GET, handleWiFiPage);
    server.on("/history", HTTP_GET, handleHistoryPage);
    server.on("/debug", HTTP_GET, handleDebugPage);
    
    server.onNotFound([]() {
        server.send(404, "text/plain", "РЎС‚СЂР°РЅРёС†Р° РЅРµ РЅР°Р№РґРµРЅР°");
    });
    
    server.begin();
    Serial.println("вњ“ Р’РµР±-СЃРµСЂРІРµСЂ Р·Р°РїСѓС‰РµРЅ");
    Serial.println();
}

// ============================================================================
// Р’РЎРџРћРњРћР“РђРўР•Р›Р¬РќР«Р• Р¤РЈРќРљР¦РР Р”Р›РЇ WI-FI
// ============================================================================

String wifiStrengthToHTML(int rssi) {
    String strength;
    String color;
    
    if (rssi >= -50) {
        strength = "РћС‚Р»РёС‡РЅС‹Р№";
        color = "#00C851"; // Р·РµР»РµРЅС‹Р№
    } else if (rssi >= -60) {
        strength = "РҐРѕСЂРѕС€РёР№";
        color = "#33b5e5"; // РіРѕР»СѓР±РѕР№
    } else if (rssi >= -70) {
        strength = "РЎСЂРµРґРЅРёР№";
        color = "#ffbb33"; // Р¶РµР»С‚С‹Р№
    } else if (rssi >= -80) {
        strength = "РЎР»Р°Р±С‹Р№";
        color = "#ff4444"; // РєСЂР°СЃРЅС‹Р№
    } else {
        strength = "РћС‡РµРЅСЊ СЃР»Р°Р±С‹Р№";
        color = "#cc0000"; // С‚РµРјРЅРѕ-РєСЂР°СЃРЅС‹Р№
    }
    
    return "<span style='color:" + color + "; font-weight:bold;'>" + strength + " (" + String(rssi) + " dBm)</span>";
}

String encryptionTypeToString(wifi_auth_mode_t type) {
    switch(type) {
        case WIFI_AUTH_OPEN: return "РћС‚РєСЂС‹С‚Р°СЏ";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "РќРµРёР·РІРµСЃС‚РЅРѕ";
    }
}

// ============================================================================
// Р“Р›РђР’РќРђРЇ РЎРўР РђРќРР¦Рђ (РЎ РљРћРњРђРќР”РќРћР™ РЎРўР РћРљРћР™ Р РРќР”РРљРђР¦РР•Р™ A,B,C)
// ============================================================================

void handleRoot() {
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>РљР»РёРјР°С‚-РєРѕРЅС‚СЂРѕР»СЊ</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    html += ".header { text-align: center; margin-bottom: 30px; }";
    html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin: 30px 0; }";
    html += ".card { background: #f9f9f9; padding: 20px; border-radius: 8px; border-left: 4px solid #4CAF50; }";
    
    html += ".power-indicators { display: flex; justify-content: space-between; margin: 20px 0; background: #e8f5e9; padding: 15px; border-radius: 8px; }";
    html += ".power-item { text-align: center; flex: 1; padding: 10px; }";
    html += ".power-label { font-size: 0.9em; color: #666; margin-bottom: 5px; }";
    html += ".power-value { font-size: 1.8em; font-weight: bold; color: #2c3e50; }";
    
    html += ".command-section { background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0; }";
    html += "#commandOutput { background: white; padding: 10px; border-radius: 5px; font-family: monospace; min-height: 50px; white-space: pre-wrap; overflow-y: auto; max-height: 200px; }";
    html += ".quick-buttons { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 10px; }";
    html += ".quick-btn { background: #e0e0e0; padding: 8px 12px; border-radius: 4px; cursor: pointer; border: none; transition: background 0.2s; }";
    html += ".quick-btn:hover { background: #bdbdbd; }";
    
    html += ".status-value { font-size: 1.2em; font-weight: bold; }";
    html += ".temp-status { color: #e74c3c; }";
    html += ".hum-status { color: #3498db; }";
    html += ".sys-status { color: #2c3e50; }";
    
    html += ".nav { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 10px; margin: 30px 0; }";
    html += ".nav-btn { background: #4CAF50; color: white; padding: 15px; text-align: center; text-decoration: none; border-radius: 5px; display: block; transition: background 0.3s; }";
    html += ".nav-btn:hover { background: #45a049; }";
    html += ".btn { background: #2196F3; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; transition: background 0.2s; }";
    html += ".btn:hover { background: #1976D2; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>рџЊЎ РЎРРЎРўР•РњРђ РљР›РРњРђРў-РљРћРќРўР РћР›РЇ v4.0</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + " | " + getTimeString() + "</p>";
    html += "</div>";
    
    html += "<div class='power-indicators'>";
    html += "<div class='power-item'>";
    html += "<div class='power-label'>РќРђРЎРћРЎ (A)</div>";
    html += "<div class='power-value' id='pumpPower'>" + String((heatingState.pumpPower * 100) / 255) + "%</div>";
    html += "<div style='font-size: 0.8em; color: #666;'>PWM: " + String(heatingState.pumpPower) + "</div>";
    html += "</div>";
    
    html += "<div class='power-item'>";
    html += "<div class='power-label'>Р’Р•РќРўРР›РЇРўРћР  (B)</div>";
    html += "<div class='power-value' id='fanPower'>" + String((heatingState.fanPower * 100) / 255) + "%</div>";
    html += "<div style='font-size: 0.8em; color: #666;'>PWM: " + String(heatingState.fanPower) + "</div>";
    html += "</div>";
    
    html += "<div class='power-item'>";
    html += "<div class='power-label'>Р’Р«РўРЇР–РљРђ (C)</div>";
    html += "<div class='power-value' id='extractorPower'>" + String((heatingState.extractorPower * 100) / 255) + "%</div>";
    html += "<div style='font-size: 0.8em; color: #666;'>PWM: " + String(heatingState.extractorPower) + "</div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='status-grid'>";
    html += "<div class='card'>";
    html += "<h3>рџЊЎ РўРµРјРїРµСЂР°С‚СѓСЂР°</h3>";
    html += "<div class='status-value temp-status' id='tempRoom'>" + String(sensorData.tempRoom, 1) + "В°C</div>";
    html += "<div>РўРµРїР»РѕРЅРѕСЃРёС‚РµР»СЊ: <span id='tempCarrier'>" + String(sensorData.tempCarrier, 1) + "В°C</span></div>";
    html += "<div>Р¦РµР»СЊ: " + String(config.tempMin, 1) + "-" + String(config.tempMax, 1) + "В°C</div>";
    html += "<div>РўСЂРµРЅРґ: <span id='tempTrend'>--</span></div>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>рџ’§ Р’Р»Р°Р¶РЅРѕСЃС‚СЊ</h3>";
    html += "<div class='status-value hum-status' id='humidity'>" + String(sensorData.humidity, 1) + "%</div>";
    html += "<div>Р¦РµР»СЊ: " + String(config.humidityConfig.minHumidity, 1) + "-" + String(config.humidityConfig.maxHumidity, 1) + "%</div>";
    html += "<div>РЈРІР»Р°Р¶РЅРёС‚РµР»СЊ: <span id='humidifierStatus'>" + String(humidifierState.active ? "Р’РљР›" : "Р’Р«РљР›") + "</span></div>";
    html += "<div>Р”Р°РІР»РµРЅРёРµ: <span id='pressure'>" + String(sensorData.pressure, 1) + " hPa</span></div>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>вљЎ РЎРёСЃС‚РµРјР°</h3>";
    html += "<div>Р РµР¶РёРј: <span class='sys-status' id='mode'>";
    html += heatingState.manualMode ? "Р СѓС‡РЅРѕР№" : (heatingState.forceMode ? "Р¤РѕСЂСЃР°Р¶" : "РђРІС‚Рѕ");
    html += "</span></div>";
    html += "<div>Wi-Fi: " + WiFi.SSID() + " (" + String(WiFi.RSSI()) + " dBm)</div>";
    html += "<div>РџР°РјСЏС‚СЊ: <span id='memory'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
    html += "<div>Р’СЂРµРјСЏ СЂР°Р±РѕС‚С‹: <span id='uptime'>" + String(millis() / 1000) + " СЃРµРє</span></div>";
    html += "<div>Р—Р°РїРёСЃРµР№: <span id='historyCount'>" + String(historyIndex) + "</span></div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='command-section'>";
    html += "<h3>рџ’¬ РљРѕРјР°РЅРґРЅР°СЏ СЃС‚СЂРѕРєР° (Р°РЅР°Р»РѕРіРёС‡РЅРѕ Serial Monitor)</h3>";
    html += "<div style='display: flex; gap: 10px; margin-bottom: 15px;'>";
    html += "<input type='text' id='commandInput' placeholder='Р’РІРµРґРёС‚Рµ РєРѕРјР°РЅРґСѓ (pump 50, fan 40, timer 30, status...)' style='flex: 1; padding: 10px; border: 1px solid #ccc; border-radius: 4px;'>";
    html += "<button class='btn' onclick='executeCommand()'>Р’С‹РїРѕР»РЅРёС‚СЊ</button>";
    html += "<button class='btn' onclick='clearOutput()' style='background: #f44336;'>РћС‡РёСЃС‚РёС‚СЊ</button>";
    html += "</div>";
    
    html += "<div class='quick-buttons'>";
    html += "<button class='quick-btn' onclick=\"quickCommand('pump 30')\">РќР°СЃРѕСЃ 30%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('fan 40')\">Р’РµРЅС‚РёР»СЏС‚РѕСЂ 40%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('extractor 50')\">Р’С‹С‚СЏР¶РєР° 50%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('extractor 0')\">Р’С‹С‚СЏР¶РєР° Р’Р«РљР›</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('timer on 30')\">РўР°Р№РјРµСЂ 30РјРёРЅ</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('timer off')\">РўР°Р№РјРµСЂ Р’Р«РљР›</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('auto')\">РђРІС‚Рѕ</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('manual')\">Р СѓС‡РЅРѕР№</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('status')\">РЎС‚Р°С‚СѓСЃ</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('save')\">РЎРѕС…СЂР°РЅРёС‚СЊ</button>";
    html += "</div>";
    
    html += "<div id='commandOutput' style='margin-top: 15px;'>> Р“РѕС‚РѕРІ Рє РєРѕРјР°РЅРґР°Рј...</div>";
    html += "</div>";
    
    html += "<div class='nav'>";
    html += "<a href='/control' class='nav-btn'>рџЋ›пёЏ РЈРїСЂР°РІР»РµРЅРёРµ</a>";
    html += "<a href='/settings' class='nav-btn'>вљ™пёЏ РќР°СЃС‚СЂРѕР№РєРё</a>";
    html += "<a href='/wifi-settings' class='nav-btn'>рџ“¶ Wi-Fi</a>";
    html += "<a href='/learning' class='nav-btn'>рџ§  РћР±СѓС‡РµРЅРёРµ</a>";
    html += "<a href='/status' class='nav-btn'>рџ“Љ JSON СЃС‚Р°С‚СѓСЃ</a>";
    html += "<a href='/time' class='nav-btn'>рџ•ђ Р’СЂРµРјСЏ</a>";
    html += "<a href='/history' class='nav-btn'>рџ“€ РСЃС‚РѕСЂРёСЏ</a>";
    html += "<a href='/debug' class='nav-btn'>рџђћ РћС‚Р»Р°РґРєР°</a>";
    html += "</div>";
    
    html += "<div style='text-align: center; color: #777; margin-top: 20px; padding-top: 20px; border-top: 1px solid #eee;'>";
    html += "РћР±РЅРѕРІР»РµРЅРѕ: <span id='lastUpdate'>--:--:--</span>";
    html += " | РЎР»РµРґСѓСЋС‰РµРµ РѕР±РЅРѕРІР»РµРЅРёРµ С‡РµСЂРµР·: <span id='nextUpdate'>3 СЃРµРє</span>";
    html += "</div>";
    
    html += "</div>";
    
    html += "<script>";
    html += "let updateInterval = 3000;";
    html += "let lastUpdateTime = new Date();";
    
    html += "function updateStatus() {";
    html += "  fetch('/status')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (data.tempRoom !== undefined) {";
    html += "        document.getElementById('tempRoom').textContent = data.tempRoom.toFixed(1) + 'В°C';";
    html += "        document.getElementById('tempCarrier').textContent = data.tempCarrier.toFixed(1) + 'В°C';";
    html += "      }";
    html += "      if (data.humidity !== undefined) {";
    html += "        document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';";
    html += "      }";
    html += "      if (data.pressure !== undefined) {";
    html += "        document.getElementById('pressure').textContent = data.pressure.toFixed(1) + ' hPa';";
    html += "      }";
    html += "      if (data.pumpPower !== undefined) {";
    html += "        document.getElementById('pumpPower').textContent = Math.round(data.pumpPower) + '%';";
    html += "      }";
    html += "      if (data.fanPower !== undefined) {";
    html += "        document.getElementById('fanPower').textContent = Math.round(data.fanPower) + '%';";
    html += "      }";
    html += "      if (data.extractorPower !== undefined) {";
    html += "        document.getElementById('extractorPower').textContent = Math.round(data.extractorPower) + '%';";
    html += "      }";
    html += "      if (data.mode) {";
    html += "        document.getElementById('mode').textContent = data.mode;";
    html += "      }";
    html += "      if (data.memory) {";
    html += "        document.getElementById('memory').textContent = data.memory + ' KB';";
    html += "      }";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "      lastUpdateTime = new Date();";
    html += "    })";
    html += "    .catch(error => {";
    html += "      console.error('РћС€РёР±РєР° РѕР±РЅРѕРІР»РµРЅРёСЏ:', error);";
    html += "    });";
    html += "}";
    
    html += "function executeCommand() {";
    html += "  const input = document.getElementById('commandInput');";
    html += "  const command = input.value.trim();";
    html += "  if (!command) return;";
    html += "  const output = document.getElementById('commandOutput');";
    html += "  output.innerHTML += '\\n> ' + command + '\\n[Р’С‹РїРѕР»РЅСЏСЋ...]';";
    html += "  output.scrollTop = output.scrollHeight;";
    html += "  fetch('/command', {";
    html += "    method: 'POST',";
    html += "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },";
    html += "    body: 'cmd=' + encodeURIComponent(command)";
    html += "  })";
    html += "  .then(response => response.text())";
    html += "  .then(text => {";
    html += "    output.innerHTML = output.innerHTML.replace('[Р’С‹РїРѕР»РЅСЏСЋ...]', text);";
    html += "    output.scrollTop = output.scrollHeight;";
    html += "    input.value = '';";
    html += "    setTimeout(updateStatus, 1000);";
    html += "  })";
    html += "  .catch(error => {";
    html += "    output.innerHTML = output.innerHTML.replace('[Р’С‹РїРѕР»РЅСЏСЋ...]', 'вќЊ РћС€РёР±РєР°: ' + error);";
    html += "    output.scrollTop = output.scrollHeight;";
    html += "  });";
    html += "}";
    
    html += "function quickCommand(cmd) {";
    html += "  document.getElementById('commandInput').value = cmd;";
    html += "  executeCommand();";
    html += "}";
    
    html += "function clearOutput() {";
    html += "  document.getElementById('commandOutput').innerHTML = '> Р“РѕС‚РѕРІ Рє РєРѕРјР°РЅРґР°Рј...';";
    html += "}";
    
    html += "document.getElementById('commandInput').addEventListener('keypress', function(e) {";
    html += "  if (e.key === 'Enter') executeCommand();";
    html += "});";
    
    html += "function updateNextUpdateTimer() {";
    html += "  const now = new Date();";
    html += "  const timeSinceUpdate = now - lastUpdateTime;";
    html += "  const timeLeft = Math.max(0, updateInterval - timeSinceUpdate);";
    html += "  document.getElementById('nextUpdate').textContent = Math.round(timeLeft/1000) + ' СЃРµРє';";
    html += "}";
    
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  updateStatus();";
    html += "  setInterval(updateStatus, updateInterval);";
    html += "  setInterval(updateNextUpdateTimer, 1000);";
    html += "  let uptimeSeconds = " + String(millis() / 1000) + ";";
    html += "  setInterval(() => {";
    html += "    uptimeSeconds++;";
    html += "    const hours = Math.floor(uptimeSeconds / 3600);";
    html += "    const minutes = Math.floor((uptimeSeconds % 3600) / 60);";
    html += "    const seconds = uptimeSeconds % 60;";
    html += "    document.getElementById('uptime').textContent = ";
    html += "      hours.toString().padStart(2, '0') + ':' + ";
    html += "      minutes.toString().padStart(2, '0') + ':' + ";
    html += "      seconds.toString().padStart(2, '0');";
    html += "  }, 1000);";
    html += "});";
    html += "</script>";
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// JSON API Р”Р›РЇ РЎРўРђРўРЈРЎРђ
// ============================================================================

void handleStatus() {
    JsonDocument doc;
    
    doc["tempRoom"] = sensorData.tempRoom;
    doc["tempCarrier"] = sensorData.tempCarrier;
    doc["humidity"] = sensorData.humidity;
    doc["pressure"] = sensorData.pressure;
    doc["pumpPower"] = (heatingState.pumpPower * 100) / 255;
    doc["fanPower"] = (heatingState.fanPower * 100) / 255;
    doc["extractorPower"] = (heatingState.extractorPower * 100) / 255;
    doc["extractorTimer"] = config.extractorTimer.enabled;
    
    if (heatingState.emergencyMode) doc["mode"] = "РђРІР°СЂРёСЏ";
    else if (heatingState.forceMode) doc["mode"] = "Р¤РѕСЂСЃР°Р¶";
    else if (heatingState.manualMode) doc["mode"] = "Р СѓС‡РЅРѕР№";
    else doc["mode"] = "РђРІС‚Рѕ";
    
    doc["time"] = getTimeString();
    doc["memory"] = ESP.getFreeHeap() / 1024;
    doc["uptime"] = millis() / 1000;
    doc["historyCount"] = historyIndex;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ============================================================================
// РћР‘Р РђР‘РћРўРљРђ РљРћРњРђРќР” РР— Р’Р•Р‘Рђ
// ============================================================================

void handleWebCommand() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    if (!server.hasArg("cmd")) {
        server.send(400, "text/plain", "Missing command");
        return;
    }
    
    String cmd = server.arg("cmd");
    cmd.trim();
    
    Serial.println("WEB COMMAND: " + cmd);
    
    String response = processWebCommand(cmd);
    server.send(200, "text/plain", response);
}

String processWebCommand(const String& cmd) {
    String lowerCmd = cmd;
    lowerCmd.toLowerCase();
    
    if (lowerCmd.startsWith("pump ")) {
        int percent = cmd.substring(5).toInt();
        percent = constrain(percent, 0, 100);
        setPumpPercent(percent);
        return "вњ… РќР°СЃРѕСЃ: " + String(percent) + "%";
    }
    else if (lowerCmd.startsWith("fan ")) {
        int percent = cmd.substring(4).toInt();
        percent = constrain(percent, 0, 100);
        setFanPercent(percent);
        return "вњ… Р’РµРЅС‚РёР»СЏС‚РѕСЂ: " + String(percent) + "%";
    }
    else if (lowerCmd.startsWith("extractor ")) {
        int percent = cmd.substring(10).toInt();
        percent = constrain(percent, 0, 100);
        setExtractorPercent(percent);
        return "вњ… Р’С‹С‚СЏР¶РєР°: " + String(percent) + "%";
    }
    else if (lowerCmd == "auto") {
        heatingState.manualMode = false;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        return "вњ… Р РµР¶РёРј: РђР’РўРћРњРђРўРР§Р•РЎРљРР™";
    }
    else if (lowerCmd == "manual") {
        heatingState.manualMode = true;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        return "вњ… Р РµР¶РёРј: Р РЈР§РќРћР™";
    }
    else if (lowerCmd == "force") {
        heatingState.forceMode = true;
        heatingState.manualMode = false;
        return "рџ”Ґ Р РµР¶РёРј: Р¤РћР РЎРђР–";
    }
    else if (lowerCmd == "emergency") {
        heatingState.emergencyMode = true;
        return "рџљЁ Р РµР¶РёРј: РђР’РђР РРЇ";
    }
    else if (lowerCmd.startsWith("timer on ")) {
        int minutes = cmd.substring(9).toInt();
        minutes = constrain(minutes, 1, 240);
        config.extractorTimer.enabled = true;
        config.extractorTimer.onMinutes = minutes;
        config.extractorTimer.offMinutes = 0;
        config.extractorTimer.cycleStart = millis();
        setExtractorPercent(config.extractorTimer.powerPercent);
        return "вЏ° РўР°Р№РјРµСЂ: Р’РљР› РЅР° " + String(minutes) + " РјРёРЅ (" + String(config.extractorTimer.powerPercent) + "%)";
    }
    else if (lowerCmd == "timer off") {
        config.extractorTimer.enabled = false;
        setExtractorPercent(0);
        return "вЏ° РўР°Р№РјРµСЂ: Р’Р«РљР›";
    }
    else if (lowerCmd.startsWith("timer set ")) {
        String params = cmd.substring(10);
        int spaceIndex = params.indexOf(' ');
        if (spaceIndex > 0) {
            int onTime = params.substring(0, spaceIndex).toInt();
            int offTime = params.substring(spaceIndex + 1).toInt();
            
            onTime = constrain(onTime, 1, 120);
            offTime = constrain(offTime, 1, 120);
            
            config.extractorTimer.onMinutes = onTime;
            config.extractorTimer.offMinutes = offTime;
            config.extractorTimer.enabled = true;
            config.extractorTimer.cycleStart = millis();
            
            return "вЏ° РўР°Р№РјРµСЂ: " + String(onTime) + " РјРёРЅ Р’РљР› / " + String(offTime) + " РјРёРЅ Р’Р«РљР›";
        }
        return "вќЊ Р¤РѕСЂРјР°С‚: timer set <РјРёРЅ_РІРєР»> <РјРёРЅ_РІС‹РєР»>";
    }
    else if (lowerCmd.startsWith("timer power ")) {
        int power = cmd.substring(12).toInt();
        power = constrain(power, 10, 100);
        config.extractorTimer.powerPercent = power;
        return "вЏ° РњРѕС‰РЅРѕСЃС‚СЊ С‚Р°Р№РјРµСЂР°: " + String(power) + "%";
    }
    else if (lowerCmd.startsWith("tmin ")) {
        float temp = cmd.substring(5).toFloat();
        config.tempMin = temp;
        saveConfiguration();
        return "рџЊЎ РњРёРЅ. С‚РµРјРїРµСЂР°С‚СѓСЂР°: " + String(temp, 1) + "В°C";
    }
    else if (lowerCmd.startsWith("tmax ")) {
        float temp = cmd.substring(5).toFloat();
        config.tempMax = temp;
        saveConfiguration();
        return "рџЊЎ РњР°РєСЃ. С‚РµРјРїРµСЂР°С‚СѓСЂР°: " + String(temp, 1) + "В°C";
    }
    else if (lowerCmd.startsWith("hmin ")) {
        float hum = cmd.substring(5).toFloat();
        config.humidityConfig.minHumidity = hum;
        saveConfiguration();
        return "рџ’§ РњРёРЅ. РІР»Р°Р¶РЅРѕСЃС‚СЊ: " + String(hum, 1) + "%";
    }
    else if (lowerCmd.startsWith("hmax ")) {
        float hum = cmd.substring(5).toFloat();
        config.humidityConfig.maxHumidity = hum;
        saveConfiguration();
        return "рџ’§ РњР°РєСЃ. РІР»Р°Р¶РЅРѕСЃС‚СЊ: " + String(hum, 1) + "%";
    }
    else if (lowerCmd == "status") {
        return "рџ“Љ РЎС‚Р°С‚СѓСЃ РѕР±РЅРѕРІР»РµРЅ (РїСЂРѕРІРµСЂСЊС‚Рµ РґР°РЅРЅС‹Рµ РІС‹С€Рµ)";
    }
    else if (lowerCmd == "save") {
        saveConfiguration();
        return "рџ’ѕ РќР°СЃС‚СЂРѕР№РєРё СЃРѕС…СЂР°РЅРµРЅС‹";
    }
    else if (lowerCmd == "reboot") {
        server.send(200, "text/plain", "рџ”„ РџРµСЂРµР·Р°РіСЂСѓР·РєР°...");
        delay(1000);
        ESP.restart();
        return "";
    }
    else if (lowerCmd == "test vent") {
        testVentilation();
        return "рџ”§ РўРµСЃС‚ РІРµРЅС‚РёР»СЏС†РёРё РІС‹РїРѕР»РЅРµРЅ";
    }
    else if (lowerCmd == "test pump") {
        setPumpPercent(50);
        delay(5000);
        setPumpPercent(0);
        return "рџ”§ РўРµСЃС‚ РЅР°СЃРѕСЃР° РІС‹РїРѕР»РЅРµРЅ (5 СЃРµРє РЅР° 50%)";
    }
    else if (lowerCmd == "test fan") {
        setFanPercent(50);
        delay(5000);
        setFanPercent(0);
        return "рџ”§ РўРµСЃС‚ РІРµРЅС‚РёР»СЏС‚РѕСЂР° РІС‹РїРѕР»РЅРµРЅ (5 СЃРµРє РЅР° 50%)";
    }
    else if (lowerCmd == "web") {
        return "рџЊђ Р’РµР±-РёРЅС‚РµСЂС„РµР№СЃ: http://" + WiFi.localIP().toString();
    }
    else if (lowerCmd.startsWith("learn ")) {
        String learnCmd = cmd.substring(6);
        processLearningCommand(learnCmd);
        return "вњ… РљРѕРјР°РЅРґР° РѕР±СѓС‡РµРЅРёСЏ РІС‹РїРѕР»РЅРµРЅР°";
    }
    else {
        return "вќЊ РќРµРёР·РІРµСЃС‚РЅР°СЏ РєРѕРјР°РЅРґР°: " + cmd + "\nР”РѕСЃС‚СѓРїРЅС‹Рµ: pump, fan, extractor, timer, tmin, tmax, hmin, hmax, auto, manual, status, save, test";
    }
}

// ============================================================================
// РЎРўР РђРќРР¦Рђ РќРђРЎРўР РћР•Рљ WI-FI (РџРћР›РќРђРЇ Р’Р•Р РЎРРЇ)
// ============================================================================

void handleWiFiSettingsPage() {
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>РќР°СЃС‚СЂРѕР№РєРё Wi-Fi</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 900px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    html += "h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }";
    html += "h2 { color: #34495e; margin-top: 30px; }";
    html += ".section { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 25px; border-left: 4px solid #3498db; }";
    html += ".current-info { background: #e8f5e9; padding: 15px; border-radius: 5px; margin: 15px 0; }";
    html += ".network-list { max-height: 300px; overflow-y: auto; border: 1px solid #ddd; border-radius: 5px; padding: 10px; background: white; }";
    html += ".network-item { padding: 10px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; }";
    html += ".network-item:hover { background: #f0f0f0; }";
    html += ".network-ssid { font-weight: bold; }";
    html += ".network-details { font-size: 0.9em; color: #666; }";
    html += ".connect-btn { background: #4CAF50; color: white; padding: 5px 10px; border: none; border-radius: 3px; cursor: pointer; font-size: 0.9em; }";
    html += ".connect-btn:hover { background: #45a049; }";
    html += ".form-group { margin-bottom: 15px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #34495e; }";
    html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
    html += ".btn { background: #3498db; color: white; padding: 12px 25px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; transition: background 0.3s; margin-right: 10px; }";
    html += ".btn:hover { background: #2980b9; }";
    html += ".btn-scan { background: #9b59b6; }";
    html += ".btn-scan:hover { background: #8e44ad; }";
    html += ".btn-save { background: #2ecc71; }";
    html += ".btn-save:hover { background: #27ae60; }";
    html += ".status-message { padding: 10px; border-radius: 5px; margin: 10px 0; }";
    html += ".status-success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
    html += ".status-error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
    html += ".status-info { background: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; }";
    html += ".back-link { display: inline-block; margin-top: 20px; color: #7f8c8d; text-decoration: none; }";
    html += ".back-link:hover { color: #34495e; }";
    html += ".hidden { display: none; }";
    html += "</style>";
    html += "<script>";
    html += "function showManualForm() {";
    html += "  document.getElementById('manualForm').classList.remove('hidden');";
    html += "  document.getElementById('manualForm').scrollIntoView({ behavior: 'smooth' });";
    html += "}";
    html += "function connectToNetwork(ssid, encrypted) {";
    html += "  document.getElementById('connectSsid').value = ssid;";
    html += "  document.getElementById('connectEncrypted').value = encrypted;";
    html += "  if (encrypted === 'true') {";
    html += "    document.getElementById('manualForm').classList.remove('hidden');";
    html += "    document.getElementById('connectPassword').focus();";
    html += "  } else {";
    html += "    document.getElementById('connectPassword').value = '';";
    html += "    document.getElementById('wifiForm').submit();";
    html += "  }";
    html += "}";
    html += "function startScan() {";
    html += "  document.getElementById('scanBtn').innerHTML = 'вЏі РЎРєР°РЅРёСЂРѕРІР°РЅРёРµ...';";
    html += "  document.getElementById('scanBtn').disabled = true;";
    html += "  window.location.href = '/scan-wifi';";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>рџ“¶ РќР°СЃС‚СЂРѕР№РєРё Wi-Fi</h1>";
    html += "<p><a href='/' class='back-link'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    
    // РўРµРєСѓС‰РµРµ РїРѕРґРєР»СЋС‡РµРЅРёРµ
    html += "<div class='section'>";
    html += "<h2>РўРµРєСѓС‰РµРµ РїРѕРґРєР»СЋС‡РµРЅРёРµ</h2>";
    html += "<div class='current-info'>";
    html += "<p><strong>SSID:</strong> " + WiFi.SSID() + "</p>";
    html += "<p><strong>IP Р°РґСЂРµСЃ:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC Р°РґСЂРµСЃ:</strong> " + WiFi.macAddress() + "</p>";
    html += "<p><strong>РЎРёРіРЅР°Р»:</strong> " + wifiStrengthToHTML(WiFi.RSSI()) + "</p>";
    html += "<p><strong>РЎС‚Р°С‚СѓСЃ:</strong> " + String(WiFi.status() == WL_CONNECTED ? "вњ… РџРѕРґРєР»СЋС‡РµРЅРѕ" : "вќЊ РћС‚РєР»СЋС‡РµРЅРѕ") + "</p>";
    html += "</div>";
    html += "</div>";
    
    // РЎРєР°РЅРёСЂРѕРІР°РЅРёРµ СЃРµС‚РµР№
    html += "<div class='section'>";
    html += "<h2>Р”РѕСЃС‚СѓРїРЅС‹Рµ СЃРµС‚Рё</h2>";
    html += "<button id='scanBtn' class='btn btn-scan' onclick='startScan()'>";
    html += "рџ”Ќ РЎРєР°РЅРёСЂРѕРІР°С‚СЊ СЃРµС‚Рё";
    html += "</button>";
    html += "<div class='network-list'>";
    
    // РџРѕРєР°Р·Р°С‚СЊ СЃРѕС…СЂР°РЅРµРЅРЅС‹Рµ СЃРµС‚Рё РёР· preferences
    preferences.begin("wifi", true);
    String savedSSID = preferences.getString("ssid", "");
    preferences.end();
    
    if (savedSSID.length() > 0) {
        html += "<div class='network-item'>";
        html += "<div>";
        html += "<div class='network-ssid'>" + savedSSID + " (СЃРѕС…СЂР°РЅРµРЅРЅР°СЏ)</div>";
        html += "<div class='network-details'>РЎРѕС…СЂР°РЅРµРЅРЅР°СЏ СЃРµС‚СЊ - РёСЃРїРѕР»СЊР·СѓР№С‚Рµ С„РѕСЂРјСѓ РЅРёР¶Рµ РґР»СЏ РїРѕРґРєР»СЋС‡РµРЅРёСЏ</div>";
        html += "</div>";
        html += "<button class='connect-btn' onclick=\"connectToNetwork('" + savedSSID + "', 'true')\">РџРѕРґРєР»СЋС‡РёС‚СЊ</button>";
        html += "</div>";
    }
    
    html += "<div class='network-item'>";
    html += "<div>";
    html += "<div class='network-ssid'>Р’РІРµРґРёС‚Рµ РІСЂСѓС‡РЅСѓСЋ</div>";
    html += "<div class='network-details'>Р•СЃР»Рё РІР°С€РµР№ СЃРµС‚Рё РЅРµС‚ РІ СЃРїРёСЃРєРµ</div>";
    html += "</div>";
    html += "<button class='connect-btn' onclick=\"showManualForm()\">Р’СЂСѓС‡РЅСѓСЋ</button>";
    html += "</div>";
    
    html += "</div>"; // network-list
    html += "</div>"; // section
    
    // Р¤РѕСЂРјР° РїРѕРґРєР»СЋС‡РµРЅРёСЏ (СЃРєСЂС‹С‚Р° РїРѕ СѓРјРѕР»С‡Р°РЅРёСЋ)
    html += "<div class='section'>";
    html += "<h2>РџРѕРґРєР»СЋС‡РµРЅРёРµ Рє СЃРµС‚Рё</h2>";
    
    // Р’С‹РІРѕРґ СЃРѕРѕР±С‰РµРЅРёР№ РѕР± РѕС€РёР±РєР°С…/СѓСЃРїРµС…Рµ
    if (server.hasArg("error")) {
        html += "<div class='status-message status-error'>";
        html += "вќЊ " + server.arg("error");
        html += "</div>";
    }
    if (server.hasArg("success")) {
        html += "<div class='status-message status-success'>";
        html += "вњ… " + server.arg("success");
        html += "</div>";
    }
    
    html += "<form id='wifiForm' method='POST' action='/save-wifi' class='hidden' id='manualForm'>";
    html += "<div class='form-group'>";
    html += "<label for='connectSsid'>РќР°Р·РІР°РЅРёРµ СЃРµС‚Рё (SSID):</label>";
    html += "<input type='text' id='connectSsid' name='ssid' placeholder='Р’РІРµРґРёС‚Рµ РЅР°Р·РІР°РЅРёРµ СЃРµС‚Рё' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='connectPassword'>РџР°СЂРѕР»СЊ:</label>";
    html += "<input type='password' id='connectPassword' name='password' placeholder='Р’РІРµРґРёС‚Рµ РїР°СЂРѕР»СЊ'>";
    html += "<small>РћСЃС‚Р°РІСЊС‚Рµ РїСѓСЃС‚С‹Рј РґР»СЏ РѕС‚РєСЂС‹С‚С‹С… СЃРµС‚РµР№</small>";
    html += "</div>";
    
    html += "<input type='hidden' id='connectEncrypted' name='encrypted' value='true'>";
    
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='save' checked> РЎРѕС…СЂР°РЅРёС‚СЊ РЅР°СЃС‚СЂРѕР№РєРё</label>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-save'>рџ”— РџРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ</button>";
    html += "<button type='button' class='btn' onclick=\"window.location.href='/wifi-settings'\">РћС‚РјРµРЅР°</button>";
    html += "</form>";
    
    // РљРЅРѕРїРєР° РґР»СЏ РїРѕРєР°Р·Р° С„РѕСЂРјС‹
    html += "<button class='btn' onclick=\"showManualForm()\" id='showFormBtn'>вњЏпёЏ Р’РІРµСЃС‚Рё РґР°РЅРЅС‹Рµ РІСЂСѓС‡РЅСѓСЋ</button>";
    
    html += "</div>"; // section
    
    // Р¤РѕСЂРјР° С‚РѕС‡РєРё РґРѕСЃС‚СѓРїР°
    html += "<div class='section'>";
    html += "<h2>РўРѕС‡РєР° РґРѕСЃС‚СѓРїР°</h2>";
    html += "<div class='current-info'>";
    
    WiFiMode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        html += "<p><strong>РЎС‚Р°С‚СѓСЃ:</strong> вњ… РђРєС‚РёРІРЅР°</p>";
        html += "<p><strong>SSID С‚РѕС‡РєРё РґРѕСЃС‚СѓРїР°:</strong> ClimateControl</p>";
        html += "<p><strong>IP Р°РґСЂРµСЃ:</strong> " + WiFi.softAPIP().toString() + "</p>";
        html += "<p><strong>РџР°СЂРѕР»СЊ:</strong> 12345678</p>";
        html += "<p><em>РўРѕС‡РєР° РґРѕСЃС‚СѓРїР° Р°РєС‚РёРІРЅР°, РµСЃР»Рё РЅРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє Wi-Fi СЃРµС‚Рё</em></p>";
    } else {
        html += "<p><strong>РЎС‚Р°С‚СѓСЃ:</strong> вќЊ РќРµ Р°РєС‚РёРІРЅР°</p>";
        html += "<p><em>РўРѕС‡РєР° РґРѕСЃС‚СѓРїР° Р±СѓРґРµС‚ Р°РІС‚РѕРјР°С‚РёС‡РµСЃРєРё Р·Р°РїСѓС‰РµРЅР° РїСЂРё РѕС‚СЃСѓС‚СЃС‚РІРёРё Wi-Fi РїРѕРґРєР»СЋС‡РµРЅРёСЏ</em></p>";
    }
    
    html += "</div>";
    html += "</div>";
    
    html += "</div>"; // container
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// РЎРљРђРќРР РћР’РђРќРР• WI-FI РЎР•РўР•Р™
// ============================================================================

void handleScanWiFi() {
    Serial.println("рџ“¶ РЎРєР°РЅРёСЂРѕРІР°РЅРёРµ Wi-Fi СЃРµС‚РµР№...");
    
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    
    int n = WiFi.scanNetworks();
    Serial.printf("РќР°Р№РґРµРЅРѕ %d СЃРµС‚РµР№\n", n);
    
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='3;url=/wifi-settings'>";
    html += "<title>РЎРєР°РЅРёСЂРѕРІР°РЅРёРµ Wi-Fi</title>";
    html += "<style>";
    html += "body { font-family: Arial; text-align: center; padding: 50px; }";
    html += ".spinner { border: 8px solid #f3f3f3; border-top: 8px solid #3498db; border-radius: 50%; width: 60px; height: 60px; animation: spin 2s linear infinite; margin: 20px auto; }";
    html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='spinner'></div>";
    html += "<h1>рџ“¶ РЎРєР°РЅРёСЂРѕРІР°РЅРёРµ Wi-Fi СЃРµС‚РµР№...</h1>";
    html += "<p>РќР°Р№РґРµРЅРѕ СЃРµС‚РµР№: " + String(n) + "</p>";
    html += "<p>РџРµСЂРµРЅР°РїСЂР°РІР»РµРЅРёРµ РЅР° СЃС‚СЂР°РЅРёС†Сѓ РЅР°СЃС‚СЂРѕРµРє...</p>";
    html += "<script>";
    html += "setTimeout(function() { window.location.href = '/wifi-settings?scanned=true'; }, 3000);";
    html += "</script>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// РЎРћРҐР РђРќР•РќРР• РќРђРЎРўР РћР•Рљ WI-FI
// ============================================================================

void handleSaveWiFiSettings() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    bool saveSettings = server.hasArg("save");
    
    Serial.println("рџ’ѕ РЎРѕС…СЂР°РЅРµРЅРёРµ РЅР°СЃС‚СЂРѕРµРє Wi-Fi:");
    Serial.println("  SSID: " + ssid);
    Serial.println("  РЎРѕС…СЂР°РЅРёС‚СЊ: " + String(saveSettings ? "Р”Р°" : "РќРµС‚"));
    
    if (ssid.length() == 0) {
        server.send(400, "text/plain", "РћС€РёР±РєР°: SSID РЅРµ РјРѕР¶РµС‚ Р±С‹С‚СЊ РїСѓСЃС‚С‹Рј");
        return;
    }
    
    // РЎРѕС…СЂР°РЅСЏРµРј РІ Preferences
    if (saveSettings) {
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        Serial.println("вњ“ РќР°СЃС‚СЂРѕР№РєРё СЃРѕС…СЂР°РЅРµРЅС‹ РІ Preferences");
    }
    
    // РџСЂРѕР±СѓРµРј РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ
    WiFi.disconnect(true);
    delay(1000);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    Serial.println("рџ”— РџСЂРѕР±СѓСЋ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ Рє: " + ssid);
    
    // Р–РґРµРј РїРѕРґРєР»СЋС‡РµРЅРёСЏ
    int attempts = 0;
    bool connected = false;
    
    while (attempts < 30 && !connected) {
        delay(500);
        Serial.print(".");
        attempts++;
        
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
    }
    
    String response = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    response += "<meta http-equiv='refresh' content='5;url=/'>";
    response += "<title>РџРѕРґРєР»СЋС‡РµРЅРёРµ Wi-Fi</title>";
    response += "<style>";
    response += "body { font-family: Arial; text-align: center; padding: 50px; }";
    response += ".success { color: #2ecc71; }";
    response += ".error { color: #e74c3c; }";
    response += "</style>";
    response += "</head><body>";
    
    if (connected) {
        response += "<h1 class='success'>вњ… РЈСЃРїРµС€РЅРѕ РїРѕРґРєР»СЋС‡РµРЅРѕ!</h1>";
        response += "<p>РЎРµС‚СЊ: " + ssid + "</p>";
        response += "<p>IP Р°РґСЂРµСЃ: " + WiFi.localIP().toString() + "</p>";
        response += "<p>РЎРёРіРЅР°Р»: " + String(WiFi.RSSI()) + " dBm</p>";
        response += "<p>РџРµСЂРµРЅР°РїСЂР°РІР»РµРЅРёРµ РЅР° РіР»Р°РІРЅСѓСЋ СЃС‚СЂР°РЅРёС†Сѓ...</p>";
        
        Serial.println("\nвњ“ РџРѕРґРєР»СЋС‡РµРЅРёРµ СѓСЃРїРµС€РЅРѕ!");
        Serial.println("  IP: " + WiFi.localIP().toString());
        Serial.println("  RSSI: " + String(WiFi.RSSI()) + " dBm");
    } else {
        response += "<h1 class='error'>вќЊ РќРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ</h1>";
        response += "<p>РЎРµС‚СЊ: " + ssid + "</p>";
        response += "<p>РџСЂРѕРІРµСЂСЊС‚Рµ РїР°СЂРѕР»СЊ Рё СѓР±РµРґРёС‚РµСЃСЊ, С‡С‚Рѕ СЃРµС‚СЊ РґРѕСЃС‚СѓРїРЅР°.</p>";
        response += "<p>РЎРёСЃС‚РµРјР° РІРµСЂРЅРµС‚СЃСЏ РІ СЂРµР¶РёРј С‚РѕС‡РєРё РґРѕСЃС‚СѓРїР°.</p>";
        
        // Р—Р°РїСѓСЃРєР°РµРј С‚РѕС‡РєСѓ РґРѕСЃС‚СѓРїР°
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ClimateControl", "12345678");
        
        Serial.println("\nвљ  РќРµ СѓРґР°Р»РѕСЃСЊ РїРѕРґРєР»СЋС‡РёС‚СЊСЃСЏ, Р·Р°РїСѓСЃРєР°СЋ С‚РѕС‡РєСѓ РґРѕСЃС‚СѓРїР°");
    }
    
    response += "</body></html>";
    
    server.send(200, "text/html", response);
}

// ============================================================================
// РЎРўР РђРќРР¦Рђ РќРђРЎРўР РћР•Рљ РЎРРЎРўР•РњР«
// ============================================================================

void handleSettingsPage() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>РќР°СЃС‚СЂРѕР№РєРё СЃРёСЃС‚РµРјС‹</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    html += "h1 { color: #2c3e50; border-bottom: 2px solid #4CAF50; padding-bottom: 10px; }";
    html += ".form-group { margin-bottom: 20px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #34495e; }";
    html += "input[type='number'], input[type='text'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
    html += ".section { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 25px; border-left: 4px solid #3498db; }";
    html += ".section h3 { margin-top: 0; color: #2980b9; }";
    html += ".btn { background: #4CAF50; color: white; padding: 12px 25px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; transition: background 0.3s; }";
    html += ".btn:hover { background: #45a049; }";
    html += ".btn-secondary { background: #3498db; margin-left: 10px; }";
    html += ".btn-secondary:hover { background: #2980b9; }";
    html += ".back-link { display: inline-block; margin-top: 20px; color: #7f8c8d; text-decoration: none; }";
    html += ".back-link:hover { color: #34495e; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>вљ™пёЏ РќР°СЃС‚СЂРѕР№РєРё СЃРёСЃС‚РµРјС‹</h1>";
    html += "<p><a href='/' class='back-link'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    
    html += "<form method='POST' action='/settings'>";
    
    html += "<div class='section'>";
    html += "<h3>рџЊЎ РўРµРјРїРµСЂР°С‚СѓСЂР°</h3>";
    html += "<div class='form-group'>";
    html += "<label>РњРёРЅРёРјР°Р»СЊРЅР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР° (В°C):</label>";
    html += "<input type='number' step='0.1' name='tempMin' value='" + String(config.tempMin, 1) + "' min='10' max='40'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњР°РєСЃРёРјР°Р»СЊРЅР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР° (В°C):</label>";
    html += "<input type='number' step='0.1' name='tempMax' value='" + String(config.tempMax, 1) + "' min='10' max='40'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>рџ’§ Р’Р»Р°Р¶РЅРѕСЃС‚СЊ</h3>";
    html += "<div class='form-group'>";
    html += "<label>РњРёРЅРёРјР°Р»СЊРЅР°СЏ РІР»Р°Р¶РЅРѕСЃС‚СЊ (%):</label>";
    html += "<input type='number' step='0.1' name='humMin' value='" + String(config.humidityConfig.minHumidity, 1) + "' min='30' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњР°РєСЃРёРјР°Р»СЊРЅР°СЏ РІР»Р°Р¶РЅРѕСЃС‚СЊ (%):</label>";
    html += "<input type='number' step='0.1' name='humMax' value='" + String(config.humidityConfig.maxHumidity, 1) + "' min='30' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Р“РёСЃС‚РµСЂРµР·РёСЃ РІР»Р°Р¶РЅРѕСЃС‚Рё (%):</label>";
    html += "<input type='number' name='humHyst' value='" + String(config.humidityConfig.hysteresis) + "' min='1' max='10'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>рџ”§ РћР±РјРµР¶РµРЅРЅСЏ РїСЂРёСЃС‚СЂРѕС—РІ (Р°РІС‚РѕРјР°С‚РёС‡РЅРёР№ СЂРµР¶РёРј)</h3>";
    html += "<div class='form-group'>";
    html += "<label>РњС–РЅС–РјСѓРј РЅР°СЃРѕСЃР° (%):</label>";
    html += "<input type='number' name='pumpMin' value='" + String(config.pumpMinPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњР°РєСЃРёРјСѓРј РЅР°СЃРѕСЃР° (%):</label>";
    html += "<input type='number' name='pumpMax' value='" + String(config.pumpMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњР°РєСЃРёРјСѓРј РІРµРЅС‚РёР»СЏС‚РѕСЂР° (%):</label>";
    html += "<input type='number' name='fanMax' value='" + String(config.fanMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњС–РЅС–РјСѓРј РІРёС‚СЏР¶РєРё (%):</label>";
    html += "<input type='number' name='extractorMin' value='" + String(config.extractorMinPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњР°РєСЃРёРјСѓРј РІРёС‚СЏР¶РєРё (%):</label>";
    html += "<input type='number' name='extractorMax' value='" + String(config.extractorMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>вЏ° РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё</h3>";
    html += "<div class='form-group'>";
    html += "<label>Р’СЂРµРјСЏ СЂР°Р±РѕС‚С‹ (РјРёРЅСѓС‚С‹):</label>";
    html += "<input type='number' name='extOn' value='" + String(config.extractorTimer.onMinutes) + "' min='1' max='240'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Р’СЂРµРјСЏ РїР°СѓР·С‹ (РјРёРЅСѓС‚С‹):</label>";
    html += "<input type='number' name='extOff' value='" + String(config.extractorTimer.offMinutes) + "' min='0' max='240'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РњРѕС‰РЅРѕСЃС‚СЊ С‚Р°Р№РјРµСЂР° (%):</label>";
    html += "<input type='number' name='extPower' value='" + String(config.extractorTimer.powerPercent) + "' min='10' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='extEnabled' " + String(config.extractorTimer.enabled ? "checked" : "") + "> Р’РєР»СЋС‡РёС‚СЊ С‚Р°Р№РјРµСЂ</label>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>вљЎ РЎРёСЃС‚РµРјРЅС‹Рµ РЅР°СЃС‚СЂРѕР№РєРё</h3>";
    html += "<div class='form-group'>";
    html += "<label>РњРёРЅРёРјР°Р»СЊРЅР°СЏ РјРѕС‰РЅРѕСЃС‚СЊ РІРµРЅС‚РёР»СЏС‚РѕСЂР° (%):</label>";
    html += "<input type='number' name='fanMin' value='" + String(config.fanMinPercent) + "' min='0' max='30'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>РџРµСЂРёРѕРґ РІС‹РІРѕРґР° СЃС‚Р°С‚СѓСЃР° (СЃРµРєСѓРЅРґС‹):</label>";
    html += "<input type='number' name='statusPeriod' value='" + String(config.statusPeriod / 1000) + "' min='10' max='600'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div style='margin-top: 30px;'>";
    html += "<button type='submit' class='btn'>рџ’ѕ РЎРѕС…СЂР°РЅРёС‚СЊ РЅР°СЃС‚СЂРѕР№РєРё</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='window.location.href=\"/\"'>РћС‚РјРµРЅР°</button>";
    html += "</div>";
    
    html += "</form>";
    
    html += "</div>";
    
    html += "<script>";
    html += "document.querySelector('form').addEventListener('submit', function(e) {";
    html += "  const tempMin = parseFloat(document.querySelector('[name=\"tempMin\"]').value);";
    html += "  const tempMax = parseFloat(document.querySelector('[name=\"tempMax\"]').value);";
    html += "  if (tempMin >= tempMax) {";
    html += "    alert('РћС€РёР±РєР°: РјРёРЅРёРјР°Р»СЊРЅР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР° РґРѕР»Р¶РЅР° Р±С‹С‚СЊ РјРµРЅСЊС€Рµ РјР°РєСЃРёРјР°Р»СЊРЅРѕР№!');";
    html += "    e.preventDefault();";
    html += "  }";
    html += "  const humMin = parseFloat(document.querySelector('[name=\"humMin\"]').value);";
    html += "  const humMax = parseFloat(document.querySelector('[name=\"humMax\"]').value);";
    html += "  if (humMin >= humMax) {";
    html += "    alert('РћС€РёР±РєР°: РјРёРЅРёРјР°Р»СЊРЅР°СЏ РІР»Р°Р¶РЅРѕСЃС‚СЊ РґРѕР»Р¶РЅР° Р±С‹С‚СЊ РјРµРЅСЊС€Рµ РјР°РєСЃРёРјР°Р»СЊРЅРѕР№!');";
    html += "    e.preventDefault();";
    html += "  }";
    html += "});";
    html += "</script>";
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleSaveSettings() {
    if (server.hasArg("tempMin")) {
        config.tempMin = server.arg("tempMin").toFloat();
    }
    if (server.hasArg("tempMax")) {
        config.tempMax = server.arg("tempMax").toFloat();
    }
    if (server.hasArg("humMin")) {
        config.humidityConfig.minHumidity = server.arg("humMin").toFloat();
    }
    if (server.hasArg("humMax")) {
        config.humidityConfig.maxHumidity = server.arg("humMax").toFloat();
    }
    if (server.hasArg("humHyst")) {
        config.humidityConfig.hysteresis = server.arg("humHyst").toInt();
    }
    if (server.hasArg("pumpMin")) {
        config.pumpMinPercent = server.arg("pumpMin").toInt();
    }
    if (server.hasArg("pumpMax")) {
        config.pumpMaxPercent = server.arg("pumpMax").toInt();
    }
    if (server.hasArg("fanMax")) {
        config.fanMaxPercent = server.arg("fanMax").toInt();
    }
    if (server.hasArg("extractorMin")) {
        config.extractorMinPercent = server.arg("extractorMin").toInt();
    }
    if (server.hasArg("extractorMax")) {
        config.extractorMaxPercent = server.arg("extractorMax").toInt();
    }
    if (server.hasArg("extOn")) {
        config.extractorTimer.onMinutes = server.arg("extOn").toInt();
    }
    if (server.hasArg("extOff")) {
        config.extractorTimer.offMinutes = server.arg("extOff").toInt();
    }
    if (server.hasArg("extPower")) {
        config.extractorTimer.powerPercent = server.arg("extPower").toInt();
    }
    if (server.hasArg("extEnabled")) {
        config.extractorTimer.enabled = true;
    } else {
        config.extractorTimer.enabled = false;
    }
    if (server.hasArg("fanMin")) {
        config.fanMinPercent = server.arg("fanMin").toInt();
    }
    if (server.hasArg("statusPeriod")) {
        config.statusPeriod = server.arg("statusPeriod").toInt() * 1000UL;
    }
    
    saveConfiguration();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='2;url=/settings'>";
    html += "<title>РќР°СЃС‚СЂРѕР№РєРё СЃРѕС…СЂР°РЅРµРЅС‹</title>";
    html += "<style>body { font-family: Arial; text-align: center; padding: 50px; }</style>";
    html += "</head><body>";
    html += "<h1>вњ… РќР°СЃС‚СЂРѕР№РєРё СѓСЃРїРµС€РЅРѕ СЃРѕС…СЂР°РЅРµРЅС‹!</h1>";
    html += "<p>РџРµСЂРµРЅР°РїСЂР°РІР»РµРЅРёРµ РѕР±СЂР°С‚РЅРѕ РЅР° СЃС‚СЂР°РЅРёС†Сѓ РЅР°СЃС‚СЂРѕРµРє...</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// РЎРўР РђРќРР¦Рђ РЈРџР РђР’Р›Р•РќРРЇ
// ============================================================================

void handleControlPage() {
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>РЈРїСЂР°РІР»РµРЅРёРµ</title>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
    html += ".slider { width: 100%; margin: 10px 0; }";
    html += ".btn { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; margin: 5px; cursor: pointer; }";
    html += ".btn:hover { background: #45a049; }";
    html += ".slider-value { display: inline-block; width: 50px; text-align: center; font-weight: bold; }";
    html += ".control-section { margin-bottom: 30px; padding: 20px; background: #f9f9f9; border-radius: 8px; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>рџЋ›пёЏ РЈРїСЂР°РІР»РµРЅРёРµ СЃРёСЃС‚РµРјРѕР№</h1>";
    html += "<p><a href='/'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    
    html += "<div class='control-section'>";
    html += "<h3>РќР°СЃРѕСЃ (A)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String((heatingState.pumpPower * 100) / 255) + "' class='slider' id='pumpSlider' oninput='updatePump(this.value)'>";
    html += "<span class='slider-value' id='pumpValue'>" + String((heatingState.pumpPower * 100) / 255) + "%</span>";
    html += "<button class='btn' onclick=\"sendCmd('pump 0')\">Р’С‹РєР»</button>";
    html += "<button class='btn' onclick=\"sendCmd('pump 30')\">30%</button>";
    html += "<button class='btn' onclick=\"sendCmd('pump 50')\">50%</button>";
    html += "<button class='btn' onclick=\"sendCmd('pump 80')\">80%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>Р’РµРЅС‚РёР»СЏС‚РѕСЂ (B)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String((heatingState.fanPower * 100) / 255) + "' class='slider' id='fanSlider' oninput='updateFan(this.value)'>";
    html += "<span class='slider-value' id='fanValue'>" + String((heatingState.fanPower * 100) / 255) + "%</span>";
    html += "<button class='btn' onclick=\"sendCmd('fan 0')\">Р’С‹РєР»</button>";
    html += "<button class='btn' onclick=\"sendCmd('fan 30')\">30%</button>";
    html += "<button class='btn' onclick=\"sendCmd('fan 50')\">50%</button>";
    html += "<button class='btn' onclick=\"sendCmd('fan 80')\">80%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>Р’С‹С‚СЏР¶РєР° (C)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String((heatingState.extractorPower * 100) / 255) + "' class='slider' id='extractorSlider' oninput='updateExtractor(this.value)'>";
    html += "<span class='slider-value' id='extractorValue'>" + String((heatingState.extractorPower * 100) / 255) + "%</span>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 0')\">Р’С‹РєР»</button>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 30')\">30%</button>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 50')\">50%</button>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 80')\">80%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>Р РµР¶РёРјС‹ СЂР°Р±РѕС‚С‹</h3>";
    html += "<button class='btn' onclick=\"sendCmd('auto')\">РђРІС‚Рѕ</button>";
    html += "<button class='btn' onclick=\"sendCmd('manual')\">Р СѓС‡РЅРѕР№</button>";
    html += "<button class='btn' onclick=\"sendCmd('force')\" style='background: #ff9800;'>Р¤РѕСЂСЃР°Р¶</button>";
    html += "<button class='btn' onclick=\"sendCmd('emergency')\" style='background: #f44336;'>РђРІР°СЂРёСЏ</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё</h3>";
    html += "<button class='btn' onclick=\"sendCmd('timer on 5')\">5 РјРёРЅ</button>";
    html += "<button class='btn' onclick=\"sendCmd('timer on 15')\">15 РјРёРЅ</button>";
    html += "<button class='btn' onclick=\"sendCmd('timer on 30')\">30 РјРёРЅ</button>";
    html += "<button class='btn' onclick=\"sendCmd('timer on 60')\">60 РјРёРЅ</button>";
    html += "<button class='btn' onclick=\"sendCmd('timer off')\" style='background: #f44336;'>Р’С‹РєР»</button>";
    html += "<p>РўРµРєСѓС‰РёР№ С‚Р°Р№РјРµСЂ: " + String(config.extractorTimer.enabled ? "Р’РљР› (" + String(config.extractorTimer.onMinutes) + " РјРёРЅ)" : "Р’Р«РљР›") + "</p>";
    html += "</div>";
    
    html += "</div>";
    
    html += "<script>";
    html += "function updatePump(v) { document.getElementById('pumpValue').textContent = v + '%'; sendCmd('pump ' + v); }";
    html += "function updateFan(v) { document.getElementById('fanValue').textContent = v + '%'; sendCmd('fan ' + v); }";
    html += "function updateExtractor(v) { document.getElementById('extractorValue').textContent = v + '%'; sendCmd('extractor ' + v); }";
    html += "function sendCmd(cmd) {";
    html += "  fetch('/command', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'cmd=' + encodeURIComponent(cmd)";
    html += "  }).then(response => response.text()).then(text => {";
    html += "    console.log('РљРѕРјР°РЅРґР° РІС‹РїРѕР»РЅРµРЅР°:', text);";
    html += "  });";
    html += "}";
    html += "</script>";
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// Р”РћРџРћР›РќРРўР•Р›Р¬РќР«Р• РЎРўР РђРќРР¦Р«
// ============================================================================

void handleTimePage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 600px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>рџ•ђ Р’СЂРµРјСЏ СЃРёСЃС‚РµРјС‹</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>РўРµРєСѓС‰РµРµ РІСЂРµРјСЏ:</strong> " + getTimeString() + "</p>";
    html += "<p><strong>Р”Р°С‚Р°:</strong> " + getDateString() + "</p>";
    html += "<p><strong>Р¤РѕСЂРјР°С‚ РІСЂРµРјРµРЅРё:</strong> " + getFormattedTime() + "</p>";
    html += "<p><strong>РЎРёРЅС…СЂРѕРЅРёР·Р°С†РёСЏ:</strong> " + String(isTimeSynced() ? "вњ… РЎРёРЅС…СЂРѕРЅРёР·РёСЂРѕРІР°РЅРѕ" : "вљ  РќРµС‚ СЃРёРЅС…СЂРѕРЅРёР·Р°С†РёРё") + "</p>";
    html += "</div>";
    html += "<p><a href='/'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleWiFiPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 600px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>рџ“¶ Wi-Fi РёРЅС„РѕСЂРјР°С†РёСЏ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>SSID:</strong> " + WiFi.SSID() + "</p>";
    html += "<p><strong>IP Р°РґСЂРµСЃ:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC Р°РґСЂРµСЃ:</strong> " + WiFi.macAddress() + "</p>";
    html += "<p><strong>РЎРёРіРЅР°Р» (RSSI):</strong> " + String(WiFi.RSSI()) + " dBm</p>";
    html += "<p><strong>РЎС‚Р°С‚СѓСЃ:</strong> " + String(WiFi.status() == WL_CONNECTED ? "РџРѕРґРєР»СЋС‡РµРЅРѕ" : "РћС‚РєР»СЋС‡РµРЅРѕ") + "</p>";
    html += "</div>";
    html += "<p><a href='/'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleHistoryPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 800px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>рџ“€ РСЃС‚РѕСЂРёСЏ РґР°РЅРЅС‹С…</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>Р’СЃРµРіРѕ Р·Р°РїРёСЃРµР№:</strong> " + String(historyIndex) + "</p>";
    html += "<p><strong>Р Р°Р·РјРµСЂ Р±СѓС„РµСЂР°:</strong> " + String(HISTORY_BUFFER_SIZE) + " Р·Р°РїРёСЃРµР№</p>";
    html += "<p><strong>РРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅРѕ:</strong> " + String(historyInitialized ? "Р”Р°" : "РќРµС‚") + "</p>";
    html += "<p><em>РџРѕР»РЅС‹Р№ РїСЂРѕСЃРјРѕС‚СЂ РёСЃС‚РѕСЂРёРё Р±СѓРґРµС‚ РґРѕСЃС‚СѓРїРµРЅ РІ Р±СѓРґСѓС‰РёС… РІРµСЂСЃРёСЏС…</em></p>";
    html += "</div>";
    html += "<p><a href='/'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleDebugPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 800px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>рџђћ РћС‚Р»Р°РґРѕС‡РЅР°СЏ РёРЅС„РѕСЂРјР°С†РёСЏ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>РЎРІРѕР±РѕРґРЅР°СЏ РїР°РјСЏС‚СЊ:</strong> " + String(ESP.getFreeHeap() / 1024) + " KB</p>";
    html += "<p><strong>Р’СЃРµРіРѕ РїР°РјСЏС‚Рё:</strong> " + String(ESP.getHeapSize() / 1024) + " KB</p>";
    html += "<p><strong>Р—Р°РґР°С‡ FreeRTOS:</strong> " + String(uxTaskGetNumberOfTasks()) + "</p>";
    html += "<p><strong>Р’СЂРµРјСЏ СЂР°Р±РѕС‚С‹:</strong> " + String(millis() / 1000) + " СЃРµРєСѓРЅРґ</p>";
    html += "<p><strong>РўРµРјРїРµСЂР°С‚СѓСЂР° С‡РёРїР°:</strong> " + String(temperatureRead()) + "В°C</p>";
    html += "<p><strong>Р§Р°СЃС‚РѕС‚Р° CPU:</strong> " + String(getCpuFrequencyMhz()) + " MHz</p>";
    html += "<p><strong>Р’РµСЂСЃРёСЏ SDK:</strong> " + String(ESP.getSdkVersion()) + "</p>";
    html += "</div>";
    html += "<p><a href='/'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// РЎРўР РђРќРР¦Р« РЎРРЎРўР•РњР« РћР‘РЈР§Р•РќРРЇ
// ============================================================================

void handleLearningPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 800px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>рџ§  РЎРёСЃС‚РµРјР° РѕР±СѓС‡РµРЅРёСЏ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>Р—Р°РїРёСЃРµР№:</strong> " + String(learningCount) + "</p>";
    html += "<p><strong>РЎС‚Р°С‚СѓСЃ:</strong> " + String(learningEnabled ? "Р’РєР»СЋС‡РµРЅРѕ" : "Р’С‹РєР»СЋС‡РµРЅРѕ") + "</p>";
    html += "<p><strong>РђРєС‚РёРІРЅРѕ:</strong> " + String(isLearningActive ? "Р”Р°" : "РќРµС‚") + "</p>";
    html += "</div>";
    html += "<p><a href='/'>в†ђ РќР° РіР»Р°РІРЅСѓСЋ</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleLearningAPI() {
    if (!server.hasArg("cmd")) {
        server.send(400, "text/plain", "No command");
        return;
    }
    
    String cmd = server.arg("cmd");
    processLearningCommand(cmd);
    server.send(200, "text/plain", "OK");
}

// ============================================================================
// Р—РђР”РђР§Рђ Р’Р•Р‘-РЎР•Р Р’Р•Р Рђ
// ============================================================================

void webTask(void *parameter) {
    Serial.println("вњ“ Р’РµР±-Р·Р°РґР°С‡Р° Р·Р°РїСѓС‰РµРЅР°");
    
    while (1) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
