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
#include "learning_system.h"
#include "global_declarations.h"
#include <ArduinoJson.h>
#include "advanced_climate_logic.h"
#include "data_storage.h"
#include "data_logger.h"
#include "google_sheets_sync.h"
#include "energy_monitor.h"
#include "config_manager.h"
#include "ota_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <NetBIOS.h>
#include <Update.h>
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
    // Для GET запитів не перевіряємо
    if (server.method() != HTTP_POST) {
        return true;
    }

    // Якщо є Referer header, перевіряємо його
    if (server.hasHeader("Referer")) {
        String referer = server.header("Referer");
        String host = server.hostHeader();

        // Перевіряємо що Referer містить наш хост
        if (referer.indexOf(host) == -1 &&
            referer.indexOf(WiFi.localIP().toString()) == -1 &&
            referer.indexOf("klimat.local") == -1) {
            Serial.println("⚠️ CSRF: Невірний Referer: " + referer);
            return false;
        }
    } else {
        // Якщо Referer відсутній, логуємо попередження але дозволяємо
        Serial.println("⚠️ CSRF: Відсутній Referer header (дозволено для форм)");
    }

    return true;
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ WI-FI ТА ВЕБ-СЕРВЕРА
// ============================================================================

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    // Скануємо мережі
    int n = WiFi.scanNetworks();

    bool connected = false;
    String connectedSSID = "";

    // Збираємо список доступних відомих мереж
    struct AvailableNetwork {
        String ssid;
        String password;
        int rssi;
    };

    std::vector<AvailableNetwork> availableNetworks;

    for (int i = 0; i < KNOWN_NETWORKS_COUNT; i++) {
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == KNOWN_NETWORKS[i].ssid) {
                availableNetworks.push_back({KNOWN_NETWORKS[i].ssid, KNOWN_NETWORKS[i].password, WiFi.RSSI(j)});
                break;
            }
        }
    }

    // Сортуємо по сигналу
    std::sort(availableNetworks.begin(), availableNetworks.end(), [](const AvailableNetwork& a, const AvailableNetwork& b) {
        return a.rssi > b.rssi;
    });

    // Пробуємо підключитись до мереж в порядку кращого сигналу
    for (const auto& net : availableNetworks) {
        WiFi.disconnect(true);
        delay(100);

        // Налаштування статичної IP якщо увімкнено
        if (config.useStaticIP) {
            IPAddress ip, gateway, subnet, dns;
            if (ip.fromString(config.staticIP) &&
                gateway.fromString(config.gateway) &&
                subnet.fromString(config.subnet) &&
                dns.fromString(config.dns)) {
                // Перевірка підмережі
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

            // Зберігаємо успішні налаштування
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

    // Якщо підключились - показуємо інформацію
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

        // Встановлюємо hostname
        WiFi.setHostname("klimat");

        // NetBIOS - працює на Windows краще за mDNS
        Serial.print("║  🔧 NetBIOS запуск...");
        if (NBNS.begin("klimat")) {
            Serial.println("                           ✅ ║");
            Serial.println("║  💻 http://klimat (Windows)                            ║");
        } else {
            Serial.println("                           ❌ ║");
        }

        // mDNS - для Mac/iOS/Linux
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
    } else {
        // Якщо не вдалося підключитись - запускаємо точку доступу
        Serial.println("\n❌ Не вдалося підключитись ні до однієї відомої мережі");
        Serial.println("Запускаємо точку доступу...");

        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ClimateControl", "12345678");

        Serial.print("✅ Точка доступу запущена. IP: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("   SSID: ClimateControl");
        Serial.println("   Пароль: 12345678");
    }

    // Перевіряємо підключення
    if (WiFi.status() == WL_CONNECTED) {
        if (WiFi.localIP() != IPAddress(0,0,0,0)) {
            Serial.println("✅ Локальна мережа доступна");
            configTime(0, 0, "pool.ntp.org");
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

    // ============================================================================
    // НАЛАШТУВАННЯ РОУТІВ ВЕБ-СЕРВЕРА
    // ============================================================================

    // Головні сторінки
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/control", HTTP_GET, handleControlPage);
    server.on("/command", HTTP_POST, handleWebCommand);

    // Налаштування
    server.on("/settings", HTTP_GET, handleSettingsPage);
    server.on("/settings", HTTP_POST, handleSaveSettings);

    // WiFi налаштування
    server.on("/wifi-settings", HTTP_GET, handleWiFiSettingsPage);
    server.on("/save-wifi", HTTP_POST, handleSaveWiFiSettings);
    server.on("/scan-wifi", HTTP_GET, handleScanWiFi);
    server.on("/wifi", HTTP_GET, handleWiFiPage);
    server.on("/saveNetwork", HTTP_POST, handleSaveNetworkSettings);

    // Навчання
    server.on("/learning", HTTP_GET, handleLearningPage);
    server.on("/learning/api", HTTP_POST, handleLearningAPI);

    // Час та допомога
    server.on("/time", HTTP_GET, handleTimePage);
    server.on("/help", HTTP_GET, handleHelpPage);
    server.on("/debug", HTTP_GET, handleDebugPage);

    // Історія та графіки
    server.on("/history", HTTP_GET, handleHistoryPage);
    server.on("/history/data", HTTP_GET, handleHistoryData);
    server.on("/history/stats", HTTP_GET, handleHistoryStats);
    server.on("/history/export", HTTP_GET, handleHistoryExport);

    // Серво калібрування
    server.on("/servo", HTTP_GET, handleServoPage);
    server.on("/servo/api", HTTP_POST, handleServoAPI);

    // Енергоконтролер
    server.on("/energy", HTTP_GET, handleEnergyPage);
    server.on("/energy/api", HTTP_GET, handleEnergyAPI);
    server.on("/energy/history", HTTP_GET, handleEnergyHistory);
    server.on("/energy/stats", HTTP_GET, handleEnergyHistoryStats);

    // OTA та Backup
    server.on("/ota", HTTP_GET, handleOTAPage);
    server.on("/api/ota/status", HTTP_GET, handleOTAStatus);
    server.on("/api/ota/password", HTTP_POST, handleOTAPasswordChange);
    server.on("/api/backup/create", HTTP_POST, handleBackupCreate);
    server.on("/api/backup/restore", HTTP_POST, handleBackupRestore);
    server.on("/api/backup/status", HTTP_GET, handleBackupStatus);
    server.on("/api/config/export", HTTP_GET, handleConfigExport);
    server.on("/api/config/import", HTTP_POST, handleConfigImport);
    server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);

    // Проста Web OTA Upload сторінка
    server.on("/update", HTTP_GET, []() {
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
        html += "var xhr=new XMLHttpRequest();";
        html += "document.getElementById('progress').style.display='block';";
        html += "xhr.upload.addEventListener('progress',function(e){";
        html += "if(e.lengthComputable){var p=Math.round((e.loaded/e.total)*100);";
        html += "document.getElementById('bar').style.width=p+'%';";
        html += "document.getElementById('status').innerHTML=p+'%';}});";
        html += "xhr.addEventListener('load',function(){";
        html += "if(xhr.status==200){document.getElementById('status').innerHTML='✅ Успіх! Перезавантаження...';}";
        html += "else{document.getElementById('status').innerHTML='❌ Помилка: '+xhr.responseText;}});";
        html += "xhr.open('POST','/update',true);xhr.send(data);});";
        html += "</script>";
        html += "</div></body></html>";
        server.send(200, "text/html", html);
    });

    // Web OTA Upload handler
    server.on("/update", HTTP_POST, handleOTAUploadResult, handleOTAUpload);

    server.onNotFound([]() {
        server.send(404, "text/plain", "Сторінка не знайдена");
    });

    server.begin();
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

// Сторінка OTA та Backup
void handleOTAPage() {
    if (!checkAuth()) return;

    String html = getHtmlHead("OTA та Backup");
    html += getNavHeader("OTA та Backup");

    html += "<div class='container'>";

    // Інформація про версію
    html += "<div class='card'>";
    html += "<h2>Інформація про систему</h2>";
    html += "<table style='width:100%'>";
    html += "<tr><td><strong>Версія прошивки:</strong></td><td>" + String(VERSION) + "</td></tr>";
    html += "<tr><td><strong>Дата збірки:</strong></td><td>" + String(BUILD_DATE) + " " + String(BUILD_TIME) + "</td></tr>";
    html += "<tr><td><strong>Розмір прошивки:</strong></td><td>" + String(FIRMWARE_SIZE_KB) + " KB</td></tr>";
    html += "<tr><td><strong>Вільна пам'ять:</strong></td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
    html += "</table>";
    html += "</div>";

    // OTA оновлення
    html += "<div class='card'>";
    html += "<h2>Оновлення прошивки (OTA)</h2>";

    // Web Upload форма
    html += "<h3>Завантаження файлу</h3>";
    html += "<form id='otaForm' method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update' accept='.bin' required style='margin-bottom:10px;'><br>";
    html += "<button type='submit' class='btn' onclick='return confirmOTA()'>Завантажити прошивку</button>";
    html += "</form>";

    // Progress bar
    html += "<div id='progress' style='display:none; margin-top:20px;'>";
    html += "<div style='background:#333; border-radius:10px; overflow:hidden;'>";
    html += "<div id='progressBar' style='width:0%; height:30px; background:linear-gradient(90deg,#4CAF50,#8BC34A); transition:width 0.3s;'></div>";
    html += "</div>";
    html += "<p id='progressText' style='text-align:center; margin-top:10px;'>0%</p>";
    html += "</div>";

    // OTA пароль
    html += "<h3 style='margin-top:30px;'>OTA пароль</h3>";
    html += "<p style='font-size:14px;color:#888;'>Поточний пароль для Arduino IDE OTA: <code>" + getOTAPassword() + "</code></p>";
    html += "<input type='password' id='newOtaPassword' placeholder='Новий пароль (мін. 4 символи)'>";
    html += "<button class='btn' onclick='changeOtaPassword()'>Змінити пароль</button>";

    html += "</div>";

    // Backup конфігурації
    html += "<div class='card'>";
    html += "<h2>Backup конфігурації</h2>";

    // Статус backup
    BackupStatus backupStatus = getBackupStatus();
    if (backupStatus.exists) {
        html += "<p style='color:#4CAF50;'>Останній backup: ";
        if (backupStatus.valid) {
            html += String(backupStatus.size) + " bytes";
            html += " - " + backupStatus.description;
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

    // Factory Reset
    html += "<div class='card' style='border:2px solid #ff4444;'>";
    html += "<h2 style='color:#ff4444;'>Factory Reset</h2>";
    html += "<p style='color:#888;'>Скидання ВСІХ налаштувань до заводських. Ця дія незворотня!</p>";
    html += "<button class='btn' onclick='factoryReset()' style='background:#ff4444;'>Скинути до заводських</button>";
    html += "</div>";

    html += "</div>"; // container

    // JavaScript
    html += "<script>";

    // Підтвердження OTA
    html += "function confirmOTA() {";
    html += "  return confirm('УВАГА!\\n\\n";
    html += "1. Переконайтесь у стабільному живленні\\n";
    html += "2. НЕ вимикайте ESP32 під час оновлення\\n";
    html += "3. Backup буде створено автоматично\\n\\n";
    html += "Продовжити?');";
    html += "}";

    // Progress bar для upload
    html += "document.getElementById('otaForm').onsubmit = function(e) {";
    html += "  if (!confirmOTA()) { e.preventDefault(); return false; }";
    html += "  e.preventDefault();";
    html += "  var formData = new FormData(e.target);";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.upload.onprogress = function(e) {";
    html += "    if (e.lengthComputable) {";
    html += "      var percent = Math.round((e.loaded / e.total) * 100);";
    html += "      document.getElementById('progressBar').style.width = percent + '%';";
    html += "      document.getElementById('progressText').innerText = percent + '%';";
    html += "    }";
    html += "  };";
    html += "  xhr.onload = function() {";
    html += "    if (xhr.status === 200) {";
    html += "      document.getElementById('progressText').innerText = 'Успішно! Перезавантаження...';";
    html += "      setTimeout(function() { location.reload(); }, 15000);";
    html += "    } else {";
    html += "      alert('Помилка: ' + xhr.responseText);";
    html += "      document.getElementById('progress').style.display = 'none';";
    html += "    }";
    html += "  };";
    html += "  xhr.onerror = function() { alert('Помилка з\\'єднання'); };";
    html += "  document.getElementById('progress').style.display = 'block';";
    html += "  xhr.open('POST', '/update');";
    html += "  xhr.send(formData);";
    html += "};";

    // Зміна OTA паролю
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

    // Backup функції
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

// API: OTA статус
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

// API: Зміна OTA паролю
void handleOTAPasswordChange() {
    if (!checkAuth()) return;

    String body = server.arg("plain");
    JsonDocument doc;
    deserializeJson(doc, body);

    String newPassword = doc["password"];

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

// API: Створити Backup
void handleBackupCreate() {
    if (!checkAuth()) return;

    if (createBackup("Manual backup from web")) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Backup failed\"}");
    }
}

// API: Відновити Backup
void handleBackupRestore() {
    if (!checkAuth()) return;

    // Відправляємо відповідь перед restart
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Відновлення... Система перезавантажиться.\"}");

    delay(1000);
    restoreBackup();  // Ця функція робить restart
}

// API: Статус Backup
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

// API: Export Config
void handleConfigExport() {
    if (!checkAuth()) return;

    String json = exportConfigJSON();
    server.send(200, "application/json", json);
}

// API: Import Config
void handleConfigImport() {
    if (!checkAuth()) return;

    String body = server.arg("plain");

    if (importConfigJSON(body)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    }
}

// API: Factory Reset
void handleFactoryReset() {
    if (!checkAuth()) return;

    server.send(200, "application/json", "{\"success\":true,\"message\":\"Factory reset... Система перезавантажиться.\"}");

    delay(2000);
    factoryResetComplete();  // Ця функція робить restart
}

// Web OTA Upload - результат
void handleOTAUploadResult() {
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

// Web OTA Upload - обробка файлу
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

        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.println("❌ Update.begin() failed");
            Update.printError(Serial);
            setOTAInProgress(false);
            resumeCriticalTasks();
            return;
        }
        Serial.printf("📥 OTA Started: %s\n", upload.filename.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.println("❌ Update.write() failed");
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("📊 Total: %u bytes, written: %u bytes\n", upload.totalSize, Update.progress());

        if (Update.end(true)) {
            Serial.println("✅ OTA Success!");
            setOTAInProgress(false);
        } else {
            Serial.printf("❌ OTA Error: %s\n", Update.errorString());
            setOTAInProgress(false);
            resumeCriticalTasks();
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("❌ Upload aborted");
        Update.end();
        setOTAInProgress(false);
        resumeCriticalTasks();
    }
}

// ============================================================================
// ЗАВДАННЯ ВЕБ-СЕРВЕРА
// ============================================================================

void webTask(void *parameter) {
    Serial.println("✅ Веб-завдання запущено");

    while (1) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
