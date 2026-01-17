// ============================================================================
// WEB_PAGES_WIFI.CPP - WiFi налаштування
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Функції: handleWiFiSettingsPage(), handleScanWiFi(), handleSaveWiFiSettings(),
//          handleWiFiPage(), handleSaveNetworkSettings()
// ============================================================================

#include "web_interface.h"
#include "web_common.h"
#include "system_core.h"
#include "global_declarations.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Forward declarations
extern bool checkCSRF();
extern String getUkraineMarquee();
extern Preferences preferences;

// ============================================================================
// СТОРІНКА НАЛАШТУВАНЬ WI-FI
// ============================================================================

void handleWiFiSettingsPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("Налаштування Wi-Fi");

    // Додаткові стилі
    html += "<style>";
    html += ".section { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 25px; border-left: 4px solid #3498db; }";
    html += ".current-info { background: #e8f5e9; padding: 15px; border-radius: 5px; margin: 15px 0; }";
    html += ".network-list { max-height: 300px; overflow-y: auto; border: 1px solid #ddd; border-radius: 5px; padding: 10px; background: white; }";
    html += ".network-item { padding: 10px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px; }";
    html += ".network-item:hover { background: #f0f0f0; }";
    html += ".network-ssid { font-weight: 600; }";
    html += ".network-details { font-size: 0.9em; color: #666; }";
    html += ".connect-btn { background: #4CAF50; color: white; padding: 8px 15px; border: none; border-radius: 5px; cursor: pointer; font-size: 0.9em; }";
    html += ".connect-btn:hover { background: #45a049; }";
    html += ".btn-scan { background: #9b59b6; }";
    html += ".btn-scan:hover { background: #8e44ad; }";
    html += ".btn-save { background: #2ecc71; }";
    html += ".btn-save:hover { background: #27ae60; }";
    html += ".status-message { padding: 10px; border-radius: 5px; margin: 10px 0; }";
    html += ".status-success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
    html += ".status-error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
    html += ".hidden { display: none; }";
    html += "</style>";

    html += "<script>";
    html += "var savedNetworks = {};";

    // Завантажуємо список збережених мереж (без паролів!)
    preferences.begin("wifi", true);
    int networkCount = preferences.getInt("netCount", 0);
    for (int i = 0; i < networkCount; i++) {
        String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
        if (savedSSID.length() > 0) {
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

    html += "<div class='container'>";
    html += getNavHeader("📶 Налаштування Wi-Fi");

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
    html += "<button id='scanBtn' class='btn btn-scan' onclick='startScan()'>📡 Сканувати мережі</button>";
    html += "<div class='network-list'>";

    // Показати результати сканування якщо є параметр scanned
    if (server.hasArg("scanned")) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
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
                html += "<button class='connect-btn' onclick=\"connectToNetwork('" + ssidEscaped + "', '" + String(encrypted ? "true" : "false") + "')\">Підключити</button>";
                html += "</div>";
            }
            WiFi.scanDelete();
        }
    } else {
        // Показати збережені мережі
        preferences.begin("wifi", true);
        int networkCount = preferences.getInt("netCount", 0);

        if (networkCount > 0) {
            html += "<div class='status-message' style='background:#d1ecf1;color:#0c5460;'>💾 Збережено мереж: " + String(networkCount) + "</div>";

            for (int i = 0; i < networkCount; i++) {
                String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
                if (savedSSID.length() > 0) {
                    String savedSSIDEscaped = htmlEscape(savedSSID);
                    html += "<div class='network-item'>";
                    html += "<div class='network-ssid'>📱 " + savedSSIDEscaped + "</div>";
                    html += "<button class='connect-btn' onclick=\"connectToNetwork('" + savedSSIDEscaped + "', 'true')\">Підключити</button>";
                    html += "</div>";
                }
            }
        }
        preferences.end();

        html += "<div class='network-item'><div class='network-ssid'>📡 Натисніть кнопку сканування</div></div>";
    }

    html += "</div></div>"; // network-list, section

    // Форма підключення
    html += "<div class='section'>";
    html += "<h2>Підключення до мережі</h2>";

    if (server.hasArg("error")) {
        html += "<div class='status-message status-error'>❌ " + server.arg("error") + "</div>";
    }
    if (server.hasArg("success")) {
        html += "<div class='status-message status-success'>✅ " + server.arg("success") + "</div>";
    }

    html += "<form id='wifiForm' method='POST' action='/save-wifi'>";
    html += "<div id='manualForm' class='hidden'>";
    html += "<div class='form-group' style='margin-bottom:15px;'>";
    html += "<label for='connectSsid'>Назва мережі (SSID):</label>";
    html += "<input type='text' id='connectSsid' name='ssid' placeholder='Введіть назву мережі' required>";
    html += "</div>";

    html += "<div class='form-group' style='margin-bottom:15px;'>";
    html += "<label for='connectPassword'>Пароль:</label>";
    html += "<input type='password' id='connectPassword' name='password' placeholder='Введіть пароль'>";
    html += "<small style='color:#666;'>Залиште пустим для відкритих мереж</small>";
    html += "</div>";

    html += "<input type='hidden' id='connectEncrypted' name='encrypted' value='true'>";

    html += "<div class='form-group' style='margin-bottom:15px;'>";
    html += "<label><input type='checkbox' name='save' checked> Зберегти налаштування</label>";
    html += "</div>";

    html += "<button type='submit' class='btn btn-save'>🔗 Підключитися</button>";
    html += "<button type='button' class='btn' onclick=\"document.getElementById('manualForm').classList.add('hidden')\">Скасувати</button>";
    html += "</div></form>";

    html += "<button class='btn' onclick=\"showManualForm()\" id='showFormBtn'>📝 Ввести дані вручну</button>";
    html += "<a href='/wifi' class='btn' style='display: inline-block; text-decoration: none; margin-left: 10px;'>⚙️ Розширені налаштування</a>";

    html += "</div>"; // section

    html += getNavFooter();
    html += "</div>"; // container

    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

// ============================================================================
// СКАНУВАННЯ WI-FI
// ============================================================================

void handleScanWiFi() {
    if (!checkAuth()) return;

    Serial.println("📶 Сканування Wi-Fi мереж...");
    int n = WiFi.scanNetworks(false, false);
    Serial.printf("Знайдено %d мереж\n", n);

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='0;url=/wifi-settings?scanned=true'>";
    html += "<title>Сканування завершено</title>";
    html += "</head><body><p>Сканування завершено. Перенаправлення...</p></body></html>";

    server.send(200, "text/html", html);
}

// ============================================================================
// ЗБЕРЕЖЕННЯ НАЛАШТУВАНЬ WI-FI
// ============================================================================

void handleSaveWiFiSettings() {
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
        return;
    }

    String ssid = server.arg("ssid");
    String password = server.arg("password");
    bool saveSettings = server.hasArg("save");

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
    }

    // Пробуємо підключитись
    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    bool connected = false;
    while (attempts < 30 && !connected) {
        delay(500);
        attempts++;
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
    }

    String response = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    response += "<meta http-equiv='refresh' content='5;url=/'>";
    response += "<title>Підключення Wi-Fi</title>";
    response += "<style>body{font-family:Arial;text-align:center;padding:50px;}.success{color:#2ecc71;}.error{color:#e74c3c;}</style>";
    response += "</head><body>";

    if (connected) {
        response += "<h1 class='success'>✅ Успішно підключено!</h1>";
        response += "<p>Мережа: " + ssid + "</p>";
        response += "<p>IP адреса: " + WiFi.localIP().toString() + "</p>";
        response += "<p>Перенаправлення...</p>";

        // Зберігаємо в список
        preferences.begin("wifi", false);
        int networkCount = preferences.getInt("netCount", 0);

        bool exists = false;
        for (int i = 0; i < networkCount; i++) {
            String savedSSID = preferences.getString(("ssid" + String(i)).c_str(), "");
            if (savedSSID == ssid) {
                preferences.putString(("pass" + String(i)).c_str(), password);
                exists = true;
                break;
            }
        }

        if (!exists && networkCount < 5) {
            preferences.putString(("ssid" + String(networkCount)).c_str(), ssid);
            preferences.putString(("pass" + String(networkCount)).c_str(), password);
            networkCount++;
            preferences.putInt("netCount", networkCount);
        }

        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
    } else {
        response += "<h1 class='error'>❌ Не вдалося підключитись</h1>";
        response += "<p>Перевірте пароль.</p>";

        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ClimateControl", "12345678");
    }

    response += "</body></html>";
    server.send(200, "text/html", response);
}

// ============================================================================
// СТОРІНКА РОЗШИРЕНИХ НАЛАШТУВАНЬ WIFI
// ============================================================================

void handleWiFiPage() {
    if (!checkAuth()) return;

    String html = getHtmlHead("WiFi налаштування");

    html += "<style>";
    html += ".info-box { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 15px 0; }";
    html += ".info-item { display: flex; justify-content: space-between; padding: 5px 0; flex-wrap: wrap; }";
    html += ".form-group { margin: 15px 0; }";
    html += ".form-group label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += ".help-icon { display: inline-block; width: 22px; height: 22px; line-height: 22px; background: #9c27b0; color: #fff; border-radius: 50%; text-align: center; font-size: 14px; font-weight: bold; cursor: pointer; margin-left: 8px; }";
    html += ".help-icon:hover { background: #7b1fa2; }";
    html += "</style>";

    html += "<div class='container'>";
    html += getNavHeader("📶 WiFi налаштування");

    // Попередження
    html += "<div style='background: #fff3cd; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #ffc107;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Ваш пристрій і ESP32 мають бути в <strong>ОДНІЙ WiFi мережі!</strong>";
    html += "</div>";

    // Поточний стан
    html += "<div class='info-box'>";
    html += "<div class='info-item'><span><strong>SSID:</strong></span><span>" + htmlEscape(WiFi.SSID()) + "</span></div>";
    html += "<div class='info-item'><span><strong>IP:</strong></span><span>" + htmlEscape(WiFi.localIP().toString()) + "</span></div>";
    html += "<div class='info-item'><span><strong>mDNS:</strong></span><span>http://klimat.local</span></div>";
    html += "<div class='info-item'><span><strong>MAC:</strong></span><span>" + htmlEscape(WiFi.macAddress()) + "</span></div>";
    html += "<div class='info-item'><span><strong>Сигнал:</strong></span><span>" + String(WiFi.RSSI()) + " dBm</span></div>";
    html += "</div>";

    // Форма налаштувань
    html += "<form action='/saveNetwork' method='POST'>";

    html += "<h2>⚙ Статична IP</h2>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='useStaticIP' value='1' " + String(config.useStaticIP ? "checked" : "") + "> Використовувати статичну IP</label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>IP адреса:</label>";
    html += "<input type='text' name='staticIP' value='" + config.staticIP + "' placeholder='192.168.1.100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Шлюз:</label>";
    html += "<input type='text' name='gateway' value='" + config.gateway + "' placeholder='192.168.1.1'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Маска:</label>";
    html += "<input type='text' name='subnet' value='" + config.subnet + "' placeholder='255.255.255.0'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>DNS:</label>";
    html += "<input type='text' name='dns' value='" + config.dns + "' placeholder='8.8.8.8'>";
    html += "</div>";

    html += "<h2>🔐 Безпека</h2>";
    html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-bottom: 15px; font-size: 0.9em;'>";
    html += "⚠️ Увімкніть автентифікацію для доступу через інтернет!";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='useAuth' value='1' " + String(config.useAuth ? "checked" : "") + "> Увімкнути автентифікацію</label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Логін:</label>";
    html += "<input type='text' name='authLogin' value='" + config.authLogin + "' placeholder='admin'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Пароль:</label>";
    html += "<input type='password' name='authPassword' value='" + config.authPassword + "' placeholder='Введіть пароль'>";
    html += "</div>";

    html += "<div style='text-align: center;'>";
    html += "<button type='submit' class='btn btn-success'>Зберегти налаштування</button>";
    html += "</div>";
    html += "</form>";

    html += getNavFooter();
    html += "</div>";

    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

// ============================================================================
// ЗБЕРЕЖЕННЯ МЕРЕЖЕВИХ НАЛАШТУВАНЬ
// ============================================================================

void handleSaveNetworkSettings() {
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
        return;
    }

    bool needRestart = false;

    if (server.hasArg("useStaticIP")) {
        config.useStaticIP = true;
        needRestart = true;
    } else {
        if (config.useStaticIP) needRestart = true;
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

    config.useAuth = server.hasArg("useAuth");
    if (server.hasArg("authLogin")) config.authLogin = server.arg("authLogin");
    if (server.hasArg("authPassword") && server.arg("authPassword").length() > 0) {
        config.authPassword = server.arg("authPassword");
    }

    // Зберігаємо
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

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='3;url=/wifi'>";
    html += "</head><body>";
    html += "<div style='max-width: 600px; margin: 50px auto; padding: 20px; background: white; border-radius: 10px; text-align: center;'>";
    html += "<h1>✅ Налаштування збережено!</h1>";
    if (needRestart) {
        html += "<p>⚠️ Для застосування змін потрібно перезавантажити пристрій.</p>";
    }
    html += "<p>Перенаправлення...</p>";
    html += "</div></body></html>";

    server.send(200, "text/html", html);
}
