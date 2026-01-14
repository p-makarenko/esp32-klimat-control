#include "web_interface.h"
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
// БЕЗПЕКА: ЕКРАНУВАННЯ HTML
// ============================================================================

// Функція для екранування HTML символів (захист від XSS)
String htmlEscape(const String& str) {
    String escaped = "";
    escaped.reserve(str.length() * 1.2); // Резервуємо трохи більше місця

    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        switch (c) {
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '&':  escaped += "&amp;";  break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&#x27;"; break;
            default:   escaped += c;        break;
        }
    }
    return escaped;
}

// ============================================================================
// БЕЗПЕКА: CSRF ЗАХИСТ
// ============================================================================

// Базовий CSRF захист через перевірку Referer
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
        // (деякі браузери не завжди відправляють Referer з форм)
        Serial.println("⚠️ CSRF: Відсутній Referer header (дозволено для форм)");
    }

    return true;
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ WI-FI ТА ВЕБ-СЕРВЕРА
// ============================================================================

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);  // Повне відключення
    delay(100);
    
    // Спочатку скануємо мережі
    int n = WiFi.scanNetworks();
    
    for (int i = 0; i < n; i++) {
        // Пропускаємо вивід у монітор
    }
    
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

    // Сортуємо по сигналу (кращий сигнал - більше RSSI)
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
                
                if (!WiFi.config(ip, gateway, subnet, dns)) {
                    // Помилка конфігурації
                }
            } else {
                // Невірний формат IP-адрес
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
            
            // Додаємо затримку для стабілізації з'єднання
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
        
        // Основна адреса
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
        
        // Перевіряємо, правильний чи пароль (по силі сигналу)
        if (WiFi.RSSI() < -80) {
            Serial.println("⚠️  Слабкий сигнал Wi-Fi!");
        }
    } else {
        // Якщо не вдалося підключитись ні до однієї мережі - запускаємо точку доступу
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
    
    // Тестуємо підключення з поміччю простої перевірки
    if (WiFi.status() == WL_CONNECTED) {
        
        // Проста перевірка - якщо у нас є IP адреса
        if (WiFi.localIP() != IPAddress(0,0,0,0)) {
            Serial.println("✅ Локальна мережа доступна");
            // Додаткова перевірка NTP
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
    
    // Налаштування веб-сервера
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
    server.on("/saveNetwork", HTTP_POST, handleSaveNetworkSettings);
    server.on("/history", HTTP_GET, handleHistoryPage);
    server.on("/history/data", HTTP_GET, handleHistoryData);
    server.on("/history/stats", HTTP_GET, handleHistoryStats);
    server.on("/history/export", HTTP_GET, handleHistoryExport);
    server.on("/help", HTTP_GET, handleHelpPage);
    server.on("/debug", HTTP_GET, handleDebugPage);
    server.on("/servo", HTTP_GET, handleServoPage);
    server.on("/servo/api", HTTP_POST, handleServoAPI);

    // Енергоконтролер маршрути
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
        color = "#00C851"; // зелений
    } else if (rssi >= -60) {
        strength = "Гарний";
        color = "#33b5e5"; // блакитний
    } else if (rssi >= -70) {
        strength = "Середній";
        color = "#ffbb33"; // жовтий
    } else if (rssi >= -80) {
        strength = "Слабкий";
        color = "#ff4444"; // червоний
    } else {
        strength = "Дуже слабкий";
        color = "#cc0000"; // темно-червоний
    }
    
    return "<span style='color:" + color + "; font-weight:600;'>" + strength + " (" + String(rssi) + " dBm)</span>";
}

String getUkraineMarquee() {
    String txt = "🇺🇦 СЛАВА УКРАЇНІ! | 💪 ГЕРОЯМ СЛАВА! | 🔥 москалі - ПІДАРИ! | ✊ Україна переможе! | 💩 рашисти - терористи | 💙💛 Разом до Перемоги! | ⚔️ Смерть ворогам! | 🛡️ ЗСУ - найсильніші! | 🤮 хуйло - злочинець! | 🗡️ За Україну! | 🚀 Крим - наш! | 🔴 moscow delenda est! | 💀 кацапи - genocid | ⚡ Слава Нації! | 🎖️ Честь і Воля! | 🔥 россія - терорист! | 💪 Воля або Смерть! | 🖕 всі москалі - ПІДАРИ на болотах! | 🇺🇦 Україна понад усе! &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

    String marquee = "<div style='text-align: center; margin-top: 20px;'>";
    marquee += "<button id='ukraineBtn' onclick='toggleUkraine()' style='background: linear-gradient(90deg, #0057B7 50%, #FFD700 50%); color: #000; border: 3px solid #000; padding: 15px 30px; font-size: 18px; font-weight: bold; border-radius: 10px; cursor: pointer; box-shadow: 0 4px 6px rgba(0,0,0,0.3);'>";
    marquee += "🇺🇦 ТИСНИ, ЯКЩО ЗА УКРАЇНУ! 🇺🇦";
    marquee += "</button>";
    marquee += "</div>";

    // ВИПРАВЛЕНО: Безперервна стрічка з дубльованим текстом
    marquee += "<div id='ukraineMarquee' style='max-height: 0; opacity: 0; background: linear-gradient(90deg, #0057B7 0%, #0057B7 50%, #FFD700 50%, #FFD700 100%); color: #000; padding: 0; margin-top: 20px; overflow: hidden; position: relative; transition: max-height 0.3s ease, opacity 0.3s ease, padding 0.3s ease;'>";
    marquee += "<div class='ukraine-scroll' style='display: inline-block; white-space: nowrap; animation: scroll-seamless 40s linear infinite; font-weight: bold; font-size: 16px;'>";
    marquee += "<span style='padding-right: 50px;'>" + txt + "</span>";
    marquee += "<span style='padding-right: 50px;'>" + txt + "</span>";  // Дубль для безперервності
    marquee += "</div>";
    marquee += "</div>";

    marquee += "<style>";
    // ВИПРАВЛЕНО: Анімація тепер працює правильно - текст повністю проходить
    marquee += "@keyframes scroll-seamless {";
    marquee += "  0% { transform: translateX(0%); }";
    marquee += "  100% { transform: translateX(-50%); }";  // -50% бо текст подвоєний
    marquee += "}";
    marquee += "#ukraineBtn:hover { transform: scale(1.05); box-shadow: 0 6px 12px rgba(0,0,0,0.4); }";
    marquee += "#ukraineBtn:active { transform: scale(0.98); }";
    // МОБІЛЬНА ОПТИМІЗАЦІЯ: швидше на малих екранах
    marquee += "@media (max-width: 768px) {";
    marquee += "  .ukraine-scroll { animation-duration: 25s !important; font-size: 14px; }";  // Швидше на телефоні
    marquee += "}";
    marquee += "@media (max-width: 480px) {";
    marquee += "  .ukraine-scroll { animation-duration: 20s !important; font-size: 13px; }";  // Ще швидше на малих телефонах
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
// ГОЛОВНА СТОРІНКА (З ІНТЕРАКТИВНОЮ ПАНЕЛЛЮ ТА КОМАНДНОЮ СТРОКОЮ)
// ============================================================================

void handleRoot() {
    // Перевірка WiFi перед відправкою великої відповіді
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Клімат-контроль</title>";
    html += "<style>";
    html += "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');";
    html += "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    html += ".header { text-align: center; margin-bottom: 30px; }";
    html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin: 30px 0; }";
    html += ".card { background: #f9f9f9; padding: 20px; border-radius: 8px; border-left: 4px solid #4CAF50; }";
    
    html += ".power-indicators { display: flex; justify-content: space-between; margin: 20px 0; background: #e8f5e9; padding: 15px; border-radius: 8px; }";
    html += ".power-item { text-align: center; flex: 1; padding: 10px; }";
    html += ".power-label { font-size: 0.9em; color: #666; margin-bottom: 5px; }";
    html += ".power-value { font-size: 1.8em; font-weight: 600; color: #2c3e50; }";
    
    html += ".command-section { background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0; }";
    html += "#commandOutput { background: white; padding: 10px; border-radius: 5px; font-family: 'SF Mono', 'Monaco', 'Consolas', monospace; min-height: 50px; white-space: pre-wrap; overflow-y: auto; max-height: 200px; }";
    html += ".quick-buttons { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 10px; }";
    html += ".quick-btn { background: #e0e0e0; padding: 8px 12px; border-radius: 4px; cursor: pointer; border: none; transition: background 0.2s; }";
    html += ".quick-btn:hover { background: #bdbdbd; }";
    
    html += ".status-value { font-size: 1.2em; font-weight: 600; }";
    html += ".temp-status { color: #e74c3c; }";
    html += ".hum-status { color: #3498db; }";
    html += ".sys-status { color: #2c3e50; }";
    
    html += ".nav { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 10px; margin: 30px 0; }";
    html += ".nav-btn { background: #4CAF50; color: white; padding: 15px; text-align: center; text-decoration: none; border-radius: 5px; display: block; transition: background 0.3s; font-weight: 500; }";
    html += ".nav-btn:hover { background: #45a049; }";
    html += ".btn { background: #2196F3; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; transition: background 0.2s; font-weight: 500; }";
    html += ".btn:hover { background: #1976D2; }";
    html += ".tooltip { position: relative; display: inline-block; cursor: help; }";
    html += ".tooltip .tooltiptext { visibility: hidden; width: 250px; background-color: #555; color: #fff; text-align: left; border-radius: 6px; padding: 10px; position: absolute; z-index: 1; bottom: 125%; left: 50%; margin-left: -125px; opacity: 0; transition: opacity 0.3s; font-size: 0.85em; line-height: 1.4; }";
    html += ".tooltip .tooltiptext::after { content: ''; position: absolute; top: 100%; left: 50%; margin-left: -5px; border-width: 5px; border-style: solid; border-color: #555 transparent transparent transparent; }";
    html += ".tooltip:hover .tooltiptext { visibility: visible; opacity: 1; }";
    html += ".help-icon { display: inline-block; width: 20px; height: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border-radius: 50%; text-align: center; line-height: 20px; font-size: 13px; font-weight: 600; margin-left: 8px; cursor: help; box-shadow: 0 2px 4px rgba(102,126,234,0.4); transition: transform 0.2s; }";
    html += ".help-icon:hover { transform: scale(1.2); box-shadow: 0 3px 6px rgba(102,126,234,0.6); }";
    html += ".faq-section { background: #f9f9f9; padding: 15px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #2196F3; }";
    html += ".faq-item { margin: 10px 0; }";
    html += ".faq-q { font-weight: 600; color: #2196F3; margin-bottom: 5px; }";
    html += ".faq-a { color: #666; font-size: 0.9em; margin-left: 15px; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>🌡️ КЛІМАТ-КОНТРОЛЬ <span onclick='showVersion()' style='cursor: pointer; color: #2196F3;'>" VERSION "</span></h1>";
    html += "<script>";
    html += "function showVersion() {";
    html += "  var msg = '" VERSION "';";
    html += "  msg += '\\n" VERSION_COMMENT "';";
    html += "  msg += '\\n\\nЗібрано: " BUILD_DATE " " BUILD_TIME "';";
    html += "  msg += '\\nРядків коду: " + String(TOTAL_CODE_LINES) + "';";
    if (FIRMWARE_SIZE_KB > 0) {
        html += "  msg += '\\nРозмір прошивки: " + String(FIRMWARE_SIZE_KB) + " KB';";
    }
    html += "  alert(msg);";
    html += "}";
    html += "</script>";
    
    // Інформація про мережу
    html += "<div style='background: #e8f5e9; padding: 10px 15px; border-radius: 5px; margin: 10px 0; font-size: 0.85em;'>";
    html += "📡 <strong>Підключено до:</strong> " + htmlEscape(WiFi.SSID()) + " | ";
    html += "<strong>IP:</strong> " + htmlEscape(WiFi.localIP().toString()) + " | ";
    html += "<strong>⏰</strong> " + htmlEscape(getTimeString());
    html += "</div>";

    // Підказка для мобільних (Android) з QR кодом
    html += "<div style='background: #fff3cd; padding: 12px 15px; border-radius: 5px; margin: 10px 0; border-left: 4px solid #ff9800; display: flex; align-items: center; gap: 15px;'>";
    html += "<div style='flex: 1;'>";
    html += "📱 <strong>Для Android:</strong> Використовуйте IP адресу:<br>";
    html += "<a href='http://" + WiFi.localIP().toString() + "' style='color: #d84315; font-weight: 600; text-decoration: underline; font-size: 1.1em;'>";
    html += "http://" + WiFi.localIP().toString();
    html += "</a>";
    html += "<br><small style='color: #856404;'>Android не підтримує klimat.local - збережіть IP в закладки!</small>";
    html += "</div>";
    // QR код через Google Charts API
    String qrUrl = "http://" + WiFi.localIP().toString();
    html += "<div style='text-align: center;'>";
    html += "<img src='https://api.qrserver.com/v1/create-qr-code/?size=100x100&data=" + qrUrl + "' alt='QR код' style='border: 2px solid #ff9800; border-radius: 5px;'>";
    html += "<br><small style='color: #856404;'>Скануй для підключення</small>";
    html += "</div>";
    html += "</div>";

    html += "<div style='display: inline-block; padding: 10px 20px; background: #4CAF50; color: white; border-radius: 20px; font-weight: 600; margin-top: 10px;'>";
    html += "Режим: <span id='currentMode'>";
    html += heatingState.emergencyMode ? "🚨 АВАРІЯ" : (heatingState.forceMode ? "⚡ ФОРСАЖ" : (heatingState.manualMode ? "✋ РУЧНИЙ" : "🤖 АВТО"));
    html += "</span></div>";

    // Контейнер для повідомлення про аварійний режим (оновлюється динамічно)
    html += "<div id='emergencyAlert'>";
    if (powerOutageState.detected || powerOutageState.emergencyHeatingActive) {
        html += "<div style='background: #ffebee; border: 2px solid #f44336; padding: 15px; border-radius: 8px; margin-top: 15px;'>";
        html += "<h3 style='color: #d32f2f; margin: 0 0 10px 0;'>🚨 АВАРІЙНИЙ РЕЖИМ АКТИВНИЙ</h3>";
        html += "<p style='margin: 5px 0;'>Система виявила відключення зовнішнього живлення</p>";
        html += "<p style='margin: 5px 0;'>Етап відновлення: <span id='emergencyStage'>" + String(powerOutageState.recoveryStage) + "</span></p>";
        html += "<button class='btn' style='background: #f44336; margin-top: 10px; padding: 12px 20px; font-weight: bold;' onclick='resetEmergency()'>🔄 СКИНУТИ АВАРІЙНИЙ РЕЖИМ</button>";
        html += "</div>";
    }
    html += "</div>";

    html += "</div>";
    
    html += "<div class='power-indicators'>";
    html += "<div class='power-item'>";
    html += "<div class='power-label'>💧 НАСОС</div>";
    html += "<div class='power-value' id='pumpPower'>" + String(round(heatingState.pumpPower * 100.0 / 255.0)) + "%</div>";
    html += "</div>";
    
    html += "<div class='power-item'>";
    html += "<div class='power-label'>🌪️ ВЕНТИЛЯТОР</div>";
    html += "<div class='power-value' id='fanPower'>" + String(round(heatingState.fanPower * 100.0 / 255.0)) + "%</div>";
    html += "</div>";
    
    html += "<div class='power-item'>";
    html += "<div class='power-label'>💨 ВИТЯЖКА</div>";
    html += "<div class='power-value' id='extractorPower'>" + String(round(heatingState.extractorPower * 100.0 / 255.0)) + "%</div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='status-grid'>";
    html += "<div class='card'>";
    html += "<h3>🌡️ ТЕМПЕРАТУРА</h3>";
    html += "<div class='status-value temp-status' id='tempRoom'>";
    html += sensorData.roomValid ? String(sensorData.tempRoom, 1) + "°C" : "🚨 ПОМИЛКА";
    html += "</div>";
    html += "<div>Теплоносій: <span id='tempCarrier'>";
    html += sensorData.carrierValid ? String(sensorData.tempCarrier, 1) + "°C" : "🚨 ПОМИЛКА";
    html += "</span></div>";
    html += "<div>BME280: <span id='tempBME'>";
    html += sensorData.bmeValid ? String(sensorData.tempBME, 1) + "°C" : "🚨 ПОМИЛКА";
    html += "</span></div>";
    html += "<div>Ціль: " + String(config.tempMin, 1) + "-" + String(config.tempMax, 1) + "°C</div>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>💧 ВОЛОГІСТЬ</h3>";
    html += "<div class='status-value hum-status' id='humidity'>";
    html += sensorData.bmeValid ? String(sensorData.humidity, 1) + "%" : "🚨 ПОМИЛКА";
    html += "</div>";
    html += "<div>Ціль: " + String(config.humidityConfig.minHumidity, 1) + "-" + String(config.humidityConfig.maxHumidity, 1) + "%</div>";
    html += "<div>Зволожувач: <span id='humidifierStatus'>" + String(humidifierState.active ? "ВКЛ" : "ВИМК") + "</span></div>";
    html += "<div>Тиск: <span id='pressure'>";
    html += sensorData.bmeValid ? String(sensorData.pressure, 1) + " hPa" : "🚨 ПОМИЛКА";
    html += "</span></div>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>⚙️ СИСТЕМА</h3>";
    html += "<div>Режим: <span class='sys-status' id='mode'>";
    html += heatingState.manualMode ? "РУЧНИЙ" : (heatingState.forceMode ? "ФОРСАЖ" : "АВТО");
    html += "</span></div>";
    html += "<div>Wi-Fi: " + htmlEscape(WiFi.SSID()) + " (" + String(WiFi.RSSI()) + " dBm)</div>";
    html += "<div>Пам'ять: <span id='memory'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
    html += "<div>Час роботи: <span id='uptime'>" + String(millis() / 1000) + " сек</span></div>";
    LoggerStats mainStats = getLoggerStats();
    html += "<div>Записів: <span id='historyCount'>" + String(mainStats.totalRecordsRAM) + "</span></div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='command-section'>";
    html += "<h3>💬 КОМАНДНАЯ СТРОКА (аналогічно Serial Monitor)</h3>";
    html += "<div style='display: flex; gap: 10px; margin-bottom: 15px;'>";
    html += "<input type='text' id='commandInput' list='commandList' placeholder='Почніть вводити або виберіть команду...' style='flex: 1; padding: 10px; border: 1px solid #ccc; border-radius: 4px;'>";
    html += "<datalist id='commandList'>";
    html += "<option value='status'>status - статус системи</option>";
    html += "<option value='menu'>menu - показати всі команди</option>";
    html += "<option value='pump '>pump XX - встановити насос 0-100%</option>";
    html += "<option value='fan '>fan XX - встановити вентилятор 0-100%</option>";
    html += "<option value='extractor '>extractor XX - встановити витяжку 0-100%</option>";
    html += "<option value='auto'>auto - автоматичний режим</option>";
    html += "<option value='manual'>manual - ручний режим</option>";
    html += "<option value='force'>force - форсований режим</option>";
    html += "<option value='tmin '>tmin XX - мін. температура</option>";
    html += "<option value='tmax '>tmax XX - макс. температура</option>";
    html += "<option value='hmin '>hmin XX - мін. вологість</option>";
    html += "<option value='hmax '>hmax XX - макс. вологість</option>";
    html += "<option value='timer on '>timer on XX - таймер на XX хвилин</option>";
    html += "<option value='timer off'>timer off - вимкнути таймер</option>";
    html += "<option value='timer set '>timer set XX YY - цикл вкл/викл</option>";
    html += "<option value='timer power '>timer power XX - потужність таймера</option>";
    html += "<option value='save'>save - зберегти налаштування</option>";
    html += "<option value='quiet'>quiet - вимк авто-статус</option>";
    html += "<option value='verbose'>verbose - увімк авто-статус</option>";
    html += "<option value='web'>web - інфо про веб-інтерфейс</option>";
    html += "<option value='reboot'>reboot - перезавантаження</option>";
    html += "<option value='servo'>servo - калібрування серво</option>";
    html += "<option value='servo move '>servo move XX - серво в кут XX</option>";
    html += "<option value='servo test'>servo test - тест серво</option>";
    html += "<option value='cooling'>cooling - переключити охолодження/обігрів</option>";
    html += "<option value='seasonal'>seasonal - сезонне відключення</option>";
    html += "<option value='reset'>reset - скинути аварійний режим</option>";
    html += "<option value='test vent'>test vent - тест вентиляції</option>";
    html += "<option value='test pump'>test pump - тест насоса</option>";
    html += "<option value='test fan'>test fan - тест вентилятора</option>";
    html += "<option value='sheets-sync'>sheets-sync - синхронізація з Google Sheets</option>";
    html += "<option value='sheets-stats'>sheets-stats - статистика синхронізації</option>";
    html += "</datalist>";
    html += "<button class='btn' onclick='executeCommand()'>ВИКОНАТИ</button>";
    html += "<button class='btn' onclick='clearOutput()' style='background: #f44336;'>ОЧИСТИТИ</button>";
    html += "</div>";
    
    html += "<div class='quick-buttons'>";
    html += "<button class='quick-btn' onclick=\"quickCommand('a30')\">a30 (Насос)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('b40')\">b40 (Вент.)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('c50')\">c50 (Вит.)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('c0')\">c0 (Вит.ВИМК)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('pump 30')\">Насос 30%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('fan 40')\">Вентилятор 40%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('extractor 50')\">Витяжка 50%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('extractor 0')\">Витяжка ВИМК</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('auto')\">АВТО</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('manual')\">РУЧНИЙ</button>";
    html += "</div>";

    html += "<div style='margin: 10px 0; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<label style='display: flex; align-items: center; gap: 8px; cursor: pointer;'>";
    html += "<input type='checkbox' id='manualLock' " + String(heatingState.manualModeLocked ? "checked" : "") + " onchange='toggleManualLock()' style='width: 18px; height: 18px;'>";
    html += "<span>🔒 Блокувати ручний режим (без автоповернення)</span>";
    html += "</label>";
    html += "</div>";

    html += "<div>";
    html += "<button class='quick-btn' onclick=\"quickCommand('status')\">СТАТУС</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('save')\">ЗБЕРЕГТИ</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('sheets-sync')\" style='background: #4285f4;'>📤 SYNC SHEETS</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('sheets-stats')\" style='background: #34a853;'>📊 STATS SHEETS</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('quiet')\" style='background: #ff9800;'>🔇 ВИМК ВИВІД</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('verbose')\" style='background: #4caf50;'>🔊 ВКЛ ВИВІД</button>";
    html += "</div>";
    
    html += "<div id='commandOutput' style='margin-top: 15px;'>> Готово до команд...</div>";
    html += "</div>";
    
    html += "<div class='nav'>";
    html += "<a href='/control' class='nav-btn'>🎛️ ПАНЕЛЬ КЕРУВАННЯ</a>";
    html += "<a href='/settings' class='nav-btn'>⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ</a>";
    html += "<a href='/learning' class='nav-btn'>🧠 СИСТЕМА НАВЧАННЯ</a>";
    html += "<a href='/energy' class='nav-btn' style='background: #f59e0b;'>⚡ ЕНЕРГОКОНТРОЛЕР</a>";
    html += "<a href='/help' class='nav-btn' style='background: #9c27b0;'>📖 ДОВІДКА</a>";
    html += "<a href='/history' class='nav-btn' style='background: #e91e63;'>📈 ГРАФІКИ</a>";
    html += "<a href='/status' class='nav-btn'>📊 JSON СТАТУС</a>";
    html += "<a href='/time' class='nav-btn'>🕒 ЧАС</a>";
    html += "<a href='/debug' class='nav-btn'>🔧 ВІДЛАДКА</a>";
    html += "</div>";
    
    html += "<div style='text-align: center; color: #777; margin-top: 20px; padding-top: 20px; border-top: 1px solid #eee;'>";
    html += "Оновлено: <span id='lastUpdate'>--:--:--</span>";
    html += " | Наступне оновлення через: <span id='nextUpdate'>3 сек</span>";
    html += "</div>";
    
    html += "</div>";
    
    html += getUkraineMarquee();
    
    html += "<script>";
    html += "let updateInterval = 3000;";
    html += "let lastUpdateTime = new Date();";
    
    html += "function updateModeButtons(activeMode) {";
    html += "  const btns = document.querySelectorAll('.mode-btn');";
    html += "  if (btns.length === 0) return;";
    html += "  btns.forEach(btn => {";
    html += "    btn.classList.remove('active');";
    html += "    if (btn.getAttribute('data-mode') === activeMode) {";
    html += "      btn.classList.add('active');";
    html += "    }";
    html += "  });";
    html += "}";
    
    html += "function updateStatus() {";
    html += "  fetch('/status')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (data.tempRoom !== undefined && !isNaN(data.tempRoom)) {";
    html += "        document.getElementById('tempRoom').textContent = data.tempRoom.toFixed(1) + '°C';";
    html += "      } else { document.getElementById('tempRoom').textContent = '🚨 ПОМИЛКА'; }";
    html += "      if (data.tempCarrier !== undefined && !isNaN(data.tempCarrier)) {";
    html += "        document.getElementById('tempCarrier').textContent = data.tempCarrier.toFixed(1) + '°C';";
    html += "      } else { document.getElementById('tempCarrier').textContent = '🚨 ПОМИЛКА'; }";
    html += "      if (data.tempBME !== undefined && !isNaN(data.tempBME)) {";
    html += "        document.getElementById('tempBME').textContent = data.tempBME.toFixed(1) + '°C';";
    html += "      } else { document.getElementById('tempBME').textContent = '🚨 ПОМИЛКА'; }";
    html += "      if (data.humidity !== undefined && !isNaN(data.humidity)) {";
    html += "        document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';";
    html += "      } else { document.getElementById('humidity').textContent = '🚨 ПОМИЛКА'; }";
    html += "      if (data.pressure !== undefined && !isNaN(data.pressure)) {";
    html += "        document.getElementById('pressure').textContent = data.pressure.toFixed(1) + ' hPa';";
    html += "      } else { document.getElementById('pressure').textContent = '🚨 ПОМИЛКА'; }";
    html += "      if (data.pumpPower !== undefined) {";
    html += "        document.getElementById('pumpPower').textContent = Math.round(data.pumpPower) + '%';";
    html += "        const pumpSlider = document.getElementById('pumpSlider');";
    html += "        if (pumpSlider) { pumpSlider.value = Math.round(data.pumpPower); }";
    html += "        const pumpValue = document.getElementById('pumpValue');";
    html += "        if (pumpValue) { pumpValue.textContent = Math.round(data.pumpPower) + '%'; }";
    html += "      }";
    html += "      if (data.fanPower !== undefined) {";
    html += "        document.getElementById('fanPower').textContent = Math.round(data.fanPower) + '%';";
    html += "        const fanSlider = document.getElementById('fanSlider');";
    html += "        if (fanSlider) { fanSlider.value = Math.round(data.fanPower); }";
    html += "        const fanValue = document.getElementById('fanValue');";
    html += "        if (fanValue) { fanValue.textContent = Math.round(data.fanPower) + '%'; }";
    html += "      }";
    html += "      if (data.extractorPower !== undefined) {";
    html += "        document.getElementById('extractorPower').textContent = Math.round(data.extractorPower) + '%';";
    html += "        const extractorSlider = document.getElementById('extractorSlider');";
    html += "        if (extractorSlider) { extractorSlider.value = Math.round(data.extractorPower); }";
    html += "        const extractorValue = document.getElementById('extractorValue');";
    html += "        if (extractorValue) { extractorValue.textContent = Math.round(data.extractorPower) + '%'; }";
    html += "      }";
    html += "      if (data.mode) {";
    html += "        document.getElementById('mode').textContent = data.mode;";
    html += "        let modeIcon = '🤖';";
    html += "        let modeKey = 'auto';";
    html += "        if (data.mode === 'АВАРІЯ') { modeIcon = '🚨'; modeKey = 'emergency'; }";
    html += "        else if (data.mode === 'ФОРСАЖ') { modeIcon = '⚡'; modeKey = 'force'; }";
    html += "        else if (data.mode === 'РУЧНИЙ') { modeIcon = '✋'; modeKey = 'manual'; }";
    html += "        document.getElementById('currentMode').textContent = modeIcon + ' ' + data.mode;";
    html += "        updateModeButtons(modeKey);";
    html += "      }";
    html += "      if (data.memory) {";
    html += "        document.getElementById('memory').textContent = data.memory + ' KB';";
    html += "      }";
    html += "      if (data.historyCount !== undefined) {";
    html += "        document.getElementById('historyCount').textContent = data.historyCount;";
    html += "      }";
    html += "      if (data.manualModeLocked !== undefined) {";
    html += "        const lockCheckbox = document.getElementById('manualLock');";
    html += "        if (lockCheckbox) { lockCheckbox.checked = data.manualModeLocked; }";
    html += "      }";
    html += "      const emergencyAlert = document.getElementById('emergencyAlert');";
    html += "      if (emergencyAlert) {";
    html += "        if (data.powerOutageActive) {";
    html += "          const currentContent = emergencyAlert.innerHTML;";
    html += "          if (!currentContent || currentContent.trim() === '') {";
    html += "            emergencyAlert.innerHTML = \"<div style='background: #ffebee; border: 2px solid #f44336; padding: 15px; border-radius: 8px; margin-top: 15px;'>\" +";
    html += "              \"<h3 style='color: #d32f2f; margin: 0 0 10px 0;'>🚨 АВАРІЙНИЙ РЕЖИМ АКТИВНИЙ</h3>\" +";
    html += "              \"<p style='margin: 5px 0;'>Система виявила відключення зовнішнього живлення</p>\" +";
    html += "              \"<p style='margin: 5px 0;'>Етап відновлення: <span id='emergencyStage'>\" + data.powerOutageStage + \"</span></p>\" +";
    html += "              \"<button class='btn' style='background: #f44336; margin-top: 10px; padding: 12px 20px; font-weight: bold;' onclick='resetEmergency()'>🔄 СКИНУТИ АВАРІЙНИЙ РЕЖИМ</button>\" +";
    html += "              \"</div>\";";
    html += "          } else {";
    html += "            const stageSpan = document.getElementById('emergencyStage');";
    html += "            if (stageSpan) stageSpan.textContent = data.powerOutageStage;";
    html += "          }";
    html += "        } else {";
    html += "          emergencyAlert.innerHTML = '';";
    html += "        }";
    html += "      }";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "      lastUpdateTime = new Date();";
    html += "    })";
    html += "    .catch(error => {";
    html += "      console.error('Помилка оновлення:', error);";
    html += "    });";
    html += "}";
    
    html += "function executeCommand() {";
    html += "  const input = document.getElementById('commandInput');";
    html += "  const command = input.value.trim();";
    html += "  if (!command) return;";
    html += "  const output = document.getElementById('commandOutput');";
    html += "  output.innerHTML += '\\n> ' + command + '\\n[Виконується...]';";
    html += "  output.scrollTop = output.scrollHeight;";
    html += "  fetch('/command', {";
    html += "    method: 'POST',";
    html += "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },";
    html += "    body: 'cmd=' + encodeURIComponent(command)";
    html += "  })";
    html += "  .then(response => response.text())";
    html += "  .then(text => {";
    html += "    output.innerHTML = output.innerHTML.replace('[Виконується...]', text);";
    html += "    output.scrollTop = output.scrollHeight;";
    html += "    input.value = '';";
    html += "    setTimeout(updateStatus, 1000);";
    html += "  })";
    html += "  .catch(error => {";
    html += "    output.innerHTML = output.innerHTML.replace('[Виконується...]', '❌ Помилка: ' + error);";
    html += "    output.scrollTop = output.scrollHeight;";
    html += "  });";
    html += "}";
    
    html += "function quickCommand(cmd) {";
    html += "  document.getElementById('commandInput').value = cmd;";
    html += "  executeCommand();";
    html += "}";
    
    html += "function clearOutput() {";
    html += "  document.getElementById('commandOutput').innerHTML = '> Готово до команд...';";
    html += "}";

    html += "function resetEmergency() {";
    html += "  if (confirm('Скинути аварійний режим і повернутись до AUTO?')) {";
    html += "    fetch('/command', {";
    html += "      method: 'POST',";
    html += "      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },";
    html += "      body: 'cmd=reset'";
    html += "    }).then(response => response.text()).then(text => {";
    html += "      alert('✅ ' + text);";
    html += "      setTimeout(function() { location.reload(); }, 500);";
    html += "    }).catch(error => {";
    html += "      alert('❌ Помилка: ' + error);";
    html += "    });";
    html += "  }";
    html += "}";

    html += "function toggleManualLock() {";
    html += "  fetch('/command', {";
    html += "    method: 'POST',";
    html += "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },";
    html += "    body: 'cmd=lock'";
    html += "  }).then(response => response.text()).then(text => {";
    html += "    console.log('Lock toggle:', text);";
    html += "  }).catch(error => {";
    html += "    console.error('Lock toggle error:', error);";
    html += "  });";
    html += "}";

    html += "document.getElementById('commandInput').addEventListener('keypress', function(e) {";
    html += "  if (e.key === 'Enter') executeCommand();";
    html += "});";
    
    html += "function updateNextUpdateTimer() {";
    html += "  const now = new Date();";
    html += "  const timeSinceUpdate = now - lastUpdateTime;";
    html += "  const timeLeft = Math.max(0, updateInterval - timeSinceUpdate);";
    html += "  document.getElementById('nextUpdate').textContent = Math.round(timeLeft/1000) + ' сек';";
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
    
    // Перевірка WiFi перед відправкою
    if (WiFi.status() == WL_CONNECTED) {
        server.send(200, "text/html", html);
    }
}

// ============================================================================
// JSON API ДЛЯ СТАТУСУ
// ============================================================================

void handleStatus() {
    JsonDocument doc;
    
    doc["tempRoom"] = sensorData.roomValid ? sensorData.tempRoom : (float)NAN;
    doc["tempCarrier"] = sensorData.carrierValid ? sensorData.tempCarrier : (float)NAN;
    doc["tempBME"] = sensorData.bmeValid ? sensorData.tempBME : (float)NAN;
    doc["humidity"] = sensorData.bmeValid ? sensorData.humidity : (float)NAN;
    doc["pressure"] = sensorData.bmeValid ? sensorData.pressure : (float)NAN;
    doc["pumpPower"] = round(heatingState.pumpPower * 100.0 / 255.0);
    doc["fanPower"] = round(heatingState.fanPower * 100.0 / 255.0);
    doc["extractorPower"] = round(heatingState.extractorPower * 100.0 / 255.0);
    doc["extractorTimer"] = config.extractorTimer.enabled;
    
    if (heatingState.emergencyMode) doc["mode"] = "АВАРІЯ";
    else if (heatingState.forceMode) doc["mode"] = "ФОРСАЖ";
    else if (heatingState.manualMode) doc["mode"] = "РУЧНИЙ";
    else doc["mode"] = "АВТО";

    // Інформація про аварію теплоносія
    doc["powerOutageActive"] = powerOutageState.emergencyHeatingActive || powerOutageState.detected;
    doc["powerOutageStage"] = powerOutageState.recoveryStage;

    // Інформація про блокування ручного режиму
    doc["manualModeLocked"] = heatingState.manualModeLocked;

    doc["time"] = getTimeString();
    doc["memory"] = ESP.getFreeHeap() / 1024;
    doc["uptime"] = millis() / 1000;
    LoggerStats jsonStats = getLoggerStats();
    doc["historyCount"] = jsonStats.totalRecordsRAM;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ============================================================================
// ОБРОБКА КОМАНД ІЗ ВЕБ-ІНТЕРФЕЙСУ
// ============================================================================

void handleWebCommand() {
    if (!checkAuth()) return;

    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    // БЕЗПЕКА: Перевірка CSRF
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
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

    // Перевірка режимів СПОЧАТКУ (щоб "auto" не розпізнавався як "a0")
    if (lowerCmd == "auto") {
        heatingState.manualMode = false;
        heatingState.manualModeLocked = false;
        heatingState.manualModeStartTime = 0;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        // Також скидаємо стан аварії при переході в AUTO
        powerOutageState.detected = false;
        powerOutageState.emergencyHeatingActive = false;
        powerOutageState.recoveryStage = 0;
        return "✅ Режим: АВТОМАТИЧНИЙ";
    }
    else if (lowerCmd == "manual") {
        heatingState.manualMode = true;
        heatingState.manualModeStartTime = millis();
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        return "✅ Режим: РУЧНИЙ (автоповернення через 15 хв)";
    }
    else if (lowerCmd == "lock") {
        heatingState.manualModeLocked = !heatingState.manualModeLocked;
        return String("🔒 Блокування ручного режиму: ") +
               (heatingState.manualModeLocked ? "УВІМКНЕНО" : "ВИМКНЕНО");
    }
    else if (lowerCmd == "force") {
        heatingState.forceMode = true;
        heatingState.manualMode = false;
        heatingState.emergencyMode = false;
        return "🔧 Режим: ФОРСАЖ";
    }
    else if (lowerCmd == "emergency") {
        heatingState.emergencyMode = true;
        heatingState.manualMode = false;
        heatingState.forceMode = false;
        return "⚠️ Режим: АВАРІЯ";
    }
    else if (lowerCmd == "reset" || lowerCmd == "reset_emergency") {
        // Скидання аварійного режиму
        powerOutageState.detected = false;
        powerOutageState.emergencyHeatingActive = false;
        powerOutageState.recoveryStage = 0;
        powerOutageState.autoExitCheckStart = 0;
        powerOutageState.tempAtAutoExitStart = 0;
        heatingState.emergencyMode = false;
        heatingState.forceMode = false;
        heatingState.manualMode = false;
        return "✅ Аварійний режим скинуто. Повернення до AUTO";
    }
    
    // Компактні команди (a50, b40, c30) - автоматично перемикають в РУЧНИЙ режим
    if (lowerCmd.length() >= 2 && lowerCmd.length() <= 4) {
        char device = lowerCmd[0];
        if (device == 'a' || device == 'b' || device == 'c') {
            String valueStr = lowerCmd.substring(1);
            int value = valueStr.toInt();
            
            if (value >= 0 && value <= 100) {
                heatingState.manualMode = true;
                heatingState.manualModeStartTime = millis();
                heatingState.forceMode = false;
                heatingState.emergencyMode = false;
                switch(device) {
                    case 'a':
                        setPumpPercent(value);
                        return "✅ Насос (A): " + String(value) + "% [РУЧНИЙ]";
                    case 'b':
                        setFanPercent(value);
                        return "✅ Вентилятор (B): " + String(value) + "% [РУЧНИЙ]";
                    case 'c':
                        setExtractorPercent(value);
                        return "✅ Витяжка (C): " + String(value) + "% [РУЧНИЙ]";
                }
            }
        }
    }
    
    if (lowerCmd.startsWith("pump ")) {
        heatingState.manualMode = true;
        heatingState.manualModeStartTime = millis();
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        int percent = cmd.substring(5).toInt();
        percent = constrain(percent, 0, 100);
        setPumpPercent(percent);
        return "✅ Насос: " + String(percent) + "% [РУЧНИЙ]";
    }
    else if (lowerCmd.startsWith("fan ")) {
        heatingState.manualMode = true;
        heatingState.manualModeStartTime = millis();
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        int percent = cmd.substring(4).toInt();
        percent = constrain(percent, 0, 100);
        setFanPercent(percent);
        return "✅ Вентилятор: " + String(percent) + "% [РУЧНИЙ]";
    }
    else if (lowerCmd.startsWith("extractor ")) {
        heatingState.manualMode = true;
        heatingState.manualModeStartTime = millis();
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        int percent = cmd.substring(10).toInt();
        percent = constrain(percent, 0, 100);
        setExtractorPercent(percent);
        return "✅ Витяжка: " + String(percent) + "% [РУЧНИЙ]";
    }
    else if (lowerCmd.startsWith("timer on ")) {
        int minutes = cmd.substring(9).toInt();
        minutes = constrain(minutes, 1, 240);
        config.extractorTimer.enabled = true;
        config.extractorTimer.onMinutes = minutes;
        config.extractorTimer.offMinutes = 0;
        config.extractorTimer.cycleStart = millis();
        setExtractorPercent(config.extractorTimer.powerPercent);
        return "⏰ Таймер: ВКЛ на " + String(minutes) + " хв (" + String(config.extractorTimer.powerPercent) + "%)";
    }
    else if (lowerCmd == "timer off") {
        config.extractorTimer.enabled = false;
        setExtractorPercent(0);
        return "⏰ Таймер: ВИМК";
    }
    else if (lowerCmd.startsWith("timer set ")) {
        String params = cmd.substring(10);
        int spaceIndex = params.indexOf(' ');
        if (spaceIndex > 0) {
            String onTime = params.substring(0, spaceIndex);
            String offTime = params.substring(spaceIndex + 1);
            
            // Парсимо формат M:S (хвилини:секунди) або просто M (хвилини)
            int onMin = 0, onSec = 0, offMin = 0, offSec = 0;
            
            int colonOn = onTime.indexOf(':');
            if (colonOn > 0) {
                onMin = onTime.substring(0, colonOn).toInt();
                onSec = onTime.substring(colonOn + 1).toInt();
            } else {
                onMin = onTime.toInt();
            }
            
            int colonOff = offTime.indexOf(':');
            if (colonOff > 0) {
                offMin = offTime.substring(0, colonOff).toInt();
                offSec = offTime.substring(colonOff + 1).toInt();
            } else {
                offMin = offTime.toInt();
            }
            
            // Обмеження значень
            onMin = constrain(onMin, 0, 120);
            onSec = constrain(onSec, 0, 59);
            offMin = constrain(offMin, 0, 120);
            offSec = constrain(offSec, 0, 59);
            
            config.extractorTimer.onMinutes = onMin;
            config.extractorTimer.onSeconds = onSec;
            config.extractorTimer.offMinutes = offMin;
            config.extractorTimer.offSeconds = offSec;
            config.extractorTimer.enabled = true;
            config.extractorTimer.cycleStart = millis();
            
            String result = "⏰ Таймер: ";
            if (onMin > 0) result += String(onMin) + " хв ";
            if (onSec > 0) result += String(onSec) + " сек ";
            result += "ВКЛ / ";
            if (offMin > 0) result += String(offMin) + " хв ";
            if (offSec > 0) result += String(offSec) + " сек ";
            result += "ВИМК";
            return result;
        }
        return "❌ Формат: timer set <M:S або M> <M:S або M>";
    }
    else if (lowerCmd.startsWith("timer power ")) {
        int power = cmd.substring(12).toInt();
        power = constrain(power, 10, 100);
        config.extractorTimer.powerPercent = power;
        return "⏰ Потужність таймера: " + String(power) + "%";
    }
    else if (lowerCmd.startsWith("tmin ")) {
        float temp = cmd.substring(5).toFloat();
        config.tempMin = temp;
        saveConfiguration();
        return "🌡️ Мін. температура: " + String(temp, 1) + "°C";
    }
    else if (lowerCmd.startsWith("tmax ")) {
        float temp = cmd.substring(5).toFloat();
        config.tempMax = temp;
        saveConfiguration();
        return "🌡️ Макс. температура: " + String(temp, 1) + "°C";
    }
    else if (lowerCmd.startsWith("temp ")) {
        float temp = cmd.substring(5).toFloat();
        config.tempMin = temp;
        config.tempMax = temp + 1.0f;
        saveConfiguration();
        return "🌡️ Температура: " + String(temp, 1) + "-" + String(temp + 1.0f, 1) + "°C";
    }
    else if (lowerCmd.startsWith("hmin ")) {
        float hum = cmd.substring(5).toFloat();
        config.humidityConfig.minHumidity = hum;
        saveConfiguration();
        return "💧 Мін. вологість: " + String(hum, 1) + "%";
    }
    else if (lowerCmd.startsWith("hmax ")) {
        float hum = cmd.substring(5).toFloat();
        config.humidityConfig.maxHumidity = hum;
        saveConfiguration();
        return "💧 Макс. вологість: " + String(hum, 1) + "%";
    }
    else if (lowerCmd.startsWith("hum ")) {
        float hum = cmd.substring(4).toFloat();
        config.humidityConfig.minHumidity = hum;
        config.humidityConfig.maxHumidity = hum + 5.0f;
        saveConfiguration();
        return "💧 Вологість: " + String(hum, 1) + "-" + String(hum + 5.0f, 1) + "%";
    }
    else if (lowerCmd == "status") {
        return "📊 Статус оновлений (перевірте дані вище)";
    }
    else if (lowerCmd == "save") {
        saveConfiguration();
        return "💾 Налаштування збережені";
    }
    else if (lowerCmd == "quiet") {
        config.autoStatusEnabled = false;
        saveConfiguration();
        return "✅ Автоматичний вивід статусу ВИМКНЕНО";
    }
    else if (lowerCmd == "verbose") {
        config.autoStatusEnabled = true;
        saveConfiguration();
        return "✅ Автоматичний вивід статусу УВІМКНЕНО";
    }
    else if (lowerCmd == "reboot") {
        server.send(200, "text/plain", "🔄 Перезавантаження...");
        delay(1000);
        ESP.restart();
        return "";
    }
    else if (lowerCmd == "test vent") {
        testVentilation();
        return "🔧 Тест вентиляції виконаний";
    }
    else if (lowerCmd == "test pump") {
        setPumpPercent(50);
        delay(5000);
        setPumpPercent(0);
        return "🔧 Тест насоса виконаний (5 сек на 50%)";
    }
    else if (lowerCmd == "test fan") {
        setFanPercent(50);
        delay(5000);
        setFanPercent(0);
        return "🔧 Тест вентилятора виконаний (5 сек на 50%)";
    }
    else if (lowerCmd == "web") {
        return "🌐 Веб-інтерфейс: http://" + WiFi.localIP().toString();
    }
    else if (lowerCmd.startsWith("mode ")) {
        String mode = cmd.substring(5);
        mode.toLowerCase();
        if (mode == "auto") {
            heatingState.manualMode = false;
            heatingState.manualModeLocked = false;
            heatingState.manualModeStartTime = 0;
            heatingState.forceMode = false;
            return "✅ Режим: АВТОМАТИЧНИЙ";
        }
        else if (mode == "manual") {
            heatingState.manualMode = true;
            heatingState.manualModeStartTime = millis();
            heatingState.forceMode = false;
            return "✅ Режим: РУЧНИЙ (автоповернення через 15 хв)";
        }
        return "❌ Невідомий режим: " + mode;
    }
    else if (lowerCmd.startsWith("servo move ")) {
        int angle = cmd.substring(11).toInt();
        angle = constrain(angle, 0, 180);
        moveServoSmooth(angle);
        return "✅ Серво переміщено в " + String(angle) + "°";
    }
    else if (lowerCmd == "servo set closed") {
        config.servoClosedAngle = ventState.currentAngle;
        saveConfiguration();
        return "✅ Закрите положення: " + String(config.servoClosedAngle) + "°";
    }
    else if (lowerCmd == "servo set open") {
        config.servoOpenAngle = ventState.currentAngle;
        saveConfiguration();
        return "✅ Відкрите положення: " + String(config.servoOpenAngle) + "°";
    }
    else if (lowerCmd == "servo test") {
        moveServoSmooth(config.servoOpenAngle);
        delay(2000);
        moveServoSmooth(config.servoClosedAngle);
        return "✅ Тест серво виконано";
    }
    else if (lowerCmd == "servo") {
        return "⚙️ Серво: поточне=" + String(ventState.currentAngle) + "° закрито=" + String(config.servoClosedAngle) + "° відкрито=" + String(config.servoOpenAngle) + "°";
    }
    else if (lowerCmd.startsWith("learn ")) {
        String learnCmd = cmd.substring(6);
        processLearningCommand(learnCmd);
        return "✅ Команда навчання виконана";
    }
    else if (lowerCmd == "sheets-sync" || lowerCmd == "sheets sync") {
        Serial.println("📤 Веб: Запуск синхронізації з Google Sheets...");
        if (syncToGoogleSheets()) {
            return "✅ Дані успішно відправлено в Google Sheets";
        } else {
            return "❌ Помилка синхронізації з Google Sheets";
        }
    }
    else if (lowerCmd == "sheets-stats" || lowerCmd == "sheets stats") {
        printSyncInfo();
        SyncStats stats = getSyncStats();
        String response = "📊 Статистика синхронізації:\n";
        response += "Timestamp: " + String(stats.lastSentTimestamp) + "\n";
        response += "Відправлено: " + String(stats.totalRecordsSent) + "\n";
        response += "Помилок: " + String(stats.failedSyncs);
        return response;
    }
    else {
        return "❌ Невідома команда: " + cmd + "\n📋 Доступні команди:\npump/fan/extractor XX, timer on/off, tmin/tmax/temp/hmin/hmax/hum XX,\nauto/manual/force, servo, mode, status, save, quiet, verbose, test, web, reboot, sheets-sync, sheets-stats";
    }
}

// ============================================================================
// СТОРІНКА НАЛАШТУВАНЬ WI-FI (ДОДАЄМО!)
// ============================================================================

void handleWiFiSettingsPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Налаштування Wi-Fi</title>";
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
    html += ".network-ssid { font-weight: 600; }";
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
    html += "var savedNetworks = {};";

    // БЕЗПЕКА: Не показуємо паролі в JavaScript!
    // Замість цього зберігаємо тільки список SSID збережених мереж
    preferences.begin("wifi", true);
    int networkCount = preferences.getInt("netCount", 0);
    for (int i = 0; i < networkCount; i++) {
        String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (savedSSID.length() > 0) {
            // Зберігаємо тільки true (без пароля!)
            html += "savedNetworks['" + htmlEscape(savedSSID) + "'] = true;";
        }
    }
    preferences.end();

    html += "function showManualForm() {";
    html += "  document.getElementById('manualForm').classList.remove('hidden');";
    html += "  document.getElementById('manualForm').scrollIntoView({ behavior: 'smooth' });";
    html += "}";
    html += "function connectToNetwork(ssid, encrypted) {";
    html += "  document.getElementById('connectSsid').value = ssid;";
    html += "  document.getElementById('connectEncrypted').value = encrypted;";
    html += "  if (encrypted === 'true') {";
    html += "    document.getElementById('manualForm').classList.remove('hidden');";
    html += "    if (savedNetworks[ssid]) {";
    // БЕЗПЕКА: Не заповнюємо пароль автоматично - тільки підказуємо
    html += "      document.getElementById('connectPassword').placeholder = '🔑 Мережа збережена - введіть пароль';";
    html += "    } else {";
    html += "      document.getElementById('connectPassword').placeholder = 'Введіть пароль';";
    html += "    }";
    html += "    document.getElementById('connectPassword').value = '';";
    html += "    document.getElementById('connectPassword').focus();";
    html += "  } else {";
    html += "    document.getElementById('connectPassword').value = '';";
    html += "    document.getElementById('wifiForm').submit();";
    html += "  }";
    html += "}";
    html += "function startScan() {";
    html += "  document.getElementById('scanBtn').innerHTML = '🔍 Сканування...';";
    html += "  document.getElementById('scanBtn').disabled = true;";
    html += "  window.location.href = '/scan-wifi';";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>📶 Налаштування Wi-Fi</h1>";
    html += "<p><a href='/' class='back-link'>← На головну</a></p>";
    
    // Поточне підключення
    html += "<div class='section'>";
    html += "<h2>Поточне підключення</h2>";
    html += "<div class='current-info'>";
    html += "<p><strong>SSID:</strong> " + htmlEscape(WiFi.SSID()) + "</p>";
    html += "<p><strong>IP адреса:</strong> " + htmlEscape(WiFi.localIP().toString()) + "</p>";
    html += "<p><strong>MAC адреса:</strong> " + htmlEscape(WiFi.macAddress()) + "</p>";
    html += "<p><strong>Сигнал:</strong> " + wifiStrengthToHTML(WiFi.RSSI()) + "</p>";
    html += "<p><strong>Статус:</strong> " + String(WiFi.status() == WL_CONNECTED ? "✅ Підключено" : "❌ Відключено") + "</p>";
    html += "</div>";
    html += "</div>";
    
    // Сканування мереж
    html += "<div class='section'>";
    html += "<h2>Доступні мережі</h2>";
    html += "<button id='scanBtn' class='btn btn-scan' onclick='startScan()'>";
    html += "📡 Сканувати мережі";
    html += "</button>";
    html += "<div class='network-list'>";
    
    // Показати результати сканування якщо є параметр scanned
    if (server.hasArg("scanned")) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            Serial.printf("Відображення %d знайдених мереж\n", n);
            
            for (int i = 0; i < n; i++) {
                String ssid = WiFi.SSID(i);
                String ssidEscaped = htmlEscape(ssid);
                int rssi = WiFi.RSSI(i);
                bool encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

                html += "<div class='network-item'>";
                html += "<div>";
                html += "<div class='network-ssid'>" + ssidEscaped + "</div>";
                html += "<div class='network-details'>";
                html += "Сигнал: " + String(rssi) + " dBm | ";
                html += encrypted ? "🔒 Захищена" : "🔓 Відкрита";
                html += "</div>";
                html += "</div>";
                // Екрануємо для JavaScript атрибуту onclick
                html += "<button class='connect-btn' onclick=\"connectToNetwork('" + ssidEscaped + "', '" + String(encrypted ? "true" : "false") + "')\">Підключити</button>";
                html += "</div>";
            }
            
            WiFi.scanDelete();
        } else {
            html += "<div class='network-item'>";
            html += "<div class='network-ssid'>Сканування...</div>";
            html += "</div>";
        }
    } else {
        // Показати всі збережені мережі з preferences
        preferences.begin("wifi", true);
        int networkCount = preferences.getInt("netCount", 0);
        
        if (networkCount > 0) {
            html += "<div class='status-info' style='margin-bottom: 10px;'>";
            html += "💾 Збережено мереж: " + String(networkCount);
            html += "</div>";
            
            for (int i = 0; i < networkCount; i++) {
                String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
                if (savedSSID.length() > 0) {
                    String savedSSIDEscaped = htmlEscape(savedSSID);
                    html += "<div class='network-item'>";
                    html += "<div>";
                    html += "<div class='network-ssid'>📱 " + savedSSIDEscaped + "</div>";
                    html += "<div class='network-details'>Збережена мережа #" + String(i+1) + "</div>";
                    html += "</div>";
                    html += "<button class='connect-btn' onclick=\"connectToNetwork('" + savedSSIDEscaped + "', 'true')\">Підключити</button>";
                    html += "</div>";
                }
            }
        } else {
            // Перевірка старого формату (одна мережа)
            String savedSSID = preferences.getString("ssid", "");
            if (savedSSID.length() > 0) {
                String savedSSIDEscaped = htmlEscape(savedSSID);
                html += "<div class='network-item'>";
                html += "<div>";
                html += "<div class='network-ssid'>📱 " + savedSSIDEscaped + "</div>";
                html += "<div class='network-details'>Збережена мережа (старий формат)</div>";
                html += "</div>";
                html += "<button class='connect-btn' onclick=\"connectToNetwork('" + savedSSIDEscaped + "', 'true')\">Підключити</button>";
                html += "</div>";
            }
        }
        
        preferences.end();
        
        html += "<div class='network-item'>";
        html += "<div>";
        html += "<div class='network-ssid'>📡 Натисніть кнопку сканування</div>";
        html += "<div class='network-details'>Щоб побачити доступні мережі</div>";
        html += "</div>";
        html += "</div>";
    }
    
    html += "</div>"; // network-list
    html += "</div>"; // section
    
    // Форма підключення (схована за умовчанням)
    html += "<div class='section'>";
    html += "<h2>Підключення до мережі</h2>";
    
    // Вивід сповіщень про помилки/успіх
    if (server.hasArg("error")) {
        html += "<div class='status-message status-error'>";
        html += "❌ " + server.arg("error");
        html += "</div>";
    }
    if (server.hasArg("success")) {
        html += "<div class='status-message status-success'>";
        html += "✅ " + server.arg("success");
        html += "</div>";
    }
    
    html += "<form id='wifiForm' method='POST' action='/save-wifi'>";
    html += "<div id='manualForm' class='hidden'>";
    html += "<div class='form-group'>";
    html += "<label for='connectSsid'>Назва мережі (SSID):</label>";
    html += "<input type='text' id='connectSsid' name='ssid' placeholder='Введіть назву мережі' title='Заповніть це поле' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='connectPassword'>Пароль:</label>";
    html += "<input type='password' id='connectPassword' name='password' placeholder='Введіть пароль' title='Введіть пароль для мережі'>";
    html += "<small>Залиште пустим для відкритих мереж</small>";
    html += "</div>";
    
    html += "<input type='hidden' id='connectEncrypted' name='encrypted' value='true'>";
    
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='save' checked> Зберегти налаштування</label>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-save'>🔗 Підключитися</button>";
    html += "<button type='button' class='btn' onclick=\"document.getElementById('manualForm').classList.add('hidden')\">Скасувати</button>";
    html += "</div>"; // manualForm
    html += "</form>";
    
    // Кнопка для показу форми
    html += "<button class='btn' onclick=\"showManualForm()\" id='showFormBtn'>📝 Ввести дані вручну</button>";
    html += "<a href='/wifi' class='btn' style='display: inline-block; text-decoration: none; margin-left: 10px;'>⚙️ Розширені налаштування (IP, DNS, Авторизація)</a>";
    
    html += "</div>"; // section
    
    // Форма точки доступу
    html += "<div class='section'>";
    html += "<h2>Точка доступу</h2>";
    html += "<div class='current-info'>";
    
    WiFiMode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        html += "<p><strong>Статус:</strong> ✅ Активна</p>";
        html += "<p><strong>SSID точки доступу:</strong> ClimateControl</p>";
        html += "<p><strong>IP адреса:</strong> " + WiFi.softAPIP().toString() + "</p>";
        html += "<p><strong>Пароль:</strong> 12345678</p>";
        html += "<p><em>Точка доступу активна, якщо не вдалося підключитись до Wi-Fi мережі</em></p>";
    } else {
        html += "<p><strong>Статус:</strong> ❌ Не активна</p>";
        html += "<p><em>Точка доступу буде автоматично запущена при відсутності Wi-Fi підключення</em></p>";
    }
    
    html += "</div>";
    html += "</div>";
    
    html += "<div style='text-align: center; margin-top: 30px;'><a href='/' style='background: #4CAF50; color: white; padding: 12px 24px; text-decoration: none; border-radius: 5px; display: inline-block;'>← На головну</a></div>";
    
    html += "</div>"; // container
    html += getUkraineMarquee();
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА СКАНУВАННЯ WI-FI
// ============================================================================

void handleScanWiFi() {
    Serial.println("📶 Сканування Wi-Fi мереж...");

    // Не відключаємось від поточної мережі - ESP32 може сканувати в режимі STA
    int n = WiFi.scanNetworks(false, false);  // async=false, show_hidden=false
    Serial.printf("Знайдено %d мереж\n", n);
    
    // Перенаправляємо на сторінку налаштувань з параметром scanned
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='0;url=/wifi-settings?scanned=true'>";
    html += "<title>Сканування завершено</title>";
    html += "</head><body>";
    html += "<p>Сканування завершено. Перенаправлення...</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// ЗБЕРЕЖЕННЯ НАЛАШТУВАНЬ WI-FI
// ============================================================================

void handleSaveWiFiSettings() {
    // БЕЗПЕКА: Перевірка CSRF
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
        return;
    }

    String response = "";
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    bool saveSettings = server.hasArg("save");

    Serial.println("💾 Збереження налаштувань Wi-Fi:");
    Serial.println("  SSID: " + ssid);
    Serial.println("  Зберегти: " + String(saveSettings ? "Так" : "Ні"));

    if (ssid.length() == 0) {
        server.send(400, "text/plain", "Помилка: SSID не може бути пустим");
        return;
    }
    
    // Зберігаємо в Preferences
    if (saveSettings) {
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        Serial.println("✅ Налаштування збережені в Preferences");
    }
    
    // Пробуємо підключитись
    WiFi.disconnect(true);
    delay(1000);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    Serial.println("🔗 Пробуємо підключитись до: " + ssid);
    
    // Чекаємо підключення
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
    
    response = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    response += "<meta http-equiv='refresh' content='5;url=/'>";
    response += "<title>Підключення Wi-Fi</title>";
    response += "<style>";
    response += "body { font-family: Arial; text-align: center; padding: 50px; }";
    response += ".success { color: #2ecc71; }";
    response += ".error { color: #e74c3c; }";
    response += "</style>";
    response += "</head><body>";
    
    if (connected) {
        response += "<h1 class='success'>✅ Успішно підключено!</h1>";
        response += "<p>Мережа: " + ssid + "</p>";
        response += "<p>IP адреса: " + WiFi.localIP().toString() + "</p>";
        response += "<p>Сигнал: " + String(WiFi.RSSI()) + " dBm</p>";
        response += "<p>Перенаправлення на головну сторінку...</p>";
        
        // Зберігаємо мережу в список збережених мереж (до 5 мереж)
        preferences.begin("wifi", false);
        
        // Читаємо поточну кількість збережених мереж
        int networkCount = preferences.getInt("netCount", 0);
        
        // Перевіряємо чи мережа вже є
        bool exists = false;
        for (int i = 0; i < networkCount; i++) {
            String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
            if (savedSSID == ssid) {
                // Оновлюємо пароль
                preferences.putString(("pass" + String(i)).c_str(), password);
                exists = true;
                break;
            }
        }
        
        // Якщо мережі нема - додаємо
        if (!exists && networkCount < 5) {
            preferences.putString(("ssid" + String(networkCount)).c_str(), ssid);
            preferences.putString(("pass" + String(networkCount)).c_str(), password);
            networkCount++;
            preferences.putInt("netCount", networkCount);
        }
        
        // Зберігаємо останню підключену мережу (для сумісності)
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        
        Serial.println("\n✅ Підключення успішно!");
        Serial.println("  IP: " + WiFi.localIP().toString());
        Serial.println("  RSSI: " + String(WiFi.RSSI()) + " dBm");
        Serial.printf("  Збережено мереж: %d\n", networkCount);
    } else {
        response += "<h1 class='error'>❌ Не вдалося підключитись</h1>";
        response += "<p>Мережа: " + ssid + "</p>";
        response += "<p>Перевірте пароль і впевніться, що мережа доступна.</p>";
        response += "<p>Система повернеться в режим точки доступу.</p>";
        
        // Запускаємо точку доступу
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ClimateControl", "12345678");
        
        Serial.println("\n❌ Не вдалося підключитись, запускаємо точку доступу");
    }
    
    response += "</body></html>";
    
    server.send(200, "text/html", response);
}

// ============================================================================
// СТОРІНКА НАЛАШТУВАНЬ СИСТЕМИ
// ============================================================================

void handleSettingsPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Налаштування системи</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 900px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    html += "h1 { color: #2c3e50; border-bottom: 2px solid #4CAF50; padding-bottom: 10px; margin-bottom: 20px; }";
    html += ".form-group { margin-bottom: 20px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #34495e; }";
    html += "input[type='number'], input[type='text'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; font-size: 16px; }";
    html += ".section { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 25px; border-left: 4px solid #3498db; }";
    html += ".section h3 { margin-top: 0; color: #2980b9; }";
    html += ".btn { background: #4CAF50; color: white; padding: 12px 25px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; transition: background 0.3s; }";
    html += ".btn:hover { background: #45a049; }";
    html += ".btn-secondary { background: #3498db; margin-left: 10px; }";
    html += ".btn-secondary:hover { background: #2980b9; }";
    html += ".back-link { display: inline-block; margin-top: 20px; color: #7f8c8d; text-decoration: none; }";
    html += ".back-link:hover { color: #34495e; }";

    // ВКЛАДКИ (TABS)
    html += ".tabs { display: flex; gap: 5px; margin-bottom: 20px; flex-wrap: wrap; border-bottom: 2px solid #ddd; }";
    html += ".tab { padding: 12px 20px; background: #e0e0e0; border: none; border-radius: 8px 8px 0 0; cursor: pointer; font-size: 15px; font-weight: 500; transition: all 0.3s; color: #555; }";
    html += ".tab:hover { background: #d0d0d0; }";
    html += ".tab.active { background: #4CAF50; color: white; box-shadow: 0 -2px 8px rgba(76, 175, 80, 0.3); }";
    html += ".tab-content { display: none; animation: fadeIn 0.3s; }";
    html += ".tab-content.active { display: block; }";
    html += "@keyframes fadeIn { from { opacity: 0; transform: translateY(-10px); } to { opacity: 1; transform: translateY(0); } }";

    // МОБІЛЬНА ОПТИМІЗАЦІЯ
    html += "@media (max-width: 768px) {";
    html += "  body { margin: 10px; }";
    html += "  .container { padding: 15px; }";
    html += "  h1 { font-size: 1.5em; }";
    html += "  .tabs { gap: 3px; }";
    html += "  .tab { padding: 10px 12px; font-size: 13px; }";
    html += "  input[type='number'], input[type='text'] { font-size: 16px; padding: 12px; }";
    html += "  .btn { width: 100%; margin: 5px 0; }";
    html += "  .btn-secondary { margin-left: 0; }";
    html += "}";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ СИСТЕМИ</h1>";
    html += "<p><a href='/' class='back-link'>← На головну</a></p>";

    // ВКЛАДКИ
    html += "<div class='tabs'>";
    html += "<button type='button' class='tab active' onclick='switchTab(0)'>🌡️ Клімат</button>";
    html += "<button type='button' class='tab' onclick='switchTab(1)'>🔧 Пристрої</button>";
    html += "<button type='button' class='tab' onclick='switchTab(2)'>⏰ Таймер</button>";
    html += "<button type='button' class='tab' onclick='switchTab(3)'>🔄 Сезон</button>";
    html += "<button type='button' class='tab' onclick='switchTab(4)'>🚨 Аварія</button>";
    html += "<button type='button' class='tab' onclick='switchTab(5)'>⚙️ Система</button>";
    html += "<button type='button' class='tab' onclick='switchTab(6)'>🎯 Серво</button>";
    html += "<button type='button' class='tab' onclick='switchTab(7)'>📶 WiFi</button>";
    html += "</div>";

    html += "<form method='POST' action='/settings'>";

    // TAB 0: КЛІМАТ
    html += "<div class='tab-content active' id='tab0'>";
    html += "<div class='section'>";
    html += "<h3>🌡️ ТЕМПЕРАТУРА</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна температура (°C):</label>";
    html += "<input type='number' step='0.1' name='tempMin' value='" + String(config.tempMin, 1) + "' min='10' max='40'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальна температура (°C):</label>";
    html += "<input type='number' step='0.1' name='tempMax' value='" + String(config.tempMax, 1) + "' min='10' max='40'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h3>💧 ВОЛОГІСТЬ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна вологість (%):</label>";
    html += "<input type='number' step='0.1' name='humMin' value='" + String(config.humidityConfig.minHumidity, 1) + "' min='30' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальна вологість (%):</label>";
    html += "<input type='number' step='0.1' name='humMax' value='" + String(config.humidityConfig.maxHumidity, 1) + "' min='30' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Гістерезис вологості (%):</label>";
    html += "<input type='number' name='humHyst' value='" + String(config.humidityConfig.hysteresis) + "' min='1' max='10'>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // Закриваємо tab0

    // TAB 1: ПРИСТРОЇ
    html += "<div class='tab-content' id='tab1'>";
    html += "<div class='section'>";
    html += "<h3>🔧 ОБМЕЖЕННЯ ПРИСТРОЇВ (автоматичний режим)</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум насоса (%):</label>";
    html += "<input type='number' name='pumpMin' value='" + String(config.pumpMinPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум насоса (%):</label>";
    html += "<input type='number' name='pumpMax' value='" + String(config.pumpMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум вентилятора обігріву (%):</label>";
    html += "<input type='number' name='fanMin' value='" + String(config.fanMinPercent) + "' min='0' max='30'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Мінімальна швидкість для стабільної роботи вентилятора</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум вентилятора обігріву (%):</label>";
    html += "<input type='number' name='fanMax' value='" + String(config.fanMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум витяжки (%):</label>";
    html += "<input type='number' name='extractorMin' value='" + String(config.extractorMinPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум витяжки (%):</label>";
    html += "<input type='number' name='extractorMax' value='" + String(config.extractorMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // Закриваємо tab1

    // TAB 2: ТАЙМЕР
    html += "<div class='tab-content' id='tab2'>";
    html += "<div class='section'>";
    html += "<h3>⏰ ТАЙМЕР ВИТЯЖКИ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Час роботи (хвилини):</label>";
    html += "<input type='number' name='extOnMin' value='" + String(config.extractorTimer.onMinutes) + "' min='0' max='120'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час роботи (секунди):</label>";
    html += "<input type='number' name='extOnSec' value='" + String(config.extractorTimer.onSeconds) + "' min='0' max='59'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час паузи (хвилини):</label>";
    html += "<input type='number' name='extOffMin' value='" + String(config.extractorTimer.offMinutes) + "' min='0' max='120'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час паузи (секунди):</label>";
    html += "<input type='number' name='extOffSec' value='" + String(config.extractorTimer.offSeconds) + "' min='0' max='59'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Потужність таймера (%):</label>";
    html += "<input type='number' name='extPower' value='" + String(config.extractorTimer.powerPercent) + "' min='10' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='extEnabled' " + String(config.extractorTimer.enabled ? "checked" : "") + "> Включити таймер</label>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // Закриваємо tab2

    // TAB 3: СЕЗОН
    html += "<div class='tab-content' id='tab3'>";
    html += "<div class='section'>";
    html += "<h3>🔄 СЕЗОННІ РЕЖИМИ</h3>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='coolingMode' " + String(config.coolingMode ? "checked" : "") + "> ❄️ Режим охолодження (літо: холодна вода в теплоносії)</label>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Інверсна логіка: вентилятор працює на максимум при перегріві, насос вимкнений</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='seasonalDisable' " + String(config.seasonalHeatingDisable ? "checked" : "") + "> 🌞 Автоматичне відключення обігріву (травень-вересень)</label>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Система автоматично відключить обігрів в теплі місяці</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // Закриваємо tab3

    // TAB 4: АВАРІЯ
    html += "<div class='tab-content' id='tab4'>";
    html += "<div class='section'>";
    html += "<h3>🚨 МОНІТОРИНГ АВАРІЙ (відключення живлення)</h3>";
    html += "<div class='form-group'>";
    html += "<label>Поріг падіння температури (°C):</label>";
    html += "<input type='number' step='0.1' name='poTempDrop' value='" + String(config.powerOutageTempDropThreshold, 1) + "' min='0.5' max='50.0'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Мінімальне падіння температури теплоносія для виявлення аварії</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Поріг зростання температури (°C):</label>";
    html += "<input type='number' step='0.1' name='poTempRise' value='" + String(config.powerOutageTempRiseThreshold, 1) + "' min='0.5' max='5.0'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Мінімальне зростання температури теплоносія для підтвердження відновлення</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Інтервал перевірки тренду (секунди):</label>";
    html += "<input type='number' name='poCheckInt' value='" + String(config.powerOutageCheckInterval) + "' min='10' max='300'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Як часто перевіряти тренд температури</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Тривалість етапу 1 діагностики (секунди):</label>";
    html += "<input type='number' name='poStage1' value='" + String(config.powerOutageStage1Time) + "' min='30' max='600'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Час для першої спроби аварійного обігріву</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Тривалість паузи між спробами (секунди):</label>";
    html += "<input type='number' name='poPause' value='" + String(config.powerOutagePauseTime) + "' min='60' max='1800'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Час очікування перед наступною спробою</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Тривалість етапів 2/3 (секунди):</label>";
    html += "<input type='number' name='poStage2' value='" + String(config.powerOutageStage2Time) + "' min='30' max='600'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Час для другої та третьої спроби аварійного обігріву</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час автовиходу з аварії (секунди):</label>";
    html += "<input type='number' name='poAutoExit' value='" + String(config.powerOutageAutoExitTime) + "' min='60' max='3600'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Час оцінювання стабільного зростання температури теплоносія для автоматичного виходу з режиму підтримки</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // Закриваємо tab4

    // TAB 5: СИСТЕМА
    html += "<div class='tab-content' id='tab5'>";

    html += "<div class='section'>";
    html += "<h3>📊 МОНІТОРИНГ ТА ЛОГУВАННЯ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Період виведення статусу (секунди):</label>";
    html += "<input type='number' name='statusPeriod' value='" + String(config.statusPeriod / 1000) + "' min='10' max='600'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Як часто виводити статус у Serial Monitor</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>";
    String autoStatusChecked = config.autoStatusEnabled ? " checked" : "";
    html += "<input type='checkbox' name='autoStatus'" + autoStatusChecked + "> Автоматичний вивід статусу в Serial";
    html += "</label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>";
    String use24hChecked = config.use24hFormat ? " checked" : "";
    html += "<input type='checkbox' name='use24h'" + use24hChecked + "> Використовувати 24-годинний формат часу";
    html += "</label>";
    html += "</div>";
    html += "</div>";

    html += "<div class='section'>";
    html += "<h3>🔧 РОЗШИРЕНІ ПАРАМЕТРИ</h3>";
    html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-bottom: 15px;'>";
    html += "<small style='color: #856404;'>⚠️ <strong>Увага:</strong> Змінюйте ці параметри тільки якщо розумієте що вони роблять!</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум адаптивного режиму (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='adaptiveMin' value='" + String(config.a_adaptive_min) + "' min='50' max='100'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Мінімальна потужність насоса в адаптивному режимі (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум адаптивного режиму (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='adaptiveMax' value='" + String(config.a_adaptive_max) + "' min='50' max='100'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Максимальна потужність насоса в адаптивному режимі (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Крок адаптації температури (0.1°C):</label>";
    html += "<input type='number' name='adaptiveTempStep' value='" + String(config.adaptive_temp_step) + "' min='1' max='20'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>На скільки знижувати ціль температури, якщо система не досягає порогів 30 хв (1=0.1°C)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Крок адаптації вологості (%):</label>";
    html += "<input type='number' name='adaptiveHumStep' value='" + String(config.adaptive_hum_step) + "' min='1' max='20'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>На скільки % знижувати ціль вологості, якщо система не досягає порогів 30 хв</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Нормальний діапазон мін (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='normalRangeMin' value='" + String(config.a_normal_range_min) + "' min='0' max='100'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Нижня межа нормального діапазону роботи (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Нормальний діапазон макс (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='normalRangeMax' value='" + String(config.a_normal_range_max) + "' min='0' max='100'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Верхня межа нормального діапазону роботи (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Стартова потужність насоса (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='bStartPercent' value='" + String(config.b_start_percent) + "' min='0' max='100'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Початкова швидкість насоса при старті обігріву (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Вікно аналізу тренду (секунди) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='trendWindow' value='" + String(config.trend_window_seconds) + "' min='60' max='600'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Період для аналізу тенденції зміни температури (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Поріг падіння температури (°C) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='tempDropThreshold' value='" + String(config.temp_drop_threshold, 1) + "' min='0.1' max='5.0' step='0.1'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Критичне падіння температури для аварійних дій (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Інтервал перевірки обігріву (секунди) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='heatingCheckInt' value='" + String(config.heating_check_interval) + "' min='30' max='600'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Як часто перевіряти ефективність обігріву (зарезервовано для майбутньої версії)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Розмір історії даних <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='historySize' value='" + String(config.history_size) + "' min='100' max='2000'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Кількість записів у буфері історії (зарезервовано, зараз фіксовано 1440)</small>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // Закриваємо tab5

    // TAB 6: СЕРВО
    html += "<div class='tab-content' id='tab6'>";
    html += "<div class='section'>";
    html += "<h3>🎯 КАЛІБРУВАННЯ СЕРВО</h3>";

    // Поточний стан
    html += "<div style='background: #e3f2fd; padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3;'>";
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px;'>";
    html += "<div><strong>Поточний кут:</strong><br><span id='servoCurrentAngle' style='font-size: 24px; color: #2196F3;'>" + String(ventState.currentAngle) + "°</span></div>";
    html += "<div><strong>Закрито:</strong><br><span id='servoClosedVal'>" + String(config.servoClosedAngle) + "°</span></div>";
    html += "<div><strong>Відкрито:</strong><br><span id='servoOpenVal'>" + String(config.servoOpenAngle) + "°</span></div>";
    html += "<div><strong>Режим:</strong><br><span id='servoMode' style='font-weight:600;color:";
    html += ventState.calibrationMode ? "#FF9800'>КАЛІБРУВАННЯ" : "#2196F3'>НОРМАЛЬНИЙ";
    html += "</span></div>";
    html += "</div></div>";

    // Кнопки управління
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;' onclick='toggleServoCalibration()' id='servoCalibBtn'>";
    html += ventState.calibrationMode ? "🔓 ВИЙТИ З КАЛІБРУВАННЯ" : "🔒 УВІЙТИ В КАЛІБРУВАННЯ";
    html += "</button>";
    html += "<button type='button' class='btn' style='background:#9C27B0;color:white;' onclick='autoServoCalibrate()'>🤖 АВТОКАЛІБРУВАННЯ</button>";
    html += "</div>";

    // Кнопки руху
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#2196F3;color:white;' onclick='moveServo(\"+1\")'>▲ +1°</button>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;' onclick='moveServo(\"+5\")'>▲▲ +5°</button>";
    html += "<button type='button' class='btn' style='background:#2196F3;color:white;' onclick='moveServo(\"-1\")'>▼ -1°</button>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;' onclick='moveServo(\"-5\")'>▼▼ -5°</button>";
    html += "</div>";

    // Швидкі позиції
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#4CAF50;color:white;' onclick='gotoServoPosition(\"open\")'>➤ Відкрити</button>";
    html += "<button type='button' class='btn' style='background:#f44336;color:white;' onclick='gotoServoPosition(\"closed\")'>➤ Закрити</button>";
    html += "</div>";

    // Збереження позицій
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#4CAF50;color:white;' onclick='saveServoPosition(\"closed\")'>💾 Зберегти як ЗАКРИТО</button>";
    html += "<button type='button' class='btn' style='background:#4CAF50;color:white;' onclick='saveServoPosition(\"open\")'>💾 Зберегти як ВІДКРИТО</button>";
    html += "</div>";

    // Тест
    html += "<button type='button' class='btn' style='background:#9C27B0;color:white;width:100%;margin-bottom:20px;' onclick='testServo()'>🔧 ТЕСТ (відкрити→закрити)</button>";

    // Ручне введення та налаштування
    html += "<div class='form-group'>";
    html += "<label>Кут закритої заслонки (градуси):</label>";
    html += "<input type='number' id='manualServoClosed' name='servoClosed' value='" + String(config.servoClosedAngle) + "' min='0' max='180'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Кут відкритої заслонки (градуси):</label>";
    html += "<input type='number' id='manualServoOpen' name='servoOpen' value='" + String(config.servoOpenAngle) + "' min='0' max='180'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Швидкість руху серво (мс/градус):</label>";
    html += "<input type='number' name='servoSpeed' value='" + String(config.servoSpeed) + "' min='5' max='50'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Менше = швидше (5-10 швидко, 20-30 стандарт, 40-50 повільно)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>";
    String manualVentChecked = config.manualVentControl ? " checked" : "";
    html += "<input type='checkbox' name='manualVent'" + manualVentChecked + "> Ручне керування заслонкою";
    html += "</label>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Вимкнути автоматичне регулювання заслонки (тільки вимикач)</small>";
    html += "</div>";

    html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-top: 10px;'>";
    html += "<small style='color: #856404;'>💡 <strong>Порада:</strong> Використовуйте кнопки для точного калібрування в реальному часі, ";
    html += "або введіть значення вручу та збережіть через кнопку внизу форми.</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // Закриваємо tab6

    // TAB 7: WIFI
    html += "<div class='tab-content' id='tab7'>";

    // Поточний стан WiFi
    html += "<div class='section'>";
    html += "<h3>📶 ПОТОЧНЕ ПІДКЛЮЧЕННЯ</h3>";
    html += "<div style='background: #e3f2fd; padding: 15px; border-radius: 5px; border-left: 4px solid #2196F3;'>";
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px;'>";
    html += "<div><strong>SSID:</strong><br>" + htmlEscape(WiFi.SSID()) + "</div>";
    html += "<div><strong>IP адреса:</strong><br>" + WiFi.localIP().toString() + "</div>";
    html += "<div><strong>Сила сигналу:</strong><br>" + String(WiFi.RSSI()) + " dBm";
    if (WiFi.RSSI() > -50) html += " (відмінно)";
    else if (WiFi.RSSI() > -60) html += " (добре)";
    else if (WiFi.RSSI() > -70) html += " (задовільно)";
    else html += " (слабко)";
    html += "</div>";
    html += "<div><strong>MAC адреса:</strong><br>" + WiFi.macAddress() + "</div>";
    html += "</div></div></div>";

    // Зміна WiFi мережі
    html += "<div class='section'>";
    html += "<h3>🔄 ЗМІНИТИ WI-FI МЕРЕЖУ</h3>";
    html += "<div class='form-group'>";
    html += "<label>SSID (назва мережі):</label>";
    html += "<input type='text' name='wifi_ssid' value='" + htmlEscape(WiFi.SSID()) + "' placeholder='Назва WiFi мережі'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Пароль:</label>";
    html += "<input type='password' name='wifi_password' value='' placeholder='Залиште порожнім щоб не змінювати'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Для безпеки пароль не відображається</small>";
    html += "</div>";
    html += "<div style='background: #fff3cd; padding: 15px; border-radius: 5px; margin-top: 10px; border-left: 4px solid #ffc107;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Після зміни WiFi налаштувань пристрій перезавантажиться та підключиться до нової мережі. ";
    html += "Переконайтеся, що ввели правильні дані!";
    html += "</div></div>";

    // Статична IP (додатково)
    html += "<div class='section'>";
    html += "<h3>🌐 СТАТИЧНА IP (опціонально)</h3>";
    html += "<div class='form-group'>";
    html += "<label>";
    html += "<input type='checkbox' name='use_static_ip' id='use_static_ip'";
    if (config.useStaticIP) html += " checked";
    html += " onchange='toggleStaticIP()'> Використовувати статичну IP адресу";
    html += "</label>";
    html += "</div>";
    String staticIPDisplay = config.useStaticIP ? "block" : "none";
    html += "<div id='static_ip_fields' style='display:" + staticIPDisplay + ";'>";
    html += "<div class='form-group'>";
    html += "<label>IP адреса:</label>";
    html += "<input type='text' name='static_ip' value='" + config.staticIP + "' placeholder='192.168.1.100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Шлюз (Gateway):</label>";
    html += "<input type='text' name='gateway' value='" + config.gateway + "' placeholder='192.168.1.1'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Маска підмережі:</label>";
    html += "<input type='text' name='subnet' value='" + config.subnet + "' placeholder='255.255.255.0'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>DNS сервер:</label>";
    html += "<input type='text' name='dns' value='" + config.dns + "' placeholder='8.8.8.8'>";
    html += "</div>";
    html += "</div></div>";

    // Безпека
    html += "<div class='section'>";
    html += "<h3>🔐 БЕЗПЕКА ВЕБ-ІНТЕРФЕЙСУ</h3>";
    html += "<div class='form-group'>";
    html += "<label>";
    html += "<input type='checkbox' name='use_auth' id='use_auth'";
    if (config.useAuth) html += " checked";
    html += " onchange='toggleAuth()'> Увімкнути автентифікацію (логін/пароль)";
    html += "</label>";
    html += "</div>";
    String authDisplay = config.useAuth ? "block" : "none";
    html += "<div id='auth_fields' style='display:" + authDisplay + ";'>";
    html += "<div class='form-group'>";
    html += "<label>Логін:</label>";
    html += "<input type='text' name='auth_user' value='" + config.authLogin + "' placeholder='admin'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Пароль:</label>";
    html += "<input type='password' name='auth_pass' value='' placeholder='Залиште порожнім щоб не змінювати'>";
    html += "</div>";
    html += "<div style='background: #ffebee; padding: 10px; border-radius: 5px; margin-top: 10px; border-left: 4px solid #f44336;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Обов'язково увімкніть автентифікацію якщо плануєте відкрити доступ через інтернет!";
    html += "</div>";
    html += "</div></div>";

    html += "</div>"; // Закриваємо tab7

    html += "<div style='margin-top: 30px;'>";
    html += "<button type='submit' class='btn'>💾 ЗБЕРЕГТИ НАЛАШТУВАННЯ</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='window.location.href=\"/\"'>← НА ГОЛОВНУ</button>";
    html += "</div>";

    html += "</form>";
    
    html += "</div>";
    
    html += "<script>";
    // Функція перемикання вкладок
    html += "function switchTab(tabIndex) {";
    html += "  const tabs = document.querySelectorAll('.tab');";
    html += "  const contents = document.querySelectorAll('.tab-content');";
    html += "  tabs.forEach((tab, i) => {";
    html += "    if (i === tabIndex) {";
    html += "      tab.classList.add('active');";
    html += "      contents[i].classList.add('active');";
    html += "    } else {";
    html += "      tab.classList.remove('active');";
    html += "      contents[i].classList.remove('active');";
    html += "    }";
    html += "  });";
    html += "  localStorage.setItem('settingsTab', tabIndex);";
    html += "}";
    // Відновлення останньої вкладки при завантаженні
    html += "window.addEventListener('load', function() {";
    html += "  const savedTab = localStorage.getItem('settingsTab');";
    html += "  if (savedTab !== null) switchTab(parseInt(savedTab));";
    html += "});";
    // Валідація форми
    html += "document.querySelector('form').addEventListener('submit', function(e) {";
    html += "  const tempMin = parseFloat(document.querySelector('[name=\"tempMin\"]').value);";
    html += "  const tempMax = parseFloat(document.querySelector('[name=\"tempMax\"]').value);";
    html += "  if (tempMin >= tempMax) {";
    html += "    alert('Помилка: Мінімальна температура має бути менше максимальної!');";
    html += "    e.preventDefault();";
    html += "    switchTab(0);";
    html += "    return;";
    html += "  }";
    html += "  const humMin = parseFloat(document.querySelector('[name=\"humMin\"]').value);";
    html += "  const humMax = parseFloat(document.querySelector('[name=\"humMax\"]').value);";
    html += "  if (humMin >= humMax) {";
    html += "    alert('Помилка: Мінімальна вологість має бути менше максимальної!');";
    html += "    e.preventDefault();";
    html += "    switchTab(0);";
    html += "    return;";
    html += "  }";
    html += "});";

    // Функції для серво
    html += "function sendServoCommand(cmd) {";
    html += "  return fetch('/servo/api', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'cmd=' + cmd";
    html += "  }).then(r => r.text()).then(data => {";
    html += "    if(data.startsWith('ANGLE:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('servoCurrentAngle').innerText = angle + '°';";
    html += "    } else if(data.startsWith('CLOSED:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('servoClosedVal').innerText = angle + '°';";
    html += "      document.getElementById('manualServoClosed').value = angle;";
    html += "      alert('✅ Закрите положення збережено: ' + angle + '°');";
    html += "    } else if(data.startsWith('OPEN:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('servoOpenVal').innerText = angle + '°';";
    html += "      document.getElementById('manualServoOpen').value = angle;";
    html += "      alert('✅ Відкрите положення збережено: ' + angle + '°');";
    html += "    } else if(data == 'TEST_OK') {";
    html += "      alert('✅ Тест серво завершено');";
    html += "    } else if(data.startsWith('MODE:')) {";
    html += "      location.reload();";
    html += "    }";
    html += "    return data;";
    html += "  });";
    html += "}";
    html += "function moveServo(delta) { sendServoCommand('move:' + delta); }";
    html += "function saveServoPosition(type) { sendServoCommand('save:' + type); }";
    html += "function gotoServoPosition(type) { sendServoCommand('goto:' + type); }";
    html += "function toggleServoCalibration() { sendServoCommand('calibration:toggle'); }";
    html += "function autoServoCalibrate() {";
    html += "  if(confirm('Автокалібрування: швидко перемикайте вимикач для зміни напряму. Продовжити?')) {";
    html += "    sendServoCommand('auto:calibrate');";
    html += "    alert('🤖 Швидко перемикайте вимикач протягом 8 секунд!');";
    html += "    setTimeout(() => location.reload(), 8000);";
    html += "  }";
    html += "}";
    html += "function testServo() {";
    html += "  if(confirm('Тест відкриє і закриє заслонку. Продовжити?')) {";
    html += "    sendServoCommand('test');";
    html += "  }";
    html += "}";

    // Функції для WiFi налаштувань
    html += "function toggleStaticIP() {";
    html += "  const checked = document.getElementById('use_static_ip').checked;";
    html += "  document.getElementById('static_ip_fields').style.display = checked ? 'block' : 'none';";
    html += "}";
    html += "function toggleAuth() {";
    html += "  const checked = document.getElementById('use_auth').checked;";
    html += "  document.getElementById('auth_fields').style.display = checked ? 'block' : 'none';";
    html += "}";

    html += "</script>";
    
    html += "<div style='text-align: center; margin-top: 30px;'><a href='/' style='background: #4CAF50; color: white; padding: 12px 24px; text-decoration: none; border-radius: 5px; display: inline-block;'>← На головну</a></div>";
    html += getUkraineMarquee();
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleSaveSettings() {
    // БЕЗПЕКА: Перевірка CSRF
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
        return;
    }

    // ВАЛІДАЦІЯ: Температура
    if (server.hasArg("tempMin")) {
        float tempMin = server.arg("tempMin").toFloat();
        if (tempMin < 10.0 || tempMin > 40.0) {
            server.send(400, "text/plain", "❌ Мін. температура має бути від 10 до 40°C");
            return;
        }
        config.tempMin = tempMin;
    }
    if (server.hasArg("tempMax")) {
        float tempMax = server.arg("tempMax").toFloat();
        if (tempMax < 10.0 || tempMax > 40.0) {
            server.send(400, "text/plain", "❌ Макс. температура має бути від 10 до 40°C");
            return;
        }
        if (tempMax <= config.tempMin) {
            server.send(400, "text/plain", "❌ Макс. температура має бути більшою за мін.");
            return;
        }
        config.tempMax = tempMax;
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
    if (server.hasArg("extOnMin")) {
        config.extractorTimer.onMinutes = server.arg("extOnMin").toInt();
    }
    if (server.hasArg("extOnSec")) {
        config.extractorTimer.onSeconds = server.arg("extOnSec").toInt();
    }
    if (server.hasArg("extOffMin")) {
        config.extractorTimer.offMinutes = server.arg("extOffMin").toInt();
    }
    if (server.hasArg("extOffSec")) {
        config.extractorTimer.offSeconds = server.arg("extOffSec").toInt();
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
    if (server.hasArg("servoSpeed")) {
        config.servoSpeed = constrain(server.arg("servoSpeed").toInt(), 5, 50);
    }
    if (server.hasArg("statusPeriod")) {
        config.statusPeriod = server.arg("statusPeriod").toInt() * 1000UL;
    }

    // Системні налаштування TAB 5
    if (server.hasArg("autoStatus")) {
        config.autoStatusEnabled = true;
    } else {
        config.autoStatusEnabled = false;
    }
    if (server.hasArg("use24h")) {
        config.use24hFormat = true;
    } else {
        config.use24hFormat = false;
    }
    if (server.hasArg("manualVent")) {
        config.manualVentControl = true;
    } else {
        config.manualVentControl = false;
    }
    if (server.hasArg("adaptiveMin")) {
        config.a_adaptive_min = constrain(server.arg("adaptiveMin").toInt(), 50, 100);
    }
    if (server.hasArg("adaptiveMax")) {
        config.a_adaptive_max = constrain(server.arg("adaptiveMax").toInt(), 50, 100);
    }
    if (server.hasArg("adaptiveTempStep")) {
        config.adaptive_temp_step = constrain(server.arg("adaptiveTempStep").toInt(), 1, 20);
    }
    if (server.hasArg("adaptiveHumStep")) {
        config.adaptive_hum_step = constrain(server.arg("adaptiveHumStep").toInt(), 1, 20);
    }
    if (server.hasArg("normalRangeMin")) {
        config.a_normal_range_min = constrain(server.arg("normalRangeMin").toInt(), 0, 100);
    }
    if (server.hasArg("normalRangeMax")) {
        config.a_normal_range_max = constrain(server.arg("normalRangeMax").toInt(), 0, 100);
    }
    if (server.hasArg("bStartPercent")) {
        config.b_start_percent = constrain(server.arg("bStartPercent").toInt(), 0, 100);
    }
    if (server.hasArg("trendWindow")) {
        config.trend_window_seconds = constrain(server.arg("trendWindow").toInt(), 60, 600);
    }
    if (server.hasArg("tempDropThreshold")) {
        config.temp_drop_threshold = constrain(server.arg("tempDropThreshold").toFloat(), 0.1, 5.0);
    }
    if (server.hasArg("heatingCheckInt")) {
        config.heating_check_interval = constrain(server.arg("heatingCheckInt").toInt(), 30, 600);
    }
    if (server.hasArg("historySize")) {
        config.history_size = constrain(server.arg("historySize").toInt(), 100, 2000);
    }

    // Калібрування серво
    if (server.hasArg("servoClosed")) {
        int closedAngle = constrain(server.arg("servoClosed").toInt(), 0, 180);
        config.servoClosedAngle = closedAngle;
    }
    if (server.hasArg("servoOpen")) {
        int openAngle = constrain(server.arg("servoOpen").toInt(), 0, 180);
        config.servoOpenAngle = openAngle;
    }

    // Сезонні режими
    if (server.hasArg("coolingMode")) {
        config.coolingMode = true;
    } else {
        config.coolingMode = false;
    }
    if (server.hasArg("seasonalDisable")) {
        config.seasonalHeatingDisable = true;
    } else {
        config.seasonalHeatingDisable = false;
    }

    // Параметри моніторингу аварій
    if (server.hasArg("poTempDrop")) {
        config.powerOutageTempDropThreshold = server.arg("poTempDrop").toFloat();
    }
    if (server.hasArg("poTempRise")) {
        config.powerOutageTempRiseThreshold = server.arg("poTempRise").toFloat();
    }
    if (server.hasArg("poCheckInt")) {
        config.powerOutageCheckInterval = server.arg("poCheckInt").toInt();
    }
    if (server.hasArg("poStage1")) {
        config.powerOutageStage1Time = server.arg("poStage1").toInt();
    }
    if (server.hasArg("poPause")) {
        config.powerOutagePauseTime = server.arg("poPause").toInt();
    }
    if (server.hasArg("poStage2")) {
        config.powerOutageStage2Time = server.arg("poStage2").toInt();
    }
    if (server.hasArg("poAutoExit")) {
        config.powerOutageAutoExitTime = server.arg("poAutoExit").toInt();
    }

    // WiFi налаштування
    bool wifiChanged = false;
    if (server.hasArg("wifi_ssid") && server.arg("wifi_ssid").length() > 0) {
        String newSSID = server.arg("wifi_ssid");
        if (newSSID != WiFi.SSID()) {
            prefs.begin("wifi", false);
            prefs.putString("ssid", newSSID);
            prefs.end();
            wifiChanged = true;
        }
    }
    if (server.hasArg("wifi_password") && server.arg("wifi_password").length() > 0) {
        String newPassword = server.arg("wifi_password");
        prefs.begin("wifi", false);
        prefs.putString("password", newPassword);
        prefs.end();
        wifiChanged = true;
    }

    // Статична IP
    if (server.hasArg("use_static_ip")) {
        config.useStaticIP = true;
        if (server.hasArg("static_ip")) {
            config.staticIP = server.arg("static_ip");
        }
        if (server.hasArg("gateway")) {
            config.gateway = server.arg("gateway");
        }
        if (server.hasArg("subnet")) {
            config.subnet = server.arg("subnet");
        }
        if (server.hasArg("dns")) {
            config.dns = server.arg("dns");
        }
        wifiChanged = true;
    } else {
        if (config.useStaticIP) {
            config.useStaticIP = false;
            wifiChanged = true;
        }
    }

    // Автентифікація
    if (server.hasArg("use_auth")) {
        config.useAuth = true;
        if (server.hasArg("auth_user") && server.arg("auth_user").length() > 0) {
            config.authLogin = server.arg("auth_user");
        }
        if (server.hasArg("auth_pass") && server.arg("auth_pass").length() > 0) {
            config.authPassword = server.arg("auth_pass");
        }
    } else {
        config.useAuth = false;
    }

    saveConfiguration();

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    if (wifiChanged) {
        html += "<meta http-equiv='refresh' content='3;url=/'>"; // Після перезавантаження на головну
    } else {
        html += "<meta http-equiv='refresh' content='2;url=/settings'>";
    }
    html += "<title>Налаштування збережені</title>";
    html += "<style>body { font-family: Arial; text-align: center; padding: 50px; }</style>";
    html += "</head><body>";
    html += "<h1>✅ Налаштування успішно збережені!</h1>";
    if (wifiChanged) {
        html += "<p><strong>⚠️ WiFi налаштування змінено! Пристрій перезавантажується...</strong></p>";
        html += "<p>Після перезавантаження підключіться до нової мережі та перейдіть за адресою пристрою.</p>";
    } else {
        html += "<p>Перенаправлення обернено на сторінку налаштувань...</p>";
    }
    html += getUkraineMarquee();
    html += "</body></html>";

    server.send(200, "text/html", html);

    // Якщо змінились WiFi налаштування, перезавантажуємо пристрій
    if (wifiChanged) {
        delay(2000); // Даємо час показати повідомлення
        ESP.restart();
    }
}

// ============================================================================
// СТОРІНКА КЕРУВАННЯ
// ============================================================================

void handleControlPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Керування</title>";
    html += "<style>";
    html += "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');";
    html += "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";

    // ПОКРАЩЕНІ СЛАЙДЕРИ
    html += ".slider { -webkit-appearance: none; appearance: none; width: 100%; height: 8px; border-radius: 5px; background: #ddd; outline: none; margin: 15px 0; transition: background 0.3s; }";
    html += ".slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 28px; height: 28px; border-radius: 50%; background: #4CAF50; cursor: pointer; box-shadow: 0 2px 6px rgba(0,0,0,0.2); transition: all 0.3s; }";
    html += ".slider::-webkit-slider-thumb:hover { transform: scale(1.2); box-shadow: 0 3px 10px rgba(76, 175, 80, 0.5); }";
    html += ".slider::-webkit-slider-thumb:active { transform: scale(1.1); }";
    html += ".slider::-moz-range-thumb { width: 28px; height: 28px; border-radius: 50%; background: #4CAF50; cursor: pointer; border: none; box-shadow: 0 2px 6px rgba(0,0,0,0.2); }";
    html += ".slider:hover { background: #ccc; }";

    html += ".btn { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; margin: 5px; cursor: pointer; transition: all 0.3s; font-size: 14px; }";
    html += ".btn:hover { background: #45a049; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }";
    html += ".btn:active { transform: translateY(0); }";
    html += ".slider-value { display: inline-block; min-width: 50px; text-align: center; font-weight: 700; font-size: 20px; color: #4CAF50; margin-left: 10px; }";
    html += ".control-section { margin-bottom: 30px; padding: 20px; background: #f9f9f9; border-radius: 8px; }";
    html += ".control-section h3 { margin-top: 0; color: #2c3e50; font-size: 18px; }";
    html += ".mode-buttons { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; }";
    html += ".mode-btn { padding: 15px 20px; border: 2px solid #ddd; background: #f5f5f5; color: #333; border-radius: 8px; cursor: pointer; font-size: 16px; font-weight: 500; transition: all 0.3s; position: relative; overflow: hidden; }";
    html += ".mode-btn:hover { transform: translateY(-3px); box-shadow: 0 6px 12px rgba(0,0,0,0.15); }";
    html += ".mode-btn.active { background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%); color: white; border-color: #4CAF50; box-shadow: 0 4px 15px rgba(76, 175, 80, 0.4); animation: pulse 2s infinite; }";
    html += "@keyframes pulse { 0%, 100% { box-shadow: 0 4px 15px rgba(76, 175, 80, 0.4); } 50% { box-shadow: 0 6px 20px rgba(76, 175, 80, 0.6); } }";
    html += ".mode-btn.active::before { content: '✓ '; font-weight: 600; margin-right: 5px; }";

    // МОБІЛЬНА ОПТИМІЗАЦІЯ
    html += "@media (max-width: 768px) {";
    html += "  body { margin: 10px; }";
    html += "  .container { padding: 15px; }";
    html += "  .control-section { padding: 15px; }";
    html += "  .slider { height: 12px; margin: 20px 0; }";  // Товстіші слайдери на мобільному
    html += "  .slider::-webkit-slider-thumb { width: 36px; height: 36px; }";  // Більший thumb для пальця
    html += "  .slider::-moz-range-thumb { width: 36px; height: 36px; }";
    html += "  .slider-value { font-size: 24px; display: block; margin: 10px 0; }";  // Відображення значення під слайдером
    html += "  .btn { padding: 12px 16px; margin: 3px; font-size: 13px; min-width: 60px; }";
    html += "  .mode-btn { font-size: 14px; padding: 12px 15px; }";
    html += "  h1 { font-size: 1.5em; }";
    html += "  .control-section h3 { font-size: 16px; }";
    html += "}";

    // ДОДАТКОВА ОПТИМІЗАЦІЯ ДЛЯ ДУЖЕ МАЛИХ ЕКРАНІВ
    html += "@media (max-width: 480px) {";
    html += "  .btn { display: inline-block; width: calc(33.33% - 6px); margin: 3px; padding: 10px 5px; font-size: 12px; }";
    html += "}";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>🎛️ ПАНЕЛЬ КЕРУВАННЯ СИСТЕМОЮ</h1>";
    html += "<p><a href='/'>← На головну</a></p>";
    
    html += "<div class='control-section'>";
    html += "<h3>НАСОС (A)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String(round(heatingState.pumpPower * 100.0 / 255.0)) + "' class='slider' id='pumpSlider' oninput='updatePump(this.value)'>";
    html += "<span class='slider-value' id='pumpValue'>" + String(round(heatingState.pumpPower * 100.0 / 255.0)) + "%</span>";
    html += "<button class='btn' onclick=\"setPower('pump', 0)\">ВИМК</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 30)\">30%</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 50)\">50%</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 80)\">80%</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 100)\">100%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>ВЕНТИЛЯТОР (B)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String(round(heatingState.fanPower * 100.0 / 255.0)) + "' class='slider' id='fanSlider' oninput='updateFan(this.value)'>";
    html += "<span class='slider-value' id='fanValue'>" + String(round(heatingState.fanPower * 100.0 / 255.0)) + "%</span>";
    html += "<button class='btn' onclick=\"setPower('fan', 0)\">ВИМК</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 30)\">30%</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 50)\">50%</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 80)\">80%</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 100)\">100%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>ВИТЯЖКА (C)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String(round(heatingState.extractorPower * 100.0 / 255.0)) + "' class='slider' id='extractorSlider' oninput='updateExtractor(this.value)'>";
    html += "<span class='slider-value' id='extractorValue'>" + String(round(heatingState.extractorPower * 100.0 / 255.0)) + "%</span>";
    html += "<button class='btn' onclick=\"setPower('extractor', 0)\">ВИМК</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 30)\">30%</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 50)\">50%</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 80)\">80%</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 100)\">100%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>РЕЖИМИ РОБОТИ</h3>";
    html += "<div class='mode-buttons'>";
    html += "<button class='mode-btn" + String(!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode ? " active" : "") + "' onclick=\"sendCmd('auto')\" data-mode='auto'>🤖 АВТО</button>";
    html += "<button class='mode-btn" + String(heatingState.manualMode ? " active" : "") + "' onclick=\"sendCmd('manual')\" data-mode='manual'>✋ РУЧНИЙ</button>";
    html += "</div>";

    // Показуємо кнопку скидання аварійного режиму якщо він активний
    if (powerOutageState.detected || powerOutageState.emergencyHeatingActive) {
        html += "<div style='margin-top: 15px;'>";
        html += "<button class='btn' style='background: #f44336; width: 100%; padding: 15px; font-size: 16px; font-weight: bold;' onclick=\"resetEmergency()\">🔄 СКИНУТИ АВАРІЙНИЙ РЕЖИМ</button>";
        html += "</div>";
    }

    html += "<div style='margin-top: 15px; padding: 12px; background: #fff3cd; border-left: 4px solid #ffc107; border-radius: 5px; font-size: 14px;'>";
    html += "<strong>ℹ️ Автоматичні режими:</strong><br>";
    html += "⚡ <strong>ФОРСАЖ</strong> - вмикається при температурі < 20°C (80% потужність)<br>";
    html += "🚨 <strong>АВАРІЯ</strong> - вмикається при температурі < 18°C (100% потужність)";
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
    
    html += "<script>";
    html += "function setPower(device, value) {";
    html += "  sendCmd('manual');";
    html += "  const slider = document.getElementById(device + 'Slider');";
    html += "  const display = document.getElementById(device + 'Value');";
    html += "  if (slider) slider.value = value;";
    html += "  if (display) display.textContent = value + '%';";
    html += "  sendCmd(device + ' ' + value);";
    html += "}";
    html += "function updatePump(v) { sendCmd('manual'); document.getElementById('pumpValue').textContent = v + '%'; sendCmd('pump ' + v); }";
    html += "function updateFan(v) { sendCmd('manual'); document.getElementById('fanValue').textContent = v + '%'; sendCmd('fan ' + v); }";
    html += "function updateExtractor(v) { sendCmd('manual'); document.getElementById('extractorValue').textContent = v + '%'; sendCmd('extractor ' + v); }";
    html += "function updateModeButtons(activeMode) {";
    html += "  const btns = document.querySelectorAll('.mode-btn');";
    html += "  btns.forEach(btn => {";
    html += "    btn.classList.remove('active');";
    html += "    if (btn.getAttribute('data-mode') === activeMode) {";
    html += "      btn.classList.add('active');";
    html += "    }";
    html += "  });";
    html += "}";
    html += "function sendCmd(cmd) {";
    html += "  fetch('/command', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'cmd=' + encodeURIComponent(cmd)";
    html += "  }).then(response => response.text()).then(text => {";
    html += "    console.log('Команда виконана:', text);";
    html += "    if (cmd === 'auto' || cmd === 'manual' || cmd === 'force' || cmd === 'emergency') {";
    html += "      updateModeButtons(cmd);";
    html += "      if (cmd === 'auto') {";
    html += "        setTimeout(function() { location.reload(); }, 500);";
    html += "      }";
    html += "    }";
    html += "  });";
    html += "}";
    html += "function resetEmergency() {";
    html += "  if (confirm('Скинути аварійний режим та повернутися до штатної роботи?')) {";
    html += "    sendCmd('reset_emergency');";
    html += "    setTimeout(function() { location.reload(); }, 1000);";
    html += "  }";
    html += "}";
    html += "</script>";
    
    html += "<div style='text-align: center; margin: 30px 0; padding-top: 20px; border-top: 1px solid #ddd;'>";
    html += "<a href='/' style='background: #4CAF50; color: white; padding: 12px 24px; text-decoration: none; border-radius: 5px; display: inline-block;'>← На головну</a>";
    html += "</div>";
    
    html += getUkraineMarquee();
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// ДОДАТКОВІ СТОРІНКИ
// ============================================================================

void handleTimePage() {
    String html = "";
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 600px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>🕒 ЧАС СИСТЕМИ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>Поточний час:</strong> " + getTimeString() + "</p>";
    html += "<p><strong>Дата:</strong> " + getDateString() + "</p>";
    html += "<p><strong>Формат часу:</strong> " + getFormattedTime() + "</p>";
    html += "<p><strong>Синхронізація:</strong> " + String(isTimeSynced() ? "✅ Синхронізовано" : "⚠️ Немає синхронізації") + "</p>";
    html += "</div>";
    html += "<p><a href='/'>← На головну</a></p>";
    html += "</div>";
    html += getUkraineMarquee();
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleWiFiPage() {
    String html = "";
    html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>WiFi налаштування</title>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; }";
    html += ".info-box { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 15px 0; }";
    html += ".info-item { display: flex; justify-content: space-between; padding: 5px 0; }";
    html += ".form-group { margin: 15px 0; }";
    html += ".form-group label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += ".form-group input { width: 100%; padding: 8px; box-sizing: border-box; border: 1px solid #ddd; border-radius: 4px; }";
    html += ".btn { padding: 12px 24px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }";
    html += ".btn-primary { background: #2196F3; color: white; }";
    html += ".btn-secondary { background: #666; color: white; }";
    html += ".nav { text-align: center; margin-top: 20px; }";
    // CSS для tooltips
    html += ".help-icon { display: inline-block; width: 22px; height: 22px; line-height: 22px; background: #9c27b0; color: #fff; border-radius: 50%; text-align: center; font-size: 14px; font-weight: bold; cursor: pointer; margin-left: 8px; vertical-align: middle; }";
    html += ".help-icon:hover { background: #7b1fa2; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>📶 WiFi налаштування</h1>";
    
    // Попередження про різні мережі
    html += "<div style='background: #fff3cd; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #ffc107;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong><br>";
    html += "Для доступу ваш пристрій (ноутбук/телефон) і ESP32 мають бути підключені до <strong>ОДНІЄЇ WiFi мережі!</strong><br><br>";
    html += "Якщо ви не бачите цю сторінку з іншої мережі - це нормально. Підключіться до мережі <strong>" + htmlEscape(WiFi.SSID()) + "</strong>";
    html += "</div>";

    html += "<div class='info-box'>";
    html += "<div class='info-item'><span><strong>SSID:</strong></span><span>" + htmlEscape(WiFi.SSID()) + "</span></div>";
    html += "<div class='info-item'><span><strong>IP адреса:</strong></span><span>" + htmlEscape(WiFi.localIP().toString()) + "</span></div>";
    html += "<div class='info-item'><span><strong>mDNS:</strong></span><span>http://klimat.local</span></div>";
    html += "<div class='info-item'><span><strong>MAC адреса:</strong></span><span>" + htmlEscape(WiFi.macAddress()) + "</span></div>";
    html += "<div class='info-item'><span><strong>Сигнал (RSSI):</strong></span><span>" + String(WiFi.RSSI()) + " dBm</span></div>";
    html += "<div class='info-item'><span><strong>Статус:</strong></span><span>" + String(WiFi.status() == WL_CONNECTED ? "Підключено ✅" : "Відключено ❌") + "</span></div>";
    html += "</div>";
    
    html += "<h2>⚙ Налаштування статичної IP <span class='help-icon' onclick='alert(\"Статична IP дозволяє ESP32 завжди мати одну адресу в вашій домашній мережі. Корисно для закладок та автоматизації.\")'>?</span></h2>";
    html += "<form action='/saveNetwork' method='POST'>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='useStaticIP' value='1' " + String(config.useStaticIP ? "checked" : "") + "> Використовувати статичну IP <span class='help-icon' onclick='alert(\"Якщо увімкнено, ESP32 буде використовувати фіксовану IP замість автоматичної (DHCP). Потрібна перезагрузка після зміни.\")'>?</span></label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>IP адреса: <span class='help-icon' onclick='alert(\"Виберіть вільну IP в діапазоні вашої мережі (наприклад 192.168.1.100). Перевірте що вона не зайнята іншим пристроєм!\")'>?</span></label>";
    html += "<input type='text' name='staticIP' value='" + config.staticIP + "' placeholder='192.168.1.100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Шлюз (Gateway): <span class='help-icon' onclick='alert(\"Адреса вашого роутера. Дізнатись: Windows - ipconfig (Default Gateway), Linux/Mac - ip route | grep default\")'>?</span></label>";
    html += "<input type='text' name='gateway' value='" + config.gateway + "' placeholder='192.168.1.1'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Маска підмережі (Subnet): <span class='help-icon' onclick='alert(\"Зазвичай 255.255.255.0 для домашніх мереж. Не змінюйте якщо не впевнені.\")'>?</span></label>";
    html += "<input type='text' name='subnet' value='" + config.subnet + "' placeholder='255.255.255.0'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>DNS сервер: <span class='help-icon' onclick='alert(\"8.8.8.8 - Google DNS (рекомендовано). Або використайте DNS вашого провайдера.\")'>?</span></label>";
    html += "<input type='text' name='dns' value='" + config.dns + "' placeholder='8.8.8.8'>";
    html += "</div>";
    
    html += "<h2>🔐 Безпека (для доступу через інтернет) <span class='help-icon' onclick='alert(\"Обов'язково увімкніть автентифікацію якщо плануєте відкрити доступ через інтернет (Port Forwarding)! Інакше хто завгодно зможе керувати вашою системою.\")'>?</span></h2>";
    html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-bottom: 15px; font-size: 0.9em;'>";
    html += "⚠️ Увімкніть автентифікацію якщо плануєте відкрити доступ через інтернет!";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='useAuth' value='1' " + String(config.useAuth ? "checked" : "") + "> Увімкнути автентифікацію <span class='help-icon' onclick='alert(\"Коли увімкнено, браузер запитуватиме логін та пароль перед доступом до інтерфейсу. Рекомендовано для безпеки!\")'>?</span></label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Логін: <span class='help-icon' onclick='alert(\"Ім'я користувача для входу. За замовчуванням: admin\")'>?</span></label>";
    html += "<input type='text' name='authLogin' value='" + config.authLogin + "' placeholder='admin'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Пароль: <span class='help-icon' onclick='alert(\"Використовуйте складний пароль (мінімум 8 символів)! НЕ використовуйте 12345, password, admin тощо.\")'>?</span></label>";
    html += "<input type='password' name='authPassword' value='" + config.authPassword + "' placeholder='Введіть пароль'>";
    html += "</div>";
    
    html += "<div style='text-align: center;'>";
    html += "<button type='submit' class='btn btn-primary'>Зберегти налаштування</button>";
    html += "</div>";
    html += "</form>";
    
    html += "<h2>🌐 Доступ через інтернет";
    html += "<span class='tooltip'><span class='help-icon'>?</span>";
    html += "<span class='tooltiptext'>Дозволяє керувати системою з будь-якої точки світу через інтернет. Вимагає налаштування роутера.</span>";
    html += "</span></h2>";
    html += "<div style='background: #f0f0f0; padding: 15px; border-radius: 5px; font-size: 0.9em;'>";
    html += "<p><strong>Швидкий гайд:</strong></p>";
    html += "<ol style='margin: 10px 0; padding-left: 20px; line-height: 1.8;'>";
    html += "<li>✅ Увімкнути автентифікацію вище ☝️ <span style='color: red; font-weight: 600;'>(ОБОВ'ЯЗКОВО!)</span></li>";
    html += "<li>🔧 Налаштувати Port Forwarding на роутері:<br>";
    html += "   <div style='background: white; padding: 8px; margin: 5px 0; border-radius: 3px; font-family: monospace;'>";
    html += "   Внутрішня IP: <strong>" + htmlEscape(WiFi.localIP().toString()) + "</strong><br>";
    html += "   Внутрішній порт: <strong>80</strong><br>";
    html += "   Зовнішній порт: <strong>8080</strong> (можна інший)";
    html += "   </div></li>";
    html += "<li>🌍 Дізнатися свою зовнішню IP: <a href='https://myip.com.ua' target='_blank' style='color: #2196F3;'>myip.com.ua</a></li>";
    html += "<li>🚀 Доступ звідки завгодно: <code style='background: white; padding: 2px 6px;'>http://[ваша_IP]:8080</code></li>";
    html += "</ol>";
    html += "<div style='background: #e3f2fd; padding: 10px; border-radius: 5px; margin-top: 10px;'>";
    html += "<strong>💡 Корисні поради:</strong><br>";
    html += "• Для динамічної IP → DynDNS, No-IP, DuckDNS<br>";
    html += "• Для максимальної безпеки → VPN на роутері<br>";
    html += "• Детальна інструкція в документації проекту";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='nav'>";
    html += "<a href='/'>← На головну</a>";
    html += "</div>";
    html += "</div>";
    
    html += getUkraineMarquee();
    
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleSaveNetworkSettings() {
    // БЕЗПЕКА: Перевірка CSRF
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
        return;
    }

    bool needRestart = false;

    // Зчитуємо налаштування з форми
    if (server.hasArg("useStaticIP")) {
        config.useStaticIP = true;
        needRestart = true;
    } else {
        if (config.useStaticIP) {
            needRestart = true;
        }
        config.useStaticIP = false;
    }
    
    if (server.hasArg("staticIP") && server.arg("staticIP") != config.staticIP) {
        config.staticIP = server.arg("staticIP");
        needRestart = true;
    }
    
    if (server.hasArg("gateway") && server.arg("gateway") != config.gateway) {
        config.gateway = server.arg("gateway");
        needRestart = true;
    }
    
    if (server.hasArg("subnet") && server.arg("subnet") != config.subnet) {
        config.subnet = server.arg("subnet");
        needRestart = true;
    }
    
    if (server.hasArg("dns") && server.arg("dns") != config.dns) {
        config.dns = server.arg("dns");
        needRestart = true;
    }
    
    // Налаштування безпеки
    if (server.hasArg("useAuth")) {
        config.useAuth = true;
    } else {
        config.useAuth = false;
    }
    
    if (server.hasArg("authLogin")) {
        config.authLogin = server.arg("authLogin");
    }
    
    if (server.hasArg("authPassword") && server.arg("authPassword").length() > 0) {
        config.authPassword = server.arg("authPassword");
    }
    
    // Зберігаємо в Preferences
    preferences.begin("climate", false);
    preferences.putBool("useStaticIP", config.useStaticIP);
    preferences.putString("staticIP", config.staticIP);
    preferences.putString("gateway", config.gateway);
    preferences.putString("subnet", config.subnet);
    preferences.putString("dns", config.dns);
    preferences.putBool("useAuth", config.useAuth);
    preferences.putString("authLogin", config.authLogin);
    preferences.putString("authPass", config.authPassword);
    preferences.end();
    
    Serial.println("✅ Мережеві налаштування та безпека збережено");
    
    // Відправляємо відповідь
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='3;url=/wifi'>";
    html += "</head><body>";
    html += "<div style='max-width: 600px; margin: 50px auto; padding: 20px; background: white; border-radius: 10px; text-align: center;'>";
    html += "<h1>✅ Налаштування збережено!</h1>";
    
    if (needRestart) {
        html += "<p>⚠️ Для застосування змін необхідно перезавантажити пристрій.</p>";
        html += "<p>Автоматичне перенаправлення через 3 секунди...</p>";
    } else {
        html += "<p>Перенаправлення через 3 секунди...</p>";
    }
    
    html += "<p><a href='/wifi'>← Повернутись до налаштувань WiFi</a></p>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

void handleHistoryPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>📈 Історія даних</title>";
    html += "<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0'></script>";
    html += "<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.1'></script>";
    html += "<style>";
    html += "body { font-family: 'Inter', -apple-system, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 15px; box-shadow: 0 8px 32px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; margin-bottom: 10px; }";
    html += ".info { text-align: center; color: #666; margin-bottom: 30px; font-size: 0.9em; }";
    html += ".chart-container { position: relative; height: 300px; margin: 30px 0; }";
    html += ".back-btn { display: inline-block; background: #4CAF50; color: white; padding: 12px 24px; text-decoration: none; border-radius: 8px; margin-top: 20px; }";
    html += ".back-btn:hover { background: #45a049; }";
    html += "h2 { color: #667eea; margin-top: 40px; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>📈 ІСТОРІЯ ТЕМПЕРАТУРИ І ВОЛОГОСТІ</h1>";
    html += "<div class='info' id='statsInfo'>";
    LoggerStats stats = getLoggerStats();
    html += "Записів в RAM: " + String(stats.totalRecordsRAM) + " / 1440";
    html += " | Записів в SPIFFS: " + String(stats.totalRecordsSPIFFS);
    html += " | Оновлення кожну хвилину";
    html += "</div>";

    // Вибір джерела даних
    html += "<div style='background:#fff3cd;padding:20px;border-radius:8px;margin:20px 0;border-left:4px solid #ffc107;'>";
    html += "<strong>📁 Джерело даних:</strong><br>";
    html += "<div style='margin-top:10px;'>";
    html += "<label style='margin-right:20px;'><input type='radio' name='dataSource' value='ram' checked onchange='switchDataSource()'> 📊 RAM (останні 24 години)</label>";
    html += "<label><input type='radio' name='dataSource' value='spiffs' onchange='switchDataSource()'> 💾 SPIFFS (архів)</label>";
    html += "</div>";
    html += "<div id='dateSelector' style='display:none;margin-top:15px;'>";
    html += "<label style='margin-right:10px;'>Від: <input type='date' id='startDate' style='padding:5px;border-radius:4px;border:1px solid #ddd;'></label>";
    html += "<label style='margin-right:10px;'>До: <input type='date' id='endDate' style='padding:5px;border-radius:4px;border:1px solid #ddd;'></label>";
    html += "<button onclick='loadArchiveData()' style='padding:5px 15px;background:#2196f3;color:white;border:none;border-radius:4px;cursor:pointer;'>📥 Завантажити</button>";
    html += "</div>";
    html += "</div>";

    // Інструкції для масштабування
    html += "<div style='background:#e3f2fd;padding:15px;border-radius:8px;margin:20px 0;border-left:4px solid #2196f3;'>";
    html += "<strong>📊 Керування графіками:</strong><br>";
    html += "🖱️ <strong>Масштабування:</strong> прокрутка коліщатком миші / pinch на сенсорному екрані<br>";
    html += "👆 <strong>Переміщення:</strong> клік і перетягування графіка<br>";
    html += "🔄 <strong>Скидання:</strong> подвійний клік по графіку або кнопка нижче";
    html += "</div>";

    // Графік температури
    html += "<h2>🌡️ Температура <button onclick='tempChart.resetZoom()' style='float:right;padding:8px 16px;background:#4caf50;color:white;border:none;border-radius:5px;cursor:pointer;'>🔄 Скинути масштаб</button></h2>";
    html += "<div class='chart-container'><canvas id='tempChart'></canvas></div>";

    // Графік вологості
    html += "<h2>💧 Вологість <button onclick='humChart.resetZoom()' style='float:right;padding:8px 16px;background:#4caf50;color:white;border:none;border-radius:5px;cursor:pointer;'>🔄 Скинути масштаб</button></h2>";
    html += "<div class='chart-container'><canvas id='humChart'></canvas></div>";

    html += "<a href='/' class='back-btn'>← На головну</a>";
    html += "</div>";
    html += getUkraineMarquee();
    
    // JavaScript для графіків
    html += "<script>";
    html += "let tempChart, humChart;";

    // Завантажуємо дані з нового API
    html += "fetch('/history/data?source=ram&format=json')";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "const data = result.data || [];";
    html += "const labels = [];";
    html += "const tempCarrier = [];";
    html += "const tempRoom = [];";
    html += "const tempBME = [];";
    html += "const humidity = [];";

    html += "data.forEach(record => {";
    html += "if (record.timestamp > 0) {";
    html += "const date = new Date(record.timestamp * 1000);";
    html += "const hours = String(date.getHours()).padStart(2, '0');";
    html += "const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "labels.push(hours + ':' + minutes);";
    html += "tempCarrier.push(record.tempCarrier);";
    html += "tempRoom.push(record.tempRoom);";
    html += "tempBME.push(record.tempBME);";
    html += "humidity.push(record.humidity);";
    html += "}";
    html += "});";
    
    // Графік температури
    html += "tempChart = new Chart(document.getElementById('tempChart'), {";
    html += "type: 'line',";
    html += "data: {";
    html += "labels: labels,";
    html += "datasets: [{";
    html += "label: '🔥 Теплоносій',";
    html += "data: tempCarrier,";
    html += "borderColor: '#e74c3c',";
    html += "backgroundColor: 'rgba(231, 76, 60, 0.1)',";
    html += "tension: 0.4";
    html += "}, {";
    html += "label: '🏠 Кімната',";
    html += "data: tempRoom,";
    html += "borderColor: '#3498db',";
    html += "backgroundColor: 'rgba(52, 152, 219, 0.1)',";
    html += "tension: 0.4";
    html += "}, {";
    html += "label: '🌡️ BME280',";
    html += "data: tempBME,";
    html += "borderColor: '#2ecc71',";
    html += "backgroundColor: 'rgba(46, 204, 113, 0.1)',";
    html += "tension: 0.4";
    html += "}]},";
    html += "options: {";
    html += "responsive: true,";
    html += "maintainAspectRatio: false,";
    html += "interaction: { mode: 'index', intersect: false },";
    html += "plugins: {";
    html += "legend: { display: true, position: 'top' },";
    html += "tooltip: {";
    html += "callbacks: {";
    html += "title: function(ctx) { return ctx[0].label; },";
    html += "label: function(ctx) { return ctx.parsed.y.toFixed(1) + '°C'; }";
    html += "}},";
    html += "zoom: {";
    html += "zoom: {";
    html += "wheel: { enabled: true },";
    html += "pinch: { enabled: true },";
    html += "mode: 'x'";
    html += "},";
    html += "pan: { enabled: true, mode: 'x', modifierKey: null },";
    html += "limits: { x: { min: 'original', max: 'original' } }";
    html += "}},";
    html += "scales: { y: { beginAtZero: false, title: { display: true, text: '°C' } } }";
    html += "}});";

    // Графік вологості
    html += "humChart = new Chart(document.getElementById('humChart'), {";
    html += "type: 'line',";
    html += "data: {";
    html += "labels: labels,";
    html += "datasets: [{";
    html += "label: '💧 Вологість',";
    html += "data: humidity,";
    html += "borderColor: '#3498db',";
    html += "backgroundColor: 'rgba(52, 152, 219, 0.2)',";
    html += "fill: true,";
    html += "tension: 0.4";
    html += "}]},";
    html += "options: {";
    html += "responsive: true,";
    html += "maintainAspectRatio: false,";
    html += "interaction: { mode: 'index', intersect: false },";
    html += "plugins: {";
    html += "legend: { display: true, position: 'top' },";
    html += "tooltip: {";
    html += "callbacks: {";
    html += "title: function(ctx) { return ctx[0].label; },";
    html += "label: function(ctx) { return ctx.parsed.y.toFixed(1) + '%'; }";
    html += "}},";
    html += "zoom: {";
    html += "zoom: {";
    html += "wheel: { enabled: true },";
    html += "pinch: { enabled: true },";
    html += "mode: 'x'";
    html += "},";
    html += "pan: { enabled: true, mode: 'x', modifierKey: null },";
    html += "limits: { x: { min: 'original', max: 'original' } }";
    html += "}},";
    html += "scales: { y: { beginAtZero: false, max: 100, title: { display: true, text: '%' } } }";
    html += "}});";

    // Подвійний клік для скидання масштабу
    html += "document.getElementById('tempChart').ondblclick = function() { tempChart.resetZoom(); };";
    html += "document.getElementById('humChart').ondblclick = function() { humChart.resetZoom(); };";

    // Закриваємо fetch блок
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка завантаження даних:', error);";
    html += "alert('Помилка завантаження історичних даних. Перезавантажте сторінку.');";
    html += "});";

    // Функція переключення джерела даних
    html += "function switchDataSource() {";
    html += "const source = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "const dateSelector = document.getElementById('dateSelector');";
    html += "if (source === 'spiffs') {";
    html += "dateSelector.style.display = 'block';";
    html += "const today = new Date().toISOString().split('T')[0];";
    html += "const weekAgo = new Date(Date.now() - 7*24*60*60*1000).toISOString().split('T')[0];";
    html += "document.getElementById('endDate').value = today;";
    html += "document.getElementById('startDate').value = weekAgo;";
    html += "} else {";
    html += "dateSelector.style.display = 'none';";
    html += "location.reload();";
    html += "}";
    html += "}";

    // Функція завантаження архівних даних
    html += "function loadArchiveData() {";
    html += "const startDate = document.getElementById('startDate').value;";
    html += "const endDate = document.getElementById('endDate').value;";
    html += "if (!startDate || !endDate) {";
    html += "alert('Оберіть дати!');";
    html += "return;";
    html += "}";
    html += "fetch(`/history/data?source=spiffs&start=${startDate}&end=${endDate}&format=json`)";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "const data = result.data || [];";
    html += "if (data.length === 0) {";
    html += "alert('Немає даних за вибраний період');";
    html += "return;";
    html += "}";
    html += "const labels = [];";
    html += "const tempCarrier = [];";
    html += "const tempRoom = [];";
    html += "const tempBME = [];";
    html += "const humidity = [];";
    html += "data.forEach(record => {";
    html += "if (record.timestamp > 0) {";
    html += "const date = new Date(record.timestamp * 1000);";
    html += "const day = String(date.getDate()).padStart(2, '0');";
    html += "const month = String(date.getMonth() + 1).padStart(2, '0');";
    html += "const hours = String(date.getHours()).padStart(2, '0');";
    html += "const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "labels.push(day + '/' + month + ' ' + hours + ':' + minutes);";
    html += "tempCarrier.push(record.tempCarrier);";
    html += "tempRoom.push(record.tempRoom);";
    html += "tempBME.push(record.tempBME);";
    html += "humidity.push(record.humidity);";
    html += "}";
    html += "});";
    html += "tempChart.data.labels = labels;";
    html += "tempChart.data.datasets[0].data = tempCarrier;";
    html += "tempChart.data.datasets[1].data = tempRoom;";
    html += "tempChart.data.datasets[2].data = tempBME;";
    html += "tempChart.update();";
    html += "humChart.data.labels = labels;";
    html += "humChart.data.datasets[0].data = humidity;";
    html += "humChart.update();";
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка:', error);";
    html += "alert('Помилка завантаження архівних даних');";
    html += "});";
    html += "}";

    html += "</script>";

    html += "</body></html>";
    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА ДОПОМОГИ
// ============================================================================

void handleHelpPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>📖 Допомога</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 900px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; border-bottom: 3px solid #9c27b0; padding-bottom: 15px; }";
    html += "h2 { color: #9c27b0; margin-top: 30px; }";
    html += "h3 { color: #666; margin-top: 20px; }";
    html += ".section { background: #f9f9f9; padding: 20px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #9c27b0; }";
    html += ".tip { background: #e8f5e9; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #4caf50; }";
    html += ".warning { background: #fff3cd; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #ffc107; }";
    html += ".danger { background: #ffebee; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #f44336; }";
    html += ".code { background: #263238; color: #aed581; padding: 10px; border-radius: 5px; font-family: monospace; margin: 10px 0; }";
    html += ".faq-item { margin: 20px 0; }";
    html += ".faq-q { font-weight: 600; color: #9c27b0; font-size: 1.1em; margin-bottom: 8px; }";
    html += ".faq-a { color: #555; line-height: 1.6; margin-left: 20px; }";
    html += ".back-btn { display: inline-block; background: #4CAF50; color: white; padding: 12px 24px; text-decoration: none; border-radius: 5px; margin-top: 20px; }";
    html += "ul, ol { line-height: 1.8; }";
    html += "code { background: #f5f5f5; padding: 2px 6px; border-radius: 3px; font-family: monospace; color: #e91e63; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<h1>📖 ДОВІДКА - КЛІМАТ-КОНТРОЛЬ</h1>";
    
    // Швидкі відповіді
    html += "<h2>❓ Найчастіші питання (FAQ)</h2>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>📱 Не працює з телефону Android?</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Проблема:</strong> Android не підтримує mDNS (адреси типу <code>klimat.local</code>)<br>";
    html += "<strong>Рішення:</strong><br>";
    html += "1️⃣ Використовуйте IP-адресу: <code>" + htmlEscape(WiFi.localIP().toString()) + "</code><br>";
    html += "2️⃣ Відскануйте QR-код на головній сторінці (з ноутбука)<br>";
    html += "3️⃣ Встановіть додаток BonjourBrowser для підтримки mDNS";
    html += "</div></div>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>💻 Не працює з іншої мережі WiFi?</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Проблема:</strong> Ваш пристрій і ESP32 в різних мережах<br>";
    html += "<strong>Рішення:</strong><br>";
    html += "1️⃣ Підключіть обидва до однієї WiFi мережі<br>";
    html += "2️⃣ Або налаштуйте доступ через інтернет (WiFi → Налаштування → Інтернет-доступ)<br>";
    html += "<div class='tip'>💡 Поточна мережа ESP32: <strong>" + htmlEscape(WiFi.SSID()) + "</strong></div>";
    html += "</div></div>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>🔐 Як захистити від сторонніх?</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Рішення:</strong><br>";
    html += "1️⃣ Перейдіть: WiFi → Налаштування<br>";
    html += "2️⃣ Увімкніть автентифікацію<br>";
    html += "3️⃣ Встановіть складний пароль<br>";
    html += "<div class='warning'>⚠️ Обов'язково для інтернет-доступу!</div>";
    html += "</div></div>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>❓ Що означають іконки \"?\"</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Як використовувати:</strong><br>";
    html += "Наведіть курсор миші (або торкніться на сенсорному екрані) на фіолетову іконку \"?\" біля налаштувань<br>";
    html += "З'явиться підказка з поясненням що робить це налаштування<br>";
    html += "<div class='tip'>💡 Шукайте такі іконки на сторінці WiFi налаштувань!</div>";
    html += "</div></div>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>🌐 IP-адреса постійно змінюється</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Рішення:</strong> Налаштуйте статичну IP<br>";
    html += "1️⃣ WiFi → Налаштування<br>";
    html += "2️⃣ Встановіть галочку \"Використовувати статичну IP\"<br>";
    html += "3️⃣ Введіть вільну IP з вашої мережі (наприклад 192.168.1.100)<br>";
    html += "4️⃣ Шлюз = адреса роутера (зазвичай 192.168.1.1)<br>";
    html += "5️⃣ Збережіть та перезавантажте ESP32";
    html += "</div></div>";
    
    // Доступ до системи
    html += "<h2>🔗 Доступ до системи</h2>";
    html += "<div class='section'>";
    html += "<h3>З ноутбука (Windows/Mac/Linux):</h3>";
    html += "<div class='code'>http://klimat.local</div>";
    html += "<p>✅ Працює в будь-якій мережі автоматично</p>";
    
    html += "<h3>З телефону iOS (iPhone/iPad):</h3>";
    html += "<div class='code'>http://klimat.local</div>";
    html += "<p>✅ Працює в будь-якій мережі автоматично</p>";
    
    html += "<h3>З телефону Android:</h3>";
    html += "<div class='code'>http://" + htmlEscape(WiFi.localIP().toString()) + "</div>";
    html += "<p>⚠️ Використовуйте IP-адресу (Android не підтримує mDNS)</p>";
    
    html += "<div class='tip'>";
    html += "<strong>💡 Корисно:</strong><br>";
    html += "• Додайте сторінку в закладки для швидкого доступу<br>";
    html += "• На Android: додайте на головний екран (Chrome → Меню → Додати на головний екран)<br>";
    html += "• Відскануйте QR-код на головній сторінці";
    html += "</div>";
    html += "</div>";
    
    // Статична IP
    html += "<h2>⚙️ Налаштування статичної IP</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Навіщо потрібно:</strong> ESP32 завжди матиме одну IP-адресу в домашній мережі</p>";
    html += "<p><strong>Як налаштувати:</strong></p>";
    html += "<ol>";
    html += "<li>WiFi → Налаштування</li>";
    html += "<li>☑️ Використовувати статичну IP</li>";
    html += "<li>Заповніть поля (наведіть на \"?\" для підказок)</li>";
    html += "<li>Збережіть та перезавантажте</li>";
    html += "</ol>";
    html += "<p><strong>Як дізнатися параметри:</strong></p>";
    html += "<div class='code'>";
    html += "Windows: ipconfig<br>";
    html += "Linux/Mac: ip route | grep default";
    html += "</div>";
    html += "</div>";
    
    // Безпека
    html += "<h2>🔐 Безпека</h2>";
    html += "<div class='section'>";
    html += "<div class='danger'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Обов'язково увімкніть автентифікацію якщо налаштовуєте доступ через інтернет!";
    html += "</div>";
    html += "<p><strong>Як увімкнути:</strong></p>";
    html += "<ol>";
    html += "<li>WiFi → Налаштування</li>";
    html += "<li>☑️ Увімкнути автентифікацію</li>";
    html += "<li>Встановіть логін (за замовчуванням: admin)</li>";
    html += "<li>Встановіть СКЛАДНИЙ пароль (не 12345!)</li>";
    html += "<li>Збережіть налаштування</li>";
    html += "</ol>";
    html += "<div class='tip'>";
    html += "<strong>💡 Вимоги до пароля:</strong><br>";
    html += "• Мінімум 8 символів<br>";
    html += "• Використовуйте букви, цифри, спецсимволи<br>";
    html += "• НЕ використовуйте: 12345, password, admin, qwerty";
    html += "</div>";
    html += "</div>";
    
    // Доступ через інтернет
    html += "<h2>🌐 Доступ через інтернет</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Швидкі кроки:</strong></p>";
    html += "<ol>";
    html += "<li>✅ Увімкніть автентифікацію (див. вище)</li>";
    html += "<li>🔧 Налаштуйте Port Forwarding на роутері</li>";
    html += "<li>🌍 Дізнайтесь зовнішню IP на <a href='https://myip.com.ua' target='_blank'>myip.com.ua</a></li>";
    html += "<li>🚀 Підключайтесь: http://[ваша_IP]:8080</li>";
    html += "</ol>";
    html += "<p><strong>Port Forwarding:</strong></p>";
    html += "<div class='code'>";
    html += "Внутрішня IP: " + htmlEscape(WiFi.localIP().toString()) + "<br>";
    html += "Внутрішній порт: 80<br>";
    html += "Зовнішній порт: 8080";
    html += "</div>";
    html += "<div class='warning'>";
    html += "⚠️ Детальну інструкцію для різних роутерів дивіться на сторінці WiFi → Налаштування";
    html += "</div>";
    html += "</div>";
    
    // Усунення проблем
    html += "<h2>🔧 Усунення проблем</h2>";
    html += "<div class='section'>";
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>📱 Не можу підключитися з Android телефону</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Android НЕ підтримує klimat.local!</strong><br><br>";
    html += "✅ <strong>Рішення:</strong><br>";
    html += "1. Використовуйте IP адресу: <span class='code' style='background:#fff;padding:2px 6px;'>" + WiFi.localIP().toString() + "</span><br>";
    html += "2. Відскануйте QR-код на головній сторінці<br>";
    html += "3. Збережіть IP в закладки або додайте на головний екран<br>";
    html += "4. Переконайтесь що телефон в тій самій WiFi мережі: <strong>" + htmlEscape(WiFi.SSID()) + "</strong><br><br>";
    html += "<strong>💡 Порада:</strong> Налаштуйте статичну IP в розділі WiFi, щоб адреса не змінювалась";
    html += "</div></div>";

    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>Сторінка не відкривається взагалі</div>";
    html += "<div class='faq-a'>";
    html += "1. Перевірте підключення ESP32 до WiFi (подивіться Serial Monitor)<br>";
    html += "2. <strong>Переконайтесь що пристрій в тій же мережі WiFi!</strong><br>";
    html += "3. Спробуйте перезавантажити ESP32<br>";
    html += "4. На Android - використовуйте тільки IP адресу<br>";
    html += "5. На iOS/Mac - спробуйте і klimat.local і IP";
    html += "</div></div>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>Повільно працює або зависає</div>";
    html += "<div class='faq-a'>";
    html += "1. Перевірте сигнал WiFi (має бути >-70 dBm)<br>";
    html += "2. Підійдіть ближче до роутера<br>";
    html += "3. Перезавантажте роутер та ESP32<br>";
    html += "4. Перевірте що мережа не перевантажена";
    html += "</div></div>";
    
    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>Запитує пароль хоча я його не встановлював</div>";
    html += "<div class='faq-a'>";
    html += "Автентифікація увімкнена. За замовчуванням:<br>";
    html += "Логін: <code>admin</code><br>";
    html += "Пароль: <code>12345</code><br>";
    html += "Змініть пароль в WiFi → Налаштування!";
    html += "</div></div>";
    html += "</div>";
    
    // Корисні поради
    html += "<h2>💡 Корисні поради</h2>";
    html += "<div class='section'>";
    html += "<ul>";
    html += "<li>📱 <strong>QR-код:</strong> На головній сторінці є QR-код для швидкого доступу з телефону</li>";
    html += "<li>🔖 <strong>Закладки:</strong> Додайте сторінку в закладки браузера</li>";
    html += "<li>🏠 <strong>Головний екран:</strong> На мобільному можна додати ярлик на робочий стіл</li>";
    html += "<li>📊 <strong>Історія:</strong> Система зберігає останні 24 години даних</li>";
    html += "<li>🤖 <strong>Авто-режим:</strong> Система сама підтримує температуру та вологість</li>";
    html += "<li>🔄 <strong>Оновлення:</strong> Дані оновлюються автоматично кожні 5 секунд</li>";
    html += "</ul>";
    html += "</div>";
    
    // Контакти
    html += "<h2>📞 Додаткова інформація</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Поточний стан системи:</strong></p>";
    html += "<ul>";
    html += "<li>Мережа: <strong>" + htmlEscape(WiFi.SSID()) + "</strong></li>";
    html += "<li>IP-адреса: <strong>" + htmlEscape(WiFi.localIP().toString()) + "</strong></li>";
    html += "<li>mDNS: <strong>klimat.local</strong></li>";
    html += "<li>Сигнал: <strong>" + String(WiFi.RSSI()) + " dBm</strong></li>";
    html += "<li>Версія: <strong>" VERSION "</strong></li>";
    html += "<li>Дата збірки: <strong>" BUILD_DATE " " BUILD_TIME "</strong></li>";
    html += "<li>Рядків коду: <strong>" + String(TOTAL_CODE_LINES) + "</strong></li>";
    html += "<li>Розмір прошивки: <strong>" + String(FIRMWARE_SIZE_KB) + " KB</strong></li>";
    html += "</ul>";
    html += "</div>";
    
    // Керування системою
    html += "<h2>🎛️ Керування системою</h2>";
    html += "<div class='section'>";
    html += "<h3>Режими роботи:</h3>";
    html += "<ul>";
    html += "<li><strong>🤖 АВТО:</strong> Система автоматично підтримує задані температуру та вологість</li>";
    html += "<li><strong>🔥 ОБІГРІВ:</strong> Тільки обігрів, охолодження вимкнено</li>";
    html += "<li><strong>❄️ ОХОЛОДЖЕННЯ:</strong> Тільки охолодження, обігрів вимкнено</li>";
    html += "<li><strong>💧 ЗВОЛОЖЕННЯ:</strong> Тільки зволоження повітря</li>";
    html += "<li><strong>🌬️ ОСУШЕННЯ:</strong> Тільки видалення вологості</li>";
    html += "<li><strong>🌀 ВЕНТИЛЯЦІЯ:</strong> Тільки циркуляція повітря</li>";
    html += "<li><strong>⏸️ ВИМКНЕНО:</strong> Всі пристрої вимкнені</li>";
    html += "</ul>";
    
    html += "<h3>🎚️ Регулювання:</h3>";
    html += "<p><strong>Температура:</strong> 10-35°C (рекомендовано: 20-24°C)</p>";
    html += "<p><strong>Вологість:</strong> 30-80% (рекомендовано: 40-60%)</p>";
    html += "<p><strong>Швидкість вентилятора:</strong> 0-100%</p>";
    
    html += "<div class='tip'>";
    html += "💡 <strong>Гістерезис:</strong> Система має \"мертву зону\" ±1°C та ±5% для запобігання частому перемиканню";
    html += "</div>";
    html += "</div>";
    
    // Датчики
    html += "<h2>📊 Датчики</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Система використовує:</strong></p>";
    html += "<ul>";
    html += "<li><strong>BME280:</strong> Температура, вологість, атмосферний тиск (всередині)</li>";
    html += "<li><strong>DS18B20:</strong> Температура (зовні/додатково)</li>";
    html += "</ul>";
    
    html += "<h3>🔍 Що означають показники:</h3>";
    html += "<ul>";
    html += "<li><strong>Температура:</strong> Поточна температура повітря в °C</li>";
    html += "<li><strong>Вологість:</strong> Відносна вологість повітря в %</li>";
    html += "<li><strong>Тиск:</strong> Атмосферний тиск в гПа (мм рт.ст.)</li>";
    html += "<li><strong>Потужність:</strong> Поточна потужність обігрівача 0-100%</li>";
    html += "</ul>";
    
    html += "<div class='warning'>";
    html += "⚠️ Якщо датчик показує -127°C або 0% - перевірте підключення датчика";
    html += "</div>";
    html += "</div>";
    
    // Налаштування
    html += "<h2>⚙️ Основні налаштування</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Сторінка Налаштування:</strong></p>";
    html += "<ul>";
    html += "<li><strong>Цільова температура:</strong> Яку температуру підтримувати</li>";
    html += "<li><strong>Цільова вологість:</strong> Яку вологість підтримувати</li>";
    html += "<li><strong>Гістерезис:</strong> \"Мертва зона\" для запобігання частому вмиканню/вимиканню</li>";
    html += "<li><strong>Період оновлення:</strong> Як часто зчитувати датчики (2-60 сек)</li>";
    html += "<li><strong>Калібрування:</strong> Корекція показань датчиків (якщо потрібно)</li>";
    html += "</ul>";
    
    html += "<div class='tip'>";
    html += "💡 Після зміни налаштувань натисніть <strong>\"Зберегти\"</strong> - вони зберігаються навіть після вимкнення живлення";
    html += "</div>";
    html += "</div>";
    
    // Історія даних
    html += "<h2>📈 Історія даних</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Що зберігається:</strong></p>";
    html += "<ul>";
    html += "<li>Температура (внутрішня та зовнішня)</li>";
    html += "<li>Вологість</li>";
    html += "<li>Атмосферний тиск</li>";
    html += "<li>Режим роботи системи</li>";
    html += "</ul>";
    
    html += "<p><strong>Період зберігання:</strong> Останні 24 години (288 записів по 5 хвилин)</p>";
    
    html += "<div class='warning'>";
    html += "⚠️ При вимкненні живлення історія очищується (дані зберігаються тільки в оперативній пам'яті)";
    html += "</div>";
    
    html += "<p><strong>Як переглянути:</strong></p>";
    html += "<ol>";
    html += "<li>Перейдіть на сторінку <strong>Історія</strong></li>";
    html += "<li>Виберіть період перегляду</li>";
    html += "<li>Графіки покажуть зміну параметрів за обраний час</li>";
    html += "<li>Натисніть <strong>\"Оновити\"</strong> для актуалізації даних</li>";
    html += "</ol>";
    html += "</div>";
    
    // Навчання системи
    html += "<h2>🧠 Самонавчання</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Що робить:</strong> Система аналізує ваші налаштування і адаптується до звичок</p>";
    
    html += "<ul>";
    html += "<li>Запам'ятовує улюблені режими та налаштування</li>";
    html += "<li>Оптимізує енергоспоживання</li>";
    html += "<li>Пропонує рекомендації на основі історії</li>";
    html += "</ul>";
    
    html += "<div class='tip'>";
    html += "💡 Функція активується автоматично після тижня використання системи";
    html += "</div>";
    html += "</div>";
    
    // Сервоприводи
    html += "<h2>🎚️ Сервоприводи (заслінки)</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Призначення:</strong> Керування повітряними заслінками для розподілу повітря</p>";
    
    html += "<p><strong>Налаштування кута:</strong></p>";
    html += "<ul>";
    html += "<li>0° - заслінка закрита</li>";
    html += "<li>90° - заслінка відкрита наполовину</li>";
    html += "<li>180° - заслінка повністю відкрита</li>";
    html += "</ul>";
    
    html += "<p><strong>Режими:</strong></p>";
    html += "<ul>";
    html += "<li><strong>Ручний:</strong> Встановіть кут вручну повзунком</li>";
    html += "<li><strong>Вимикач:</strong> Відкрити (180°) або закрити (0°)</li>";
    html += "</ul>";
    
    html += "<div class='warning'>";
    html += "⚠️ Не змінюйте кут надто часто - це зменшує термін служби сервоприводу";
    html += "</div>";
    html += "</div>";
    
    // Енергозбереження
    html += "<h2>⚡ Енергозбереження</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Поради для економії електроенергії:</strong></p>";
    html += "<ul>";
    html += "<li>🎯 Використовуйте режим <strong>АВТО</strong> - він найефективніший</li>";
    html += "<li>🌡️ Не встановлюйте занадто високу/низьку температуру</li>";
    html += "<li>💨 Регулюйте швидкість вентилятора - не завжди потрібна максимальна</li>";
    html += "<li>🔄 Збільште гістерезис до 2-3°C для рідших перемикань</li>";
    html += "<li>🏠 Утеплюйте приміщення для зменшення втрат тепла/холоду</li>";
    html += "<li>📊 Аналізуйте історію споживання для оптимізації</li>";
    html += "</ul>";
    html += "</div>";
    
    // Техобслуговування
    html += "<h2>🔧 Технічне обслуговування</h2>";
    html += "<div class='section'>";
    html += "<p><strong>Регулярне обслуговування:</strong></p>";
    html += "<ul>";
    html += "<li>🧹 <strong>Раз на місяць:</strong> Очистіть датчики від пилу</li>";
    html += "<li>🌀 <strong>Раз на 3 місяці:</strong> Перевірте вентилятори та заслінки</li>";
    html += "<li>📏 <strong>Раз на 6 місяців:</strong> Калібруйте датчики температури/вологості</li>";
    html += "<li>🔌 <strong>За потреби:</strong> Перевіряйте з'єднання проводів</li>";
    html += "</ul>";
    
    html += "<div class='danger'>";
    html += "⚠️ <strong>УВАГА:</strong> Перед будь-яким обслуговуванням вимкніть живлення!";
    html += "</div>";
    html += "</div>";
    
    html += "<div style='text-align: center; margin-top: 30px;'>";
    html += "<a href='/' class='back-btn'>← Повернутись на головну</a>";
    html += "</div>";
    
    html += "</div>";
    html += getUkraineMarquee();
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА НАЛАГОДЖЕННЯ
// ============================================================================

void handleDebugPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "";
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 800px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>🔧 ВІДЛАДКОВА ІНФОРМАЦІЯ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>Вільна пам'ять:</strong> " + String(ESP.getFreeHeap() / 1024) + " KB</p>";
    html += "<p><strong>Всього пам'яті:</strong> " + String(ESP.getHeapSize() / 1024) + " KB</p>";
    html += "<p><strong>Задач FreeRTOS:</strong> " + String(uxTaskGetNumberOfTasks()) + "</p>";
    html += "<p><strong>Час роботи:</strong> " + String(millis() / 1000) + " секунд</p>";
    html += "<p><strong>Температура чіпа:</strong> " + String(temperatureRead()) + "°C</p>";
    html += "<p><strong>Частота CPU:</strong> " + String(getCpuFrequencyMhz()) + " MHz</p>";
    html += "<p><strong>Версія SDK:</strong> " + String(ESP.getSdkVersion()) + "</p>";
    html += "</div>";
    html += "<p><a href='/'>← На головну</a></p>";
    html += "</div>";
    html += getUkraineMarquee();
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА СИСТЕМИ НАВЧАННЯ
// ============================================================================

void handleLearningPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "";
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 800px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>🧠 СИСТЕМА НАВЧАННЯ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>Записів:</strong> " + String(learningCount) + "</p>";
    html += "<p><strong>Статус:</strong> " + String(learningEnabled ? "Включено" : "Вимкнено") + "</p>";
    html += "<p><strong>Активно:</strong> " + String(isLearningActive ? "Так" : "Ні") + "</p>";
    html += "</div>";
    html += "<p><a href='/'>← На головну</a></p>";
    html += "</div>";
    html += getUkraineMarquee();
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
// SERVO CALIBRATION PAGE
// ============================================================================

void handleServoPage() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Калібрування серво</title>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; }";
    html += ".status { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 15px 0; }";
    html += ".status-item { display: flex; justify-content: space-between; padding: 5px 0; }";
    html += ".buttons { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin: 20px 0; }";
    html += ".btn { padding: 15px; font-size: 18px; border: none; border-radius: 5px; cursor: pointer; transition: all 0.3s; }";
    html += ".btn:active { transform: scale(0.95); }";
    html += ".btn-small { background: #2196F3; color: white; }";
    html += ".btn-big { background: #FF9800; color: white; }";
    html += ".btn-save { background: #4CAF50; color: white; grid-column: span 2; }";
    html += ".btn-test { background: #9C27B0; color: white; grid-column: span 2; }";
    html += ".angle-display { font-size: 48px; font-weight: 600; text-align: center; color: #2196F3; margin: 20px 0; }";
    html += ".nav { text-align: center; margin-top: 20px; }";
    html += ".nav a { color: #2196F3; text-decoration: none; margin: 0 10px; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>⚙ Калібрування серво</h1>";
    
    html += "<div class='status'>";
    html += "<div class='status-item'><span>Поточний кут:</span><span id='current'>" + String(ventState.currentAngle) + "°</span></div>";
    html += "<div class='status-item'><span>Закрито:</span><span id='closed'>" + String(config.servoClosedAngle) + "°</span></div>";
    html += "<div class='status-item'><span>Відкрито:</span><span id='open'>" + String(config.servoOpenAngle) + "°</span></div>";
    html += "<div class='status-item'><span>Вимикач:</span><span id='switch' style='font-weight:600;color:";
    html += ventState.switchState ? "#4CAF50'>УВІМКНЕНО" : "#f44336'>ВИМКНЕНО";
    html += "</span></div>";
    html += "<div class='status-item'><span>Режим:</span><span id='mode' style='font-weight:600;color:";
    html += ventState.calibrationMode ? "#FF9800'>КАЛІБРУВАННЯ" : "#2196F3'>НОРМАЛЬНИЙ";
    html += "</span></div>";
    html += "</div>";
    
    html += "<div class='angle-display' id='angle'>" + String(ventState.currentAngle) + "°</div>";
    
    html += "<div class='buttons'>";
    html += "<button class='btn' style='background:#FF9800;color:white;grid-column:span 2;' onclick='toggleCalibration()' id='calibBtn'>";
    html += ventState.calibrationMode ? "🔓 ВИЙТИ З КАЛІБРУВАННЯ" : "🔒 УВІЙТИ В КАЛІБРУВАННЯ";
    html += "</button>";
    html += "<button class='btn' style='background:#9C27B0;color:white;grid-column:span 2;' onclick='autoCalibrate()'>🤖 АВТОКАЛІБРУВАННЯ</button>";
    html += "<button class='btn btn-small' onclick='moveServo(\"+1\")'>▲ +1°</button>";
    html += "<button class='btn btn-big' onclick='moveServo(\"+5\")'>▲▲ +5°</button>";
    html += "<button class='btn btn-small' onclick='moveServo(\"-1\")'>▼ -1°</button>";
    html += "<button class='btn btn-big' onclick='moveServo(\"-5\")'>▼▼ -5°</button>";
    html += "<button class='btn' style='background:#4CAF50;color:white;' onclick='gotoPosition(\"open\")'>➤ Відкрити</button>";
    html += "<button class='btn' style='background:#f44336;color:white;' onclick='gotoPosition(\"closed\")'>➤ Закрити</button>";
    html += "<button class='btn btn-save' onclick='savePosition(\"closed\")'>💾 Зберегти як ЗАКРИТО</button>";
    html += "<button class='btn btn-save' onclick='savePosition(\"open\")'>💾 Зберегти як ВІДКРИТО</button>";
    html += "<button class='btn btn-test' onclick='testServo()'>🔧 Тест</button>";
    html += "</div>";
    
    html += "<div class='nav'>";
    html += "<a href='/'>🏠 Головна</a>";
    html += "<a href='/control'>🎮 Управління</a>";
    html += "<a href='/settings'>⚙ Налаштування</a>";
    html += "</div>";
    
    html += "</div>";
    
    html += "<script>";
    html += "function sendCommand(cmd) {";
    html += "  fetch('/servo/api', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'cmd=' + cmd";
    html += "  }).then(r => r.text()).then(data => {";
    html += "    if(data.startsWith('ANGLE:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('angle').innerText = angle + '°';";
    html += "      document.getElementById('current').innerText = angle + '°';";
    html += "    } else if(data.startsWith('CLOSED:')) {";
    html += "      document.getElementById('closed').innerText = data.split(':')[1] + '°';";
    html += "      alert('✓ Закрите положення збережено');";
    html += "    } else if(data.startsWith('OPEN:')) {";
    html += "      document.getElementById('open').innerText = data.split(':')[1] + '°';";
    html += "      alert('✓ Відкрите положення збережено');";
    html += "    } else if(data == 'TEST_OK') {";
    html += "      alert('✓ Тест завершено');";
    html += "      setTimeout(() => location.reload(), 1000);";
    html += "    }";
    html += "  });";
    html += "}";
    html += "function moveServo(delta) { sendCommand('move:' + delta); }";
    html += "function savePosition(type) { sendCommand('save:' + type); }";
    html += "function gotoPosition(type) { sendCommand('goto:' + type); }";
    html += "function toggleCalibration() { sendCommand('calibration:toggle'); location.reload(); }";
    html += "function autoCalibrate() { if(confirm('Автокалібрування: швидко перемикайте вимикач для зміни напряму. Продовжити?')) { sendCommand('auto:calibrate'); alert('Швидко перемикайте вимикач!'); setTimeout(() => location.reload(), 8000); } }";
    html += "function testServo() { if(confirm('Тест відкриє і закриє заслонку. Продовжити?')) sendCommand('test'); }";
    html += "setInterval(() => location.reload(), 5000);";
    html += "</script>";
    html += "<div style='text-align: center; margin-top: 30px;'><a href='/' style='background: #4CAF50; color: white; padding: 12px 24px; text-decoration: none; border-radius: 5px; display: inline-block;'>← На головну</a></div>";
    html += getUkraineMarquee();
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleServoAPI() {
    if (!server.hasArg("cmd")) {
        server.send(400, "text/plain", "No command");
        return;
    }
    
    String cmd = server.arg("cmd");
    Serial.println("WEB SERVO CMD: " + cmd);
    
    if (cmd.startsWith("move:")) {
        int delta = cmd.substring(5).toInt();
        int newAngle = constrain(ventState.currentAngle + delta, 0, 180);
        moveServoSmooth(newAngle);
        server.send(200, "text/plain", "ANGLE:" + String(newAngle));
    }
    else if (cmd == "goto:open") {
        moveServoSmooth(config.servoOpenAngle);
        server.send(200, "text/plain", "ANGLE:" + String(config.servoOpenAngle));
    }
    else if (cmd == "goto:closed") {
        moveServoSmooth(config.servoClosedAngle);
        server.send(200, "text/plain", "ANGLE:" + String(config.servoClosedAngle));
    }
    else if (cmd == "calibration:toggle") {
        ventState.calibrationMode = !ventState.calibrationMode;
        Serial.printf("Режим калібрування: %s\n", ventState.calibrationMode ? "УВІМКНЕНО" : "ВИМКНЕНО");
        server.send(200, "text/plain", ventState.calibrationMode ? "CALIB:ON" : "CALIB:OFF");
    }
    else if (cmd == "auto:calibrate") {
        startAutoCalibration();
        server.send(200, "text/plain", "AUTO_CALIB_STARTED");
    }
    else if (cmd == "save:closed") {
        config.servoClosedAngle = ventState.currentAngle;
        saveConfiguration();
        server.send(200, "text/plain", "CLOSED:" + String(config.servoClosedAngle));
    }
    else if (cmd == "save:open") {
        config.servoOpenAngle = ventState.currentAngle;
        saveConfiguration();
        server.send(200, "text/plain", "OPEN:" + String(config.servoOpenAngle));
    }
    else if (cmd == "test") {
        moveServoSmooth(config.servoOpenAngle);
        delay(2000);
        moveServoSmooth(config.servoClosedAngle);
        server.send(200, "text/plain", "TEST_OK");
    }
    else {
        server.send(400, "text/plain", "Unknown command");
    }
}

// ============================================================================
// API ДЛЯ ІСТОРИЧНИХ ДАНИХ
// ============================================================================

// API: Отримання даних за період
void handleHistoryData() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "application/json", "{\"error\":\"WiFi не підключено\"}");
    return;
  }

  // Отримуємо параметри запиту
  String source = server.arg("source");  // "ram" або "spiffs"
  String startDate = server.arg("start");
  String endDate = server.arg("end");
  String format = server.arg("format");  // "json" або "csv"

  if (source == "ram") {
    // Дані з RAM - використовуємо chunked transfer для економії пам'яті
    LoggerStats stats = getLoggerStats();
    uint16_t totalRecords = (stats.totalRecordsRAM > 1440) ? 1440 : stats.totalRecordsRAM;

    if (format == "csv") {
      // CSV формат - потокова передача
      server.sendHeader("Content-Disposition", "attachment; filename=klimat_data.csv");
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.send(200, "text/csv", "");

      // Відправляємо заголовок
      server.sendContent("timestamp,tempCarrier,tempRoom,tempBME,humidity,pumpPower,fanPower,extractorPower,mode\n");

      // Відправляємо дані порціями по 50 записів
      const uint16_t CHUNK_SIZE = 50;
      DataRecord chunk[CHUNK_SIZE];

      for (uint16_t offset = 0; offset < totalRecords; offset += CHUNK_SIZE) {
        uint16_t chunkCount = (totalRecords - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalRecords - offset);

        // Читаємо порцію даних
        if (readRAMDataChunk(chunk, offset, chunkCount)) {
          String csvChunk = "";
          csvChunk.reserve(chunkCount * 80);  // Приблизний розмір рядка

          for (uint16_t i = 0; i < chunkCount; i++) {
            if (chunk[i].timestamp > 0) {
              csvChunk += String(chunk[i].timestamp) + ",";
              csvChunk += String(chunk[i].tempCarrier, 1) + ",";
              csvChunk += String(chunk[i].tempRoom, 1) + ",";
              csvChunk += String(chunk[i].tempBME, 1) + ",";
              csvChunk += String(chunk[i].humidity, 1) + ",";
              csvChunk += String(chunk[i].pumpPower) + ",";
              csvChunk += String(chunk[i].fanPower) + ",";
              csvChunk += String(chunk[i].extractorPower) + ",";
              csvChunk += String(chunk[i].mode) + "\n";
            }
          }
          server.sendContent(csvChunk);
          yield();  // Даємо час watchdog
        }
      }
      server.sendContent("");  // Завершуємо передачу

    } else {
      // JSON формат - потокова передача
      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.send(200, "application/json", "");

      server.sendContent("{\"data\":[");

      // Відправляємо дані порціями по 50 записів
      const uint16_t CHUNK_SIZE = 50;
      DataRecord chunk[CHUNK_SIZE];
      bool firstRecord = true;

      for (uint16_t offset = 0; offset < totalRecords; offset += CHUNK_SIZE) {
        uint16_t chunkCount = (totalRecords - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalRecords - offset);

        // Читаємо порцію даних
        if (readRAMDataChunk(chunk, offset, chunkCount)) {
          String jsonChunk = "";
          jsonChunk.reserve(chunkCount * 120);  // Приблизний розмір JSON об'єкта

          for (uint16_t i = 0; i < chunkCount; i++) {
            if (chunk[i].timestamp > 0) {
              if (!firstRecord) jsonChunk += ",";
              jsonChunk += "{";
              jsonChunk += "\"timestamp\":" + String(chunk[i].timestamp) + ",";
              jsonChunk += "\"tempCarrier\":" + String(chunk[i].tempCarrier, 1) + ",";
              jsonChunk += "\"tempRoom\":" + String(chunk[i].tempRoom, 1) + ",";
              jsonChunk += "\"tempBME\":" + String(chunk[i].tempBME, 1) + ",";
              jsonChunk += "\"humidity\":" + String(chunk[i].humidity, 1) + ",";
              jsonChunk += "\"pumpPower\":" + String(chunk[i].pumpPower) + ",";
              jsonChunk += "\"fanPower\":" + String(chunk[i].fanPower) + ",";
              jsonChunk += "\"extractorPower\":" + String(chunk[i].extractorPower) + ",";
              jsonChunk += "\"mode\":" + String(chunk[i].mode);
              jsonChunk += "}";
              firstRecord = false;
            }
          }
          server.sendContent(jsonChunk);
          yield();  // Даємо час watchdog
        }
      }

      server.sendContent("]}");
      server.sendContent("");  // Завершуємо передачу
    }

  } else if (source == "spiffs") {
    // Дані з SPIFFS (агреговані)
    if (startDate.length() == 0 || endDate.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Потрібні параметри start та end\"}");
      return;
    }

    String data;
    if (format == "csv") {
      if (readSPIFFSDataCSV(startDate.c_str(), endDate.c_str(), data)) {
        server.sendHeader("Content-Disposition", "attachment; filename=klimat_archive.csv");
        server.send(200, "text/csv", data);
      } else {
        server.send(500, "application/json", "{\"error\":\"Помилка читання SPIFFS\"}");
      }
    } else {
      if (readSPIFFSData(startDate.c_str(), endDate.c_str(), data)) {
        server.send(200, "application/json", data);
      } else {
        server.send(500, "application/json", "{\"error\":\"Помилка читання SPIFFS\"}");
      }
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Невірний параметр source\"}");
  }
}

// API: Статистика логування
void handleHistoryStats() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "application/json", "{\"error\":\"WiFi не підключено\"}");
    return;
  }

  LoggerStats stats = getLoggerStats();

  String json = "{";
  json += "\"totalRecordsRAM\":" + String(stats.totalRecordsRAM) + ",";
  json += "\"totalRecordsSPIFFS\":" + String(stats.totalRecordsSPIFFS) + ",";
  json += "\"lastLogTimeRAM\":" + String(stats.lastLogTimeRAM) + ",";
  json += "\"lastLogTimeSPIFFS\":" + String(stats.lastLogTimeSPIFFS) + ",";
  json += "\"currentFileSize\":" + String(stats.currentFileSize) + ",";
  json += "\"archiveFilesCount\":" + String(stats.archiveFilesCount) + ",";
  json += "\"spiffsUsedBytes\":" + String(stats.spiffsUsedBytes) + ",";
  json += "\"spiffsTotalBytes\":" + String(stats.spiffsTotalBytes) + ",";
  json += "\"spiffsUsedPercent\":" + String((stats.spiffsUsedBytes * 100.0) / stats.spiffsTotalBytes, 1);
  json += "}";

  server.send(200, "application/json", json);
}

// API: Експорт даних
void handleHistoryExport() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "text/plain", "WiFi не підключено");
    return;
  }

  String startDate = server.arg("start");
  String endDate = server.arg("end");
  String format = server.arg("format");  // "csv" або "json"

  if (startDate.length() == 0 || endDate.length() == 0) {
    server.send(400, "text/plain", "Потрібні параметри start та end");
    return;
  }

  String data;
  String contentType;
  String filename;

  if (format == "json") {
    data = exportToJSON(startDate.c_str(), endDate.c_str());
    contentType = "application/json";
    filename = "klimat_data_" + startDate + "_" + endDate + ".json";
  } else {
    data = exportToCSV(startDate.c_str(), endDate.c_str());
    contentType = "text/csv";
    filename = "klimat_data_" + startDate + "_" + endDate + ".csv";
  }

  // Додаємо заголовок для завантаження файлу
  server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
  server.send(200, contentType, data);
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