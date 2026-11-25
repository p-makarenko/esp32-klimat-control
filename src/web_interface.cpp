// ============================================================================
// WEB_INTERFACE.CPP - Головний модуль веб-інтерфейсу
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Ініціалізація WiFi, роутинг, автентифікація, webTask
// ============================================================================

#include "web_interface.h"
#include "web_common.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "global_declarations.h"
#include <ArduinoJson.h>
#include "advanced_climate_logic.h"
#include "data_storage.h"
#include "data_logger.h"
#include "google_sheets_sync.h"
#include "energy_monitor.h"
#include "config_manager.h"
#include "dmx_controller.h"
#include "light_scheduler.h"
#include "ota_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <NetBIOS.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <vector>
#include <algorithm>

extern int historyIndex;
extern bool historyInitialized;

// ============================================================================
// АВТЕНТИФІКАЦІЯ
// ============================================================================

bool checkAuth() {
    if (!config.useAuth) return true;

    if (!server.authenticate(config.authLogin.c_str(), config.authPassword.c_str())) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

// ============================================================================
// БЕЗПЕКА: CSRF ЗАХИСТ
// ============================================================================

bool checkCSRF() {
    if (server.method() != HTTP_POST) {
        return true;
    }

    if (server.hasHeader("Referer")) {
        String referer = server.header("Referer");
        String host = server.hostHeader();

        if (referer.indexOf(host) == -1 &&
            referer.indexOf(WiFi.localIP().toString()) == -1 &&
            referer.indexOf("klimat.local") == -1) {
            Serial.println("⚠️ CSRF: Невірний Referer: " + referer);
            return false;
        }
    } else {
        Serial.println("⚠️ CSRF: Відсутній Referer header (дозволено для форм)");
    }

    return true;
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ WI-FI ТА ВЕБ-СЕРВЕРА
// ============================================================================

static volatile bool webServerStarted = false;
static volatile bool webServerRestartRequested = false;

void requestWebServerRestart() {
    if (webServerStarted) {
        webServerRestartRequested = true;
    }
}

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // Вимикаємо modem sleep — стабільніші вхідні HTTP/ping на хотспотах
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)info;
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            WiFi.setSleep(false);
            requestWebServerRestart();
        }
    });
    WiFi.disconnect(true);
    delay(100);

    int n = WiFi.scanNetworks();

    bool connected = false;
    String connectedSSID = "";

    struct NetworkCred {
        String ssid;
        String password;
    };
    std::vector<NetworkCred> allKnownNets;

    preferences.begin("wifi", true);
    int savedCount = preferences.getInt("netCount", 0);
    for (int i = 0; i < savedCount && i < 10; i++) {
        String s = preferences.getString(("ssid" + String(i)).c_str(), "");
        String p = preferences.getString(("pass" + String(i)).c_str(), "");
        if (s.length() > 0) {
            allKnownNets.push_back({s, p});
        }
    }
    String lastSSID = preferences.getString("ssid", "");
    String lastPass = preferences.getString("password", "");
    if (lastSSID.length() > 0) {
        bool exists = false;
        for (const auto& kn : allKnownNets) {
            if (kn.ssid == lastSSID) { exists = true; break; }
        }
        if (!exists) allKnownNets.push_back({lastSSID, lastPass});
    }
    preferences.end();

    for (int i = 0; i < KNOWN_NETWORKS_COUNT; i++) {
        bool exists = false;
        for (const auto& kn : allKnownNets) {
            if (kn.ssid == KNOWN_NETWORKS[i].ssid) { exists = true; break; }
        }
        if (!exists) {
            allKnownNets.push_back({KNOWN_NETWORKS[i].ssid, KNOWN_NETWORKS[i].password});
        }
    }

    struct AvailableNetwork {
        String ssid;
        String password;
        int rssi;
    };

    std::vector<AvailableNetwork> availableNetworks;

    for (const auto& kn : allKnownNets) {
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == kn.ssid) {
                availableNetworks.push_back({kn.ssid, kn.password, WiFi.RSSI(j)});
                break;
            }
        }
    }

    std::sort(availableNetworks.begin(), availableNetworks.end(), [](const AvailableNetwork& a, const AvailableNetwork& b) {
        return a.rssi > b.rssi;
    });

    for (const auto& net : availableNetworks) {
        WiFi.disconnect(true);
        delay(100);

        if (config.useStaticIP) {
            IPAddress ip, gateway, subnet, dns;
            if (ip.fromString(config.staticIP) &&
                gateway.fromString(config.gateway) &&
                subnet.fromString(config.subnet) &&
                dns.fromString(config.dns)) {
                if ((ip & subnet) == (gateway & subnet)) {
                    WiFi.config(ip, gateway, subnet, dns);
                    Serial.printf("📍 Статична IP: %s\n", config.staticIP.c_str());
                } else {
                    Serial.println("⚠️ Статична IP не в підмережі шлюза");
                }
            }
        }

        WiFi.begin(net.ssid.c_str(), net.password.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            connectedSSID = net.ssid;

            delay(1000);

            preferences.begin("wifi", false);
            preferences.putString("ssid", net.ssid);
            preferences.putString("password", net.password);
            preferences.end();
            break;
        } else {
            WiFi.disconnect(true);
            delay(500);
        }
    }

    if (connected) {
        delay(500);
        Serial.flush();
        Serial.println("\n╔════════════════════════════════════════════════════════╗");
        Serial.println("║         ✅ ПІДКЛЮЧЕНО ДО WiFi МЕРЕЖІ                  ║");
        Serial.println("╠════════════════════════════════════════════════════════╣");
        Serial.printf("║  📡 Мережа:       %-33s║\n", WiFi.SSID().c_str());
        Serial.printf("║  🌐 IP:           %-33s║\n", WiFi.localIP().toString().c_str());
        Serial.printf("║  🔌 Шлюз:         %-33s║\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("║  📶 Сигнал:       %-25d dBm ║\n", WiFi.RSSI());
        Serial.println("╠════════════════════════════════════════════════════════╣");
        Serial.println("║  🔗 ПОСИЛАННЯ ДЛЯ ДОСТУПУ:                            ║");
        Serial.flush();

        Serial.printf("║  📱 IP адреса:    http://%-28s║\n", WiFi.localIP().toString().c_str());

        WiFi.setHostname("klimat");

        Serial.print("║  🔧 NetBIOS запуск...");
        if (NBNS.begin("klimat")) {
            Serial.println("                           ✅ ║");
            Serial.println("║  💻 http://klimat (Windows)                            ║");
        } else {
            Serial.println("                           ❌ ║");
        }

        Serial.print("║  🔧 mDNS запуск...");
        if (MDNS.begin("klimat")) {
            delay(100);
            if (MDNS.addService("http", "tcp", 80)) {
                Serial.println("                              ✅ ║");
            } else {
                Serial.println("                              ⚠️  ║");
            }
            Serial.println("║  🍎 http://klimat.local (Mac/iOS/Linux)                ║");
        } else {
            Serial.println("                              ❌ ║");
        }

        Serial.println("╠════════════════════════════════════════════════════════╣");
        Serial.println("║  💡 РЕКОМЕНДАЦІЯ: використовуйте IP адресу            ║");
        Serial.println("╚════════════════════════════════════════════════════════╝\n");
        Serial.flush();

        if (WiFi.RSSI() < -80) {
            Serial.println("⚠️  Слабкий сигнал Wi-Fi!");
        }

        // Піднімаємо постійну локальну AP-мережу для TC421 (AP+STA, підмережа 192.168.10.x)
        WiFi.mode(WIFI_AP_STA);
        WiFi.setSleep(false);
        WiFi.softAPConfig(IPAddress(192, 168, 10, 1), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP(LIGHT_AP_SSID, LIGHT_AP_PASSWORD, WiFi.channel(), 0, MAX_CLIENTS);
        Serial.printf("🔆 AP для TC421 запущено: SSID='%s' IP=%s (канал %d)\n",
                      LIGHT_AP_SSID, WiFi.softAPIP().toString().c_str(), WiFi.channel());
    } else {
        Serial.println("\n❌ Не вдалося підключитись ні до однієї відомої мережі");
        Serial.println("Запускаємо точку доступу...");

        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.setSleep(false);
        WiFi.softAPConfig(IPAddress(192, 168, 10, 1), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP(LIGHT_AP_SSID, LIGHT_AP_PASSWORD, WIFI_CHANNEL, 0, MAX_CLIENTS);

        Serial.print("✅ Точка доступу запущена. IP: ");
        Serial.println(WiFi.softAPIP());
        Serial.printf("   SSID: %s\n", LIGHT_AP_SSID);
        Serial.printf("   Пароль: %s\n", LIGHT_AP_PASSWORD);
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (WiFi.localIP() != IPAddress(0,0,0,0)) {
            Serial.println("✅ Локальна мережа доступна");
            configTime(0, 0, "pool.ntp.org");
            setenv("TZ", TZ_INFO, 1);
            tzset();
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 5000)) {
                Serial.println("✅ Інтернет доступний (NTP синхронізований)");
            } else {
                Serial.println("⚠️  Локальна мережа (немає доступу до інтернету)");
            }
        } else {
            Serial.println("⚠️  Немає мережевого підключення");
        }
    }

    // Роути
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/control", HTTP_GET, handleControlPage);
    server.on("/command", HTTP_POST, handleWebCommand);

    server.on("/settings", HTTP_GET, handleSettingsPage);
    server.on("/settings", HTTP_POST, handleSaveSettings);

    server.on("/wifi-settings", HTTP_GET, handleWiFiSettingsPage);
    server.on("/save-wifi", HTTP_POST, handleSaveWiFiSettings);
    server.on("/scan-wifi", HTTP_GET, handleScanWiFi);
    server.on("/wifi", HTTP_GET, handleWiFiPage);
    server.on("/saveNetwork", HTTP_POST, handleSaveNetworkSettings);

    server.on("/help", HTTP_GET, handleHelpPage);
    server.on("/debug", HTTP_GET, handleDebugPage);
    server.on("/tablet", HTTP_GET, handleTabletPage);

    server.on("/history", HTTP_GET, handleHistoryPage);
    server.on("/history/data", HTTP_GET, handleHistoryData);
    server.on("/history/stats", HTTP_GET, handleHistoryStats);
    server.on("/history/export", HTTP_GET, handleHistoryExport);
    server.on("/history/clear-logs", HTTP_POST, handleHistoryClearLogs);

    server.on("/servo", HTTP_GET, handleServoPage);
    server.on("/servo/api", HTTP_POST, handleServoAPI);

    server.on("/energy", HTTP_GET, handleEnergyPage);
    server.on("/energy/api", HTTP_GET, handleEnergyAPI);
    server.on("/energy/history", HTTP_GET, handleEnergyHistory);
    server.on("/energy/stats", HTTP_GET, handleEnergyHistoryStats);
    server.on("/energy/reset", HTTP_POST, handleEnergyReset);
    server.on("/energy/power", HTTP_GET, handleEnergyPowerData);

    server.on("/light", HTTP_GET, handleLightPage);
    server.on("/light/save", HTTP_POST, handleLightSave);
    server.on("/light/manual", HTTP_POST, handleLightManual);
    server.on("/light/off", HTTP_GET, handleLightOff);
    server.on("/light/toggle", HTTP_POST, handleLightToggle);
    server.on("/light/preset", HTTP_GET, handleLightPreset);

    // Окрема сторінка фітолампи TC421 WiFi
    server.on("/tc421", HTTP_GET, handleTC421Page);
    server.on("/tc421/discover", HTTP_GET, handleTC421Discover);
    server.on("/tc421/target", HTTP_POST, handleTC421Target);
    server.on("/tc421/schedule/save", HTTP_POST, handleTC421ScheduleSave);
    server.on("/tc421/preset", HTTP_GET, handleTC421Preset);
    server.on("/tc421/preset/reset", HTTP_GET, handleTC421ResetProfile);
    server.on("/tc421/manual", HTTP_POST, handleTC421Manual);
    server.on("/tc421/api/manual", HTTP_ANY, handleTC421ApiManual);
    server.on("/tc421/api/status", HTTP_GET, handleTC421ApiStatus);
    server.on("/tc421/api/auto", HTTP_ANY, handleTC421ApiAuto);
    server.on("/tc421/off", HTTP_GET, handleTC421Off);
    server.on("/tc421/internal", HTTP_GET, handleTC421Internal);
    server.on("/tc421/synctime", HTTP_GET, handleTC421SyncTime);

    // Вбудований енерго- та світловий аудит (зі смартфона через Phyphox)
    server.on("/audit", HTTP_GET, handleAuditPage);
    server.on("/audit/phyphox", HTTP_GET, handleAuditPhyphoxProxy);
    server.on("/audit/profile", HTTP_GET, handleAuditProfileGet);
    server.on("/audit/profile", HTTP_POST, handleAuditProfilePost);
    server.on("/audit/data", HTTP_GET, handleAuditDataGet);
    server.on("/audit/data", HTTP_POST, handleAuditDataPost);

    // Режим тиші (автоматичне вимкнення вентиляторів і зволожувача при роботі мікрофона)
    server.on("/api/quiet", HTTP_ANY, []() {
        if (server.hasArg("on")) {
            bool on = (server.arg("on") == "1" || server.arg("on") == "true");
            uint16_t ttl = server.hasArg("ttl") ? (uint16_t)server.arg("ttl").toInt() : 120;
            setQuietMode(on, ttl);
        }
        String json = "{\"quiet\":";
        json += isQuietModeActive() ? "true" : "false";
        json += ",\"remaining\":";
        json += String(getQuietModeRemainingSec());
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/ota", HTTP_GET, handleOTAPage);
    server.on("/api/ota/status", HTTP_GET, handleOTAStatus);
    server.on("/api/ota/password", HTTP_POST, handleOTAPasswordChange);
    server.on("/api/backup/create", HTTP_POST, handleBackupCreate);
    server.on("/api/backup/restore", HTTP_POST, handleBackupRestore);
    server.on("/api/backup/status", HTTP_GET, handleBackupStatus);
    server.on("/api/config/export", HTTP_GET, handleConfigExport);
    server.on("/api/config/import", HTTP_POST, handleConfigImport);
    server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);

    server.on("/update", HTTP_GET, []() {
        if (!checkAuth()) return;
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<title>OTA Update</title>";
        html += "<style>";
        html += "body{font-family:Arial;margin:40px;background:#1a1a2e;color:#fff}";
        html += ".container{max-width:400px;margin:0 auto;padding:20px;background:#16213e;border-radius:10px}";
        html += "h1{color:#0f0;text-align:center}";
        html += "input[type=file]{width:100%;padding:10px;margin:10px 0;background:#0f3460;border:none;color:#fff;border-radius:5px}";
        html += "input[type=submit]{width:100%;padding:15px;background:#0f0;border:none;color:#000;font-size:18px;cursor:pointer;border-radius:5px}";
        html += "input[type=submit]:hover{background:#0c0}";
        html += "#progress{width:100%;height:30px;background:#0f3460;border-radius:5px;margin:10px 0;display:none}";
        html += "#bar{width:0%;height:100%;background:#0f0;border-radius:5px;transition:width 0.3s}";
        html += "#status{text-align:center;margin:10px 0}";
        html += "</style></head><body>";
        html += "<div class='container'>";
        html += "<h1>🔄 OTA Update</h1>";
        html += "<p>Версія: " + String(VERSION) + "</p>";
        html += "<form method='POST' action='/update' enctype='multipart/form-data' id='uploadForm'>";
        html += "<input type='file' name='update' accept='.bin' required>";
        html += "<div id='progress'><div id='bar'></div></div>";
        html += "<div id='status'></div>";
        html += "<input type='submit' value='Завантажити прошивку'>";
        html += "</form>";
        html += "<script>";
        html += "document.getElementById('uploadForm').addEventListener('submit',function(e){";
        html += "e.preventDefault();";
        html += "var form=e.target;var data=new FormData(form);";
        html += "var file=form.querySelector('input[type=\"file\"]').files[0];";
        html += "var xhr=new XMLHttpRequest();";
        html += "document.getElementById('progress').style.display='block';";
        html += "xhr.upload.addEventListener('progress',function(e){";
        html += "if(e.lengthComputable){var p=Math.round((e.loaded/e.total)*100);";
        html += "document.getElementById('bar').style.width=p+'%';";
        html += "document.getElementById('status').innerHTML=p+'%';}});";
        html += "xhr.addEventListener('load',function(){";
        html += "if(xhr.status==200){document.getElementById('status').innerHTML='✅ Успіх! Перезавантаження...';setTimeout(function(){location.reload();},10000);}";
        html += "else{document.getElementById('status').innerHTML='❌ Помилка: '+xhr.responseText;}});";
        html += "xhr.open('POST','/update?size='+(file?file.size:0),true);xhr.send(data);});";
        html += "</script>";
        html += "</div></body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/update", HTTP_POST, handleOTAUploadResult, handleOTAUpload);

    server.on("/update-simple", HTTP_GET, []() {
        if (!checkAuth()) return;
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Simple OTA</title></head><body>";
        html += "<h1>Simple OTA Upload</h1>";
        html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
        html += "<input type='file' name='update' accept='.bin'><br><br>";
        html += "<input type='submit' value='Upload'>";
        html += "</form></body></html>";
        server.send(200, "text/html", html);
    });

    server.onNotFound([]() {
        server.send(404, "text/plain", "Сторінка не знайдена");
    });

    server.begin();
    webServerStarted = true;
    Serial.println("  Web OTA доступний на /update");
}

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ ДЛЯ WI-FI
// ============================================================================

String wifiStrengthToHTML(int rssi) {
    String strength;
    String color;

    if (rssi >= -50) {
        strength = "Відмінний";
        color = "#00C851";
    } else if (rssi >= -60) {
        strength = "Гарний";
        color = "#33b5e5";
    } else if (rssi >= -70) {
        strength = "Середній";
        color = "#ffbb33";
    } else if (rssi >= -80) {
        strength = "Слабкий";
        color = "#ff4444";
    } else {
        strength = "Дуже слабкий";
        color = "#cc0000";
    }

    return "<span style='color:" + color + "; font-weight:600;'>" + strength + " (" + String(rssi) + " dBm)</span>";
}

String encryptionTypeToString(wifi_auth_mode_t type) {
    switch(type) {
        case WIFI_AUTH_OPEN: return "Відкрита";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
        case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "Невідомо";
    }
}

// ============================================================================
// UKRAINE MARQUEE
// ============================================================================

String getUkraineMarquee() {
    String txt = "🇺🇦 СЛАВА УКРАЇНІ! | 💪 ГЕРОЯМ СЛАВА! | 🔥 москалі - ПІДАРИ! | ✊ Україна переможе! | 💩 рашисти - терористи | 💙💛 Разом до Перемоги! | ⚔️ Смерть ворогам! | 🛡️ ЗСУ - найсильніші! | 🤮 хуйло - злочинець! | 🗡️ За Україну! | 🚀 Крим - наш! | 🔴 moscow delenda est! | 💀 кацапи - genocid | ⚡ Слава Нації! | 🎖️ Честь і Воля! | 🔥 россія - терорист! | 💪 Воля або Смерть! | 🖕 всі москалі - ПІДАРИ на болотах! | 🇺🇦 Україна понад усе! &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

    String marquee = "<div style='text-align: center; margin-top: 20px;'>";
    marquee += "<button id='ukraineBtn' onclick='toggleUkraine()' style='background: linear-gradient(90deg, #0057B7 50%, #FFD700 50%); color: #000; border: 3px solid #000; padding: 15px 30px; font-size: 18px; font-weight: bold; border-radius: 10px; cursor: pointer; box-shadow: 0 4px 6px rgba(0,0,0,0.3);'>";
    marquee += "🇺🇦 ТИСНИ, ЯКЩО ЗА УКРАЇНУ! 🇺🇦";
    marquee += "</button>";
    marquee += "</div>";

    marquee += "<div id='ukraineMarquee' style='max-height: 0; opacity: 0; background: linear-gradient(90deg, #0057B7 0%, #0057B7 50%, #FFD700 50%, #FFD700 100%); color: #000; padding: 0; margin-top: 20px; overflow: hidden; position: relative; transition: max-height 0.3s ease, opacity 0.3s ease, padding 0.3s ease;'>";
    marquee += "<div class='ukraine-scroll' style='display: inline-block; white-space: nowrap; animation: scroll-seamless 40s linear infinite; font-weight: bold; font-size: 16px;'>";
    marquee += "<span style='padding-right: 50px;'>" + txt + "</span>";
    marquee += "<span style='padding-right: 50px;'>" + txt + "</span>";
    marquee += "</div>";
    marquee += "</div>";

    marquee += "<style>";
    marquee += "@keyframes scroll-seamless {";
    marquee += "  0% { transform: translateX(0%); }";
    marquee += "  100% { transform: translateX(-50%); }";
    marquee += "}";
    marquee += "#ukraineBtn:hover { transform: scale(1.05); box-shadow: 0 6px 12px rgba(0,0,0,0.4); }";
    marquee += "#ukraineBtn:active { transform: scale(0.98); }";
    marquee += "@media (max-width: 768px) {";
    marquee += "  .ukraine-scroll { animation-duration: 25s !important; font-size: 14px; }";
    marquee += "}";
    marquee += "@media (max-width: 480px) {";
    marquee += "  .ukraine-scroll { animation-duration: 20s !important; font-size: 13px; }";
    marquee += "}";
    marquee += "</style>";

    marquee += "<script>";
    marquee += "function toggleUkraine() {";
    marquee += "  const btn = document.getElementById('ukraineBtn');";
    marquee += "  const marquee = document.getElementById('ukraineMarquee');";
    marquee += "  if (marquee.style.maxHeight === '0px' || marquee.style.maxHeight === '') {";
    marquee += "    marquee.style.maxHeight = '50px';";
    marquee += "    marquee.style.opacity = '1';";
    marquee += "    marquee.style.padding = '10px 0';";
    marquee += "    btn.textContent = '🇺🇦 СХОВАТИ 🇺🇦';";
    marquee += "  } else {";
    marquee += "    marquee.style.maxHeight = '0px';";
    marquee += "    marquee.style.opacity = '0';";
    marquee += "    marquee.style.padding = '0';";
    marquee += "    btn.textContent = '🇺🇦 ТИСНИ, ЯКЩО ЗА УКРАЇНУ! 🇺🇦';";
    marquee += "  }";
    marquee += "}";
    marquee += "</script>";

    return marquee;
}

// ============================================================================
// OTA ТА BACKUP HANDLERS
// ============================================================================

void handleOTAPage() {
    if (!checkAuth()) return;

    String html = getHtmlHead("OTA та Backup");
    html += "<div class='container'>";
    html += getNavHeader("🔄 OTA ТА BACKUP");

    html += "<div class='card'>";
    html += "<h2>Інформація про систему</h2>";
    html += "<table style='width:100%'>";
    html += "<tr><td><strong>Версія прошивки:</strong></td><td>" + String(VERSION) + "</td></tr>";
    html += "<tr><td><strong>Дата збірки:</strong></td><td>" + String(BUILD_DATE) + " " + String(BUILD_TIME) + "</td></tr>";
    html += "<tr><td><strong>Розмір прошивки:</strong></td><td>" + String(FIRMWARE_SIZE_KB) + " KB</td></tr>";
    html += "<tr><td><strong>Вільна пам'ять:</strong></td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
    html += "</table>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<h2>Оновлення прошивки (OTA)</h2>";

    html += "<h3>Завантаження файлу</h3>";
    html += "<form id='otaForm' method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.bin' required style='margin-bottom:10px;'><br>";
    html += "<button type='submit' class='btn'>Завантажити прошивку</button>";
    html += "</form>";

    html += "<div id='progress' style='display:none; margin-top:20px;'>";
    html += "<div style='background:#333; border-radius:10px; overflow:hidden;'>";
    html += "<div id='progressBar' style='width:0%; height:30px; background:linear-gradient(90deg,#4CAF50,#8BC34A); transition:width 0.3s;'></div>";
    html += "</div>";
    html += "<p id='progressText' style='text-align:center; margin-top:10px;'>0%</p>";
    html += "</div>";

    html += "<h3 style='margin-top:30px;'>OTA пароль</h3>";
    html += "<p style='font-size:14px;color:#888;'>Пароль Arduino IDE OTA встановлено (приховано з міркувань безпеки).</p>";
    html += "<input type='password' id='newOtaPassword' placeholder='Новий пароль (мін. 4 символи)'>";
    html += "<button class='btn' onclick='changeOtaPassword()'>Змінити пароль</button>";

    html += "</div>";

    html += "<div class='card'>";
    html += "<h2>Backup конфігурації</h2>";

    BackupStatus backupStatus = getBackupStatus();
    if (backupStatus.exists) {
        html += "<p style='color:#4CAF50;'>Останній backup: ";
        if (backupStatus.valid) {
            html += String(backupStatus.size) + " bytes";
            html += " - " + htmlEscape(backupStatus.description);
            html += " <span style='color:#4CAF50;'>(CRC OK)</span>";
        } else {
            html += "<span style='color:#ff4444;'>ПОШКОДЖЕНИЙ</span>";
        }
        html += "</p>";
    } else {
        html += "<p style='color:#888;'>Backup відсутній</p>";
    }

    html += "<div style='display:flex; flex-wrap:wrap; gap:10px; margin-top:15px;'>";
    html += "<button class='btn' onclick='createBackup()'>Створити Backup</button>";
    html += "<button class='btn' onclick='restoreBackup()' style='background:#ff9800;'>Відновити Backup</button>";
    html += "<button class='btn' onclick='exportConfig()' style='background:#2196F3;'>Export JSON</button>";
    html += "<button class='btn' onclick='importConfig()' style='background:#9C27B0;'>Import JSON</button>";
    html += "</div>";
    html += "</div>";

    html += "<div class='card' style='border:2px solid #ff4444;'>";
    html += "<h2 style='color:#ff4444;'>Factory Reset</h2>";
    html += "<p style='color:#888;'>Скидання ВСІХ налаштувань до заводських. Ця дія незворотня!</p>";
    html += "<button class='btn' onclick='factoryReset()' style='background:#ff4444;'>Скинути до заводських</button>";
    html += "</div>";

    html += "</div>";

    html += "<script>";

    html += "function confirmOTA() {";
    html += "  return confirm('УВАГА!\\n\\n1. Переконайтесь у стабільному живленні\\n2. НЕ вимикайте ESP32 під час оновлення\\n3. Backup буде створено автоматично\\n\\nПродовжити?');";
    html += "}";

    html += "document.getElementById('otaForm').onsubmit = function(e) {";
    html += "  if (!confirmOTA()) { e.preventDefault(); return false; }";
    html += "  e.preventDefault();";
    html += "  var fileInput = document.querySelector('input[name=\"update\"]');";
    html += "  if (!fileInput.files || !fileInput.files[0]) { alert('Оберіть файл firmware.bin!'); return false; }";
    html += "  var file = fileInput.files[0];";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.upload.onprogress = function(e) {";
    html += "    if (e.lengthComputable) {";
    html += "      var percent = Math.round((e.loaded / e.total) * 100);";
    html += "      document.getElementById('progressBar').style.width = percent + '%';";
    html += "      document.getElementById('progressText').innerText = percent + '% (' + Math.round(e.loaded/1024) + ' / ' + Math.round(e.total/1024) + ' KB)';";
    html += "    }";
    html += "  };";
    html += "  xhr.onload = function() {";
    html += "    if (xhr.status === 200) {";
    html += "      document.getElementById('progressText').innerText = '✅ Успішно! Перезавантаження пристрою...';";
    html += "      setTimeout(function() { location.reload(); }, 12000);";
    html += "    } else {";
    html += "      alert('Помилка: ' + xhr.responseText);";
    html += "      document.getElementById('progress').style.display = 'none';";
    html += "    }";
    html += "  };";
    html += "  xhr.onerror = function() { alert('Помилка з\\'єднання під час OTA'); };";
    html += "  document.getElementById('progress').style.display = 'block';";
    html += "  xhr.open('POST', '/update?size=' + file.size);";
    html += "  var formData = new FormData();";
    html += "  formData.append('update', file, file.name);";
    html += "  xhr.send(formData);";
    html += "};";

    html += "function changeOtaPassword() {";
    html += "  var newPass = document.getElementById('newOtaPassword').value;";
    html += "  if (newPass.length < 4) { alert('Пароль повинен бути мінімум 4 символи'); return; }";
    html += "  fetch('/api/ota/password', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/json'},";
    html += "    body: JSON.stringify({password: newPass})";
    html += "  }).then(r => r.json()).then(data => {";
    html += "    alert(data.success ? 'Пароль змінено' : 'Помилка: ' + data.error);";
    html += "    if (data.success) location.reload();";
    html += "  });";
    html += "}";

    html += "function createBackup() {";
    html += "  fetch('/api/backup/create', {method: 'POST'})";
    html += "    .then(r => r.json())";
    html += "    .then(data => { alert(data.success ? 'Backup створено' : 'Помилка'); location.reload(); });";
    html += "}";

    html += "function restoreBackup() {";
    html += "  if (!confirm('Відновити конфігурацію з backup?\\nСистема перезавантажиться.')) return;";
    html += "  fetch('/api/backup/restore', {method: 'POST'})";
    html += "    .then(r => r.json())";
    html += "    .then(data => { alert(data.message); });";
    html += "}";

    html += "function exportConfig() {";
    html += "  fetch('/api/config/export')";
    html += "    .then(r => r.text())";
    html += "    .then(data => {";
    html += "      var blob = new Blob([data], {type: 'application/json'});";
    html += "      var url = URL.createObjectURL(blob);";
    html += "      var a = document.createElement('a');";
    html += "      a.href = url;";
    html += "      a.download = 'klimat_config_' + Date.now() + '.json';";
    html += "      a.click();";
    html += "    });";
    html += "}";

    html += "function importConfig() {";
    html += "  var input = document.createElement('input');";
    html += "  input.type = 'file';";
    html += "  input.accept = '.json';";
    html += "  input.onchange = function(e) {";
    html += "    var file = e.target.files[0];";
    html += "    var reader = new FileReader();";
    html += "    reader.onload = function(event) {";
    html += "      fetch('/api/config/import', {";
    html += "        method: 'POST',";
    html += "        headers: {'Content-Type': 'application/json'},";
    html += "        body: event.target.result";
    html += "      }).then(r => r.json()).then(data => {";
    html += "        alert(data.success ? 'Імпорт успішний' : 'Помилка');";
    html += "        if (data.success) location.reload();";
    html += "      });";
    html += "    };";
    html += "    reader.readAsText(file);";
    html += "  };";
    html += "  input.click();";
    html += "}";

    html += "function factoryReset() {";
    html += "  if (!confirm('УВАГА!\\n\\nВсі налаштування будуть ВИДАЛЕНІ!\\nЦя дія НЕЗВОРОТНЯ!\\n\\nПродовжити?')) return;";
    html += "  if (!confirm('ВИ АБСОЛЮТНО ВПЕВНЕНІ?\\n\\nСистема повернеться до заводських налаштувань.')) return;";
    html += "  fetch('/api/factory-reset', {method: 'POST'})";
    html += "    .then(r => r.json())";
    html += "    .then(data => { alert(data.message); });";
    html += "}";

    html += "</script>";

    html += getHtmlFooter();
    server.send(200, "text/html", html);
}

void handleOTAStatus() {
    if (!checkAuth()) return;

    OTAStatus status = getOTAStatus();

    JsonDocument doc;
    doc["inProgress"] = status.inProgress;
    doc["progress"] = status.progressPercent;
    doc["currentVersion"] = status.currentVersion;
    doc["error"] = status.errorMessage;
    doc["success"] = status.success;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void handleOTAPasswordChange() {
    if (!checkAuth()) return;

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err || !doc["password"].is<const char*>()) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    String newPassword = doc["password"].as<String>();

    if (newPassword.length() < 4) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Password too short\"}");
        return;
    }

    if (changeOTAPassword(newPassword)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save\"}");
    }
}

void handleBackupCreate() {
    if (!checkAuth()) return;

    if (createBackup("Manual backup from web")) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Backup failed\"}");
    }
}

void handleBackupRestore() {
    if (!checkAuth()) return;

    server.send(200, "application/json", "{\"success\":true,\"message\":\"Відновлення... Система перезавантажиться.\"}");

    delay(1000);
    restoreBackup();
}

void handleBackupStatus() {
    if (!checkAuth()) return;

    BackupStatus status = getBackupStatus();

    JsonDocument doc;
    doc["exists"] = status.exists;
    doc["timestamp"] = status.timestamp;
    doc["size"] = status.size;
    doc["valid"] = status.valid;
    doc["description"] = status.description;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void handleConfigExport() {
    if (!checkAuth()) return;

    String json = exportConfigJSON();
    server.send(200, "application/json", json);
}

void handleConfigImport() {
    if (!checkAuth()) return;

    String body = server.arg("plain");

    if (importConfigJSON(body)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    }
}

void handleFactoryReset() {
    if (!checkAuth()) return;

    server.send(200, "application/json", "{\"success\":true,\"message\":\"Factory reset... Система перезавантажиться.\"}");

    delay(2000);
    factoryResetComplete();
}

static bool webOtaStarted = false;

void handleOTAUploadResult() {
    if (!checkAuth()) return;
    if (!webOtaStarted) {
        server.send(400, "text/plain", "OTA not started");
        return;
    }
    webOtaStarted = false;
    setOTAInProgress(false);
    bool success = !Update.hasError();
    String message = success ? "OK" : Update.errorString();

    server.sendHeader("Connection", "close");
    server.send(success ? 200 : 500, "text/plain", message);

    if (success) {
        Serial.println("\n✅ Web OTA завершено успішно");
        delay(1000);
        ESP.restart();
    }
}

void handleOTAUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("\n📥 Web OTA Upload: %s\n", upload.filename.c_str());

        // Перевірка автентифікації
        if (!checkAuth()) {
            return;
        }

        // Позначаємо що OTA активна
        setOTAInProgress(true);

        // Автоматичний backup ДО призупинення задач!
        Serial.println("\n📦 Автоматичний backup поточної конфігурації...");
        if (createBackup("Auto backup before Web OTA")) {
            Serial.println("✅ Backup створено - конфіг збережено");
        } else {
            Serial.println("⚠️  Попередження: Backup не створено, але OTA продовжується");
        }

        // Зупиняємо критичні операції
        pauseCriticalTasks();

        // Затримка для стабілізації
        delay(100);

        // Очищаємо попередні помилки Update
        Update.clearError();

        // Підтверджуємо поточний образ у ESP-IDF щоб скасувати будь-який завислий rollback стан
        esp_ota_mark_app_valid_cancel_rollback();

        const esp_partition_t* runningPart = esp_ota_get_running_partition();
        const esp_partition_t* nextPart = esp_ota_get_next_update_partition(NULL);
        if (runningPart) {
            Serial.printf("🔍 OTA Running partition: %s @ 0x%06X\n", runningPart->label, runningPart->address);
        }
        if (nextPart) {
            Serial.printf("🔍 OTA Target partition:  %s @ 0x%06X\n", nextPart->label, nextPart->address);
        } else {
            Serial.println("❌ OTA Target partition NOT FOUND!");
        }

        size_t otaSize = UPDATE_SIZE_UNKNOWN;
        if (server.hasArg("size") && server.arg("size").toInt() > 0) {
            otaSize = (size_t)server.arg("size").toInt();
            Serial.printf("📏 Отримано розмір файлу: %u байт\n", otaSize);
        }

        // Повне або часткове очищення цільового розділу перед записом щоб уникнути сміття у хвості
        if (nextPart) {
            size_t eraseSize = (otaSize != UPDATE_SIZE_UNKNOWN) ? otaSize : nextPart->size;
            eraseSize = (eraseSize + 4095) & ~4095;
            if (eraseSize > nextPart->size) eraseSize = nextPart->size;
            Serial.printf("🧹 Очищення цільового розділу %s (%u байт)...\n", nextPart->label, eraseSize);
            esp_task_wdt_reset();
            esp_err_t err = esp_partition_erase_range(nextPart, 0, eraseSize);
            if (err == ESP_OK) {
                Serial.println("✅ Цільовий розділ успішно очищено");
            } else {
                Serial.printf("⚠️ Помилка очищення: 0x%x\n", err);
            }
            esp_task_wdt_reset();
        }

        if (!Update.begin(otaSize, U_FLASH)) {
            Serial.println("❌ Update.begin() failed");
            Update.printError(Serial);
            webOtaStarted = false;
            setOTAInProgress(false);
            resumeCriticalTasks();
            return;
        }
        webOtaStarted = true;
        Serial.printf("📥 OTA Started: %s (розмір: %s)\n", upload.filename.c_str(),
                      otaSize == UPDATE_SIZE_UNKNOWN ? "невідомий" : String(otaSize).c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        esp_task_wdt_reset();
        if (!Update.hasError()) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Serial.println("❌ Update.write() failed");
                Update.printError(Serial);
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        esp_task_wdt_reset();
        Serial.printf("📊 Total: %u bytes, written: %u bytes, freeHeap: %u\n",
                      upload.totalSize, Update.progress(), ESP.getFreeHeap());

        if (Update.end(true)) {
            Serial.println("✅ OTA Success!");
            setOTAInProgress(false);
        } else {
            Serial.printf("❌ OTA Error: %s (MD5 rx: %s)\n", Update.errorString(), Update.md5String().c_str());
            Update.printError(Serial);

            // Повторна спроба активації після паузи (перевірка чи не було тимчасового збою SHA/Cache)
            const esp_partition_t* nextPart = esp_ota_get_next_update_partition(NULL);
            if (nextPart) {
                delay(200);
                esp_task_wdt_reset();
                esp_err_t retryErr = esp_ota_set_boot_partition(nextPart);
                Serial.printf("🔁 Retry esp_ota_set_boot_partition(%s): 0x%x (%s)\n",
                              nextPart->label, retryErr, esp_err_to_name(retryErr));
                if (retryErr == ESP_OK) {
                    Update.clearError();
                    Serial.println("✅ OTA Success on retry!");
                    setOTAInProgress(false);
                    return;
                }
            }

            setOTAInProgress(false);
            resumeCriticalTasks();
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("❌ Upload aborted");
        Update.end();
        webOtaStarted = false;
        setOTAInProgress(false);
        resumeCriticalTasks();
    }
}

// ============================================================================
// ЗАВДАННЯ ВЕБ-СЕРВЕРА З WATCHDOG
// ============================================================================

#include <esp_task_wdt.h>

#define WEB_WATCHDOG_TIMEOUT_S  60

void webTask(void *parameter) {
    Serial.println("✅ Веб-завдання запущено");

    esp_task_wdt_init(WEB_WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    Serial.printf("🐕 Watchdog активовано: %d сек timeout\n", WEB_WATCHDOG_TIMEOUT_S);

    while (1) {
        esp_task_wdt_reset();
        if (webServerRestartRequested) {
            webServerRestartRequested = false;
            server.stop();
            vTaskDelay(pdMS_TO_TICKS(50));
            server.begin();
            Serial.println("🔄 WebServer безпечно перезапущено після оновлення Wi-Fi");
        }
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
