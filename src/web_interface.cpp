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
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <NetBIOS.h>
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

    server.onNotFound([]() {
        server.send(404, "text/plain", "Сторінка не знайдена");
    });

    server.begin();
    Serial.println();
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
// ЗАВДАННЯ ВЕБ-СЕРВЕРА
// ============================================================================

void webTask(void *parameter) {
    Serial.println("✅ Веб-завдання запущено");

    while (1) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
