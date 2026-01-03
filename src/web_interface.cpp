#include "web_interface.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "learning_system.h"
#include <ArduinoJson.h>
#include "advanced_climate_logic.h"
#include "data_storage.h"
#include <vector>
#include <algorithm>

extern int historyIndex;
extern bool historyInitialized;

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ WI-FI ТА ВЕБ-СЕРВЕРА
// ============================================================================

void initWiFi() {
    Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ WI-FI ===");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);  // Повне відключення
    delay(100);
    
    // Список мереж для підключення
    struct WiFiNetwork {
        const char* ssid;
        const char* password;
    };
    
    WiFiNetwork networks[] = {
        {"Redmi Note 14", "12345678"},     // Основна мережа
        {"Redmi Note 9", "1234567890"},    // Резервна мережа
    };
    
    // Спочатку скануємо мережі
    Serial.println("🔍 Пошук доступних мереж...");
    
    int n = WiFi.scanNetworks();
    Serial.printf("Знайдено %d мереж:\n", n);
    
    for (int i = 0; i < n; i++) {
        Serial.printf("  %d: %s (%d dBm) %s\n", 
            i + 1, 
            WiFi.SSID(i).c_str(), 
            WiFi.RSSI(i),
            (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "відкрита" : "захищена");
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

    for (int i = 0; i < sizeof(networks)/sizeof(networks[0]); i++) {
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == networks[i].ssid && networks[i].password != NULL && strlen(networks[i].password) > 0) {
                availableNetworks.push_back({networks[i].ssid, networks[i].password, WiFi.RSSI(j)});
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
        Serial.printf("\n📡 Пробуємо підключитись до мережі: %s (сигнал: %d dBm)\n",
            net.ssid.c_str(), net.rssi);

        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(net.ssid.c_str(), net.password.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;

            if (attempts % 5 == 0) {
                Serial.printf(" [Статус: %d]", WiFi.status());
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            connectedSSID = net.ssid;
            Serial.printf("\n✅ Успішно підключено до: %s\n", net.ssid.c_str());

            // Зберігаємо успішні налаштування
            preferences.begin("wifi", false);
            preferences.putString("ssid", net.ssid);
            preferences.putString("password", net.password);
            preferences.end();

            Serial.printf("Налаштування збережені для мережі: %s\n", net.ssid.c_str());
            break;
        } else {
            Serial.printf("\n❌ Не вдалося підключитись до: %s\n", net.ssid.c_str());
            WiFi.disconnect(true);
            delay(500);
        }
    }
    
    // Якщо підключились - показуємо інформацію
    if (connected) {
        Serial.print("SSID: ");
        Serial.print(connectedSSID);
        Serial.print(" | IP адреса: ");
        Serial.print(WiFi.localIP());
        Serial.print(" | Сигнал: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        
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
    server.on("/history", HTTP_GET, handleHistoryPage);
    server.on("/debug", HTTP_GET, handleDebugPage);
    server.on("/servo", HTTP_GET, handleServoPage);
    server.on("/servo/api", HTTP_POST, handleServoAPI);
    
    server.onNotFound([]() {
        server.send(404, "text/plain", "Сторінка не знайдена");
    });
    
    server.begin();
    Serial.println("✅ Веб-сервер запущено");
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
    
    return "<span style='color:" + color + "; font-weight:bold;'>" + strength + " (" + String(rssi) + " dBm)</span>";
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
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Клімат-контроль</title>";
    html += "<style>";
    html += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');";
    html += "body { font-family: 'Roboto', sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    html += ".header { text-align: center; margin-bottom: 30px; }";
    html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin: 30px 0; }";
    html += ".card { background: #f9f9f9; padding: 20px; border-radius: 8px; border-left: 4px solid #4CAF50; }";
    
    html += ".power-indicators { display: flex; justify-content: space-between; margin: 20px 0; background: #e8f5e9; padding: 15px; border-radius: 8px; }";
    html += ".power-item { text-align: center; flex: 1; padding: 10px; }";
    html += ".power-label { font-size: 0.9em; color: #666; margin-bottom: 5px; }";
    html += ".power-value { font-size: 1.8em; font-weight: bold; color: #2c3e50; }";
    
    html += ".command-section { background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0; }";
    html += "#commandOutput { background: white; padding: 10px; border-radius: 5px; font-family: 'Consolas', monospace; min-height: 50px; white-space: pre-wrap; overflow-y: auto; max-height: 200px; }";
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
    html += "<h1>🌡️ КЛІМАТ-КОНТРОЛЬ СИСТЕМИ ВЕНТИЛЯЦІЇ v4.0</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + " | " + getTimeString() + "</p>";
    html += "<div style='display: inline-block; padding: 10px 20px; background: #4CAF50; color: white; border-radius: 20px; font-weight: bold; margin-top: 10px;'>";
    html += "Режим: <span id='currentMode'>";
    html += heatingState.emergencyMode ? "🚨 АВАРІЯ" : (heatingState.forceMode ? "⚡ ФОРСАЖ" : (heatingState.manualMode ? "✋ РУЧНИЙ" : "🤖 АВТО"));
    html += "</span></div>";
    html += "</div>";
    
    html += "<div class='power-indicators'>";
    html += "<div class='power-item'>";
    html += "<div class='power-label'>💧 НАСОС</div>";
    html += "<div class='power-value' id='pumpPower'>" + String((heatingState.pumpPower * 100) / 255) + "%</div>";
    html += "</div>";
    
    html += "<div class='power-item'>";
    html += "<div class='power-label'>🌪️ ВЕНТИЛЯТОР</div>";
    html += "<div class='power-value' id='fanPower'>" + String((heatingState.fanPower * 100) / 255) + "%</div>";
    html += "</div>";
    
    html += "<div class='power-item'>";
    html += "<div class='power-label'>💨 ВИТЯЖКА</div>";
    html += "<div class='power-value' id='extractorPower'>" + String((heatingState.extractorPower * 100) / 255) + "%</div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='status-grid'>";
    html += "<div class='card'>";
    html += "<h3>🌡️ ТЕМПЕРАТУРА</h3>";
    html += "<div class='status-value temp-status' id='tempRoom'>" + String(sensorData.tempRoom, 1) + "°C</div>";
    html += "<div>Теплоносій: <span id='tempCarrier'>" + String(sensorData.tempCarrier, 1) + "°C</span></div>";
    html += "<div>BME280: <span id='tempBME'>" + String(sensorData.tempBME, 1) + "°C</span></div>";
    html += "<div>Ціль: " + String(config.tempMin, 1) + "-" + String(config.tempMax, 1) + "°C</div>";
    html += "<div>Тренд: <span id='tempTrend'>--</span></div>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>💧 ВОЛОГІСТЬ</h3>";
    html += "<div class='status-value hum-status' id='humidity'>" + String(sensorData.humidity, 1) + "%</div>";
    html += "<div>Ціль: " + String(config.humidityConfig.minHumidity, 1) + "-" + String(config.humidityConfig.maxHumidity, 1) + "%</div>";
    html += "<div>Зволожувач: <span id='humidifierStatus'>" + String(humidifierState.active ? "ВКЛ" : "ВИМК") + "</span></div>";
    html += "<div>Тиск: <span id='pressure'>" + String(sensorData.pressure, 1) + " hPa</span></div>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>⚙️ СИСТЕМА</h3>";
    html += "<div>Режим: <span class='sys-status' id='mode'>";
    html += heatingState.manualMode ? "РУЧНИЙ" : (heatingState.forceMode ? "ФОРСАЖ" : "АВТО");
    html += "</span></div>";
    html += "<div>Wi-Fi: " + WiFi.SSID() + " (" + String(WiFi.RSSI()) + " dBm)</div>";
    html += "<div>Пам'ять: <span id='memory'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
    html += "<div>Час роботи: <span id='uptime'>" + String(millis() / 1000) + " сек</span></div>";
    html += "<div>Записів: <span id='historyCount'>" + String(historyIndex) + "</span></div>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='command-section'>";
    html += "<h3>💬 КОМАНДНАЯ СТРОКА (аналогічно Serial Monitor)</h3>";
    html += "<div style='display: flex; gap: 10px; margin-bottom: 15px;'>";
    html += "<input type='text' id='commandInput' placeholder='Введіть команду (pump 50, fan 40, a50, b40, c30, timer 30, tmin 25, status...)' style='flex: 1; padding: 10px; border: 1px solid #ccc; border-radius: 4px;'>";
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
    html += "<button class='quick-btn' onclick=\"quickCommand('status')\">СТАТУС</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('save')\">ЗБЕРЕГТИ</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('quiet')\" style='background: #ff9800;'>🔇 ВИМК ВИВІД</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('verbose')\" style='background: #4caf50;'>🔊 ВКЛ ВИВІД</button>";
    html += "</div>";
    
    html += "<div id='commandOutput' style='margin-top: 15px;'>> Готово до команд...</div>";
    html += "</div>";
    
    html += "<div class='nav'>";
    html += "<a href='/control' class='nav-btn'>🎛️ ПАНЕЛЬ КЕРУВАННЯ</a>";
    html += "<a href='/settings' class='nav-btn'>⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ</a>";
    html += "<a href='/servo' class='nav-btn'>⚙ КАЛІБРУВАННЯ СЕРВО</a>";
    html += "<a href='/wifi-settings' class='nav-btn'>📶 WI-FI</a>";
    html += "<a href='/learning' class='nav-btn'>🧠 СИСТЕМА НАВЧАННЯ</a>";
    html += "<a href='/status' class='nav-btn'>📊 JSON СТАТУС</a>";
    html += "<a href='/time' class='nav-btn'>🕒 ЧАС</a>";
    html += "<a href='/history' class='nav-btn'>📈 ІСТОРІЯ</a>";
    html += "<a href='/debug' class='nav-btn'>🔧 ВІДЛАДКА</a>";
    html += "</div>";
    
    html += "<div style='text-align: center; color: #777; margin-top: 20px; padding-top: 20px; border-top: 1px solid #eee;'>";
    html += "Оновлено: <span id='lastUpdate'>--:--:--</span>";
    html += " | Наступне оновлення через: <span id='nextUpdate'>3 сек</span>";
    html += "</div>";
    
    html += "</div>";
    
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
    html += "      if (data.tempRoom !== undefined) {";
    html += "        document.getElementById('tempRoom').textContent = data.tempRoom.toFixed(1) + '°C';";
    html += "        document.getElementById('tempCarrier').textContent = data.tempCarrier.toFixed(1) + '°C';";
    html += "      }";
    html += "      if (data.tempBME !== undefined) {";
    html += "        document.getElementById('tempBME').textContent = data.tempBME.toFixed(1) + '°C';";
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
    
    server.send(200, "text/html", html);
}

// ============================================================================
// JSON API ДЛЯ СТАТУСУ
// ============================================================================

void handleStatus() {
    JsonDocument doc;
    
    doc["tempRoom"] = sensorData.tempRoom;
    doc["tempCarrier"] = sensorData.tempCarrier;
    doc["tempBME"] = sensorData.tempBME;
    doc["humidity"] = sensorData.humidity;
    doc["pressure"] = sensorData.pressure;
    doc["pumpPower"] = (heatingState.pumpPower * 100) / 255;
    doc["fanPower"] = (heatingState.fanPower * 100) / 255;
    doc["extractorPower"] = (heatingState.extractorPower * 100) / 255;
    doc["extractorTimer"] = config.extractorTimer.enabled;
    
    if (heatingState.emergencyMode) doc["mode"] = "АВАРІЯ";
    else if (heatingState.forceMode) doc["mode"] = "ФОРСАЖ";
    else if (heatingState.manualMode) doc["mode"] = "РУЧНИЙ";
    else doc["mode"] = "АВТО";
    
    doc["time"] = getTimeString();
    doc["memory"] = ESP.getFreeHeap() / 1024;
    doc["uptime"] = millis() / 1000;
    doc["historyCount"] = historyIndex;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ============================================================================
// ОБРОБКА КОМАНД ІЗ ВЕБ-ІНТЕРФЕЙСУ
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
    
    // Перевірка режимів СПОЧАТКУ (щоб "auto" не розпізнавався як "a0")
    if (lowerCmd == "auto") {
        heatingState.manualMode = false;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        return "✅ Режим: АВТОМАТИЧНИЙ";
    }
    else if (lowerCmd == "manual") {
        heatingState.manualMode = true;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        return "✅ Режим: РУЧНИЙ";
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
    
    // Компактні команди (a50, b40, c30) - автоматично перемикають в РУЧНИЙ режим
    if (lowerCmd.length() >= 2 && lowerCmd.length() <= 4) {
        char device = lowerCmd[0];
        if (device == 'a' || device == 'b' || device == 'c') {
            String valueStr = lowerCmd.substring(1);
            int value = valueStr.toInt();
            
            if (value >= 0 && value <= 100) {
                heatingState.manualMode = true;
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
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        int percent = cmd.substring(5).toInt();
        percent = constrain(percent, 0, 100);
        setPumpPercent(percent);
        return "✅ Насос: " + String(percent) + "% [РУЧНИЙ]";
    }
    else if (lowerCmd.startsWith("fan ")) {
        heatingState.manualMode = true;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        int percent = cmd.substring(4).toInt();
        percent = constrain(percent, 0, 100);
        setFanPercent(percent);
        return "✅ Вентилятор: " + String(percent) + "% [РУЧНИЙ]";
    }
    else if (lowerCmd.startsWith("extractor ")) {
        heatingState.manualMode = true;
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
            heatingState.forceMode = false;
            return "✅ Режим: АВТОМАТИЧНИЙ";
        }
        else if (mode == "manual") {
            heatingState.manualMode = true;
            heatingState.forceMode = false;
            return "✅ Режим: РУЧНИЙ";
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
    else {
        return "❌ Невідома команда: " + cmd + "\n📋 Доступні команди:\npump/fan/extractor XX, timer on/off, tmin/tmax/temp/hmin/hmax/hum XX,\nauto/manual/force, servo, mode, status, save, quiet, verbose, test, web, reboot";
    }
}

// ============================================================================
// СТОРІНКА НАЛАШТУВАНЬ WI-FI (ДОДАЄМО!)
// ============================================================================

void handleWiFiSettingsPage() {
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
    html += "<p><strong>SSID:</strong> " + WiFi.SSID() + "</p>";
    html += "<p><strong>IP адреса:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC адреса:</strong> " + WiFi.macAddress() + "</p>";
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
                int rssi = WiFi.RSSI(i);
                bool encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                
                html += "<div class='network-item'>";
                html += "<div>";
                html += "<div class='network-ssid'>" + ssid + "</div>";
                html += "<div class='network-details'>";
                html += "Сигнал: " + String(rssi) + " dBm | ";
                html += encrypted ? "🔒 Захищена" : "🔓 Відкрита";
                html += "</div>";
                html += "</div>";
                html += "<button class='connect-btn' onclick=\"connectToNetwork('" + ssid + "', '" + String(encrypted ? "true" : "false") + "')\">Підключити</button>";
                html += "</div>";
            }
            
            WiFi.scanDelete();
        } else {
            html += "<div class='network-item'>";
            html += "<div class='network-ssid'>Сканування...</div>";
            html += "</div>";
        }
    } else {
        // Показати збережені мережі з preferences
        preferences.begin("wifi", true);
        String savedSSID = preferences.getString("ssid", "");
        preferences.end();
        
        if (savedSSID.length() > 0) {
            html += "<div class='network-item'>";
            html += "<div>";
            html += "<div class='network-ssid'>" + savedSSID + " (збережена)</div>";
            html += "<div class='network-details'>Збережена мережа - натисніть кнопку вище для сканування</div>";
            html += "</div>";
            html += "<button class='connect-btn' onclick=\"connectToNetwork('" + savedSSID + "', 'true')\">Підключити</button>";
            html += "</div>";
        }
        
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
    html += "<input type='text' id='connectSsid' name='ssid' placeholder='Введіть назву мережі' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='connectPassword'>Пароль:</label>";
    html += "<input type='password' id='connectPassword' name='password' placeholder='Введіть пароль'>";
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
    
    html += "</div>"; // container
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
        
        Serial.println("\n✅ Підключення успішно!");
        Serial.println("  IP: " + WiFi.localIP().toString());
        Serial.println("  RSSI: " + String(WiFi.RSSI()) + " dBm");
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
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Налаштування системи</title>";
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
    html += "<h1>⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ СИСТЕМИ</h1>";
    html += "<p><a href='/' class='back-link'>← На головну</a></p>";
    
    html += "<form method='POST' action='/settings'>";
    
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
    html += "<label>Максимум вентилятора (%):</label>";
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
    
    html += "<div class='section'>";
    html += "<h3>⚙️ СИСТЕМНІ НАЛАШТУВАННЯ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна потужність вентилятора (%):</label>";
    html += "<input type='number' name='fanMin' value='" + String(config.fanMinPercent) + "' min='0' max='30'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Період виведення статусу (секунди):</label>";
    html += "<input type='number' name='statusPeriod' value='" + String(config.statusPeriod / 1000) + "' min='10' max='600'>";
    html += "</div>";
    html += "</div>";
    
    html += "<div style='margin-top: 30px;'>";
    html += "<button type='submit' class='btn'>💾 ЗБЕРЕГТИ НАЛАШТУВАННЯ</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='window.location.href=\"/\"'>СКАСУВАТИ</button>";
    html += "</div>";
    
    html += "</form>";
    
    html += "</div>";
    
    html += "<script>";
    html += "document.querySelector('form').addEventListener('submit', function(e) {";
    html += "  const tempMin = parseFloat(document.querySelector('[name=\"tempMin\"]').value);";
    html += "  const tempMax = parseFloat(document.querySelector('[name=\"tempMax\"]').value);";
    html += "  if (tempMin >= tempMax) {";
    html += "    alert('Помилка: Мінімальна температура має бути менше максимальної!');";
    html += "    e.preventDefault();";
    html += "  }";
    html += "  const humMin = parseFloat(document.querySelector('[name=\"humMin\"]').value);";
    html += "  const humMax = parseFloat(document.querySelector('[name=\"humMax\"]').value);";
    html += "  if (humMin >= humMax) {";
    html += "    alert('Помилка: Мінімальна вологість має бути менше максимальної!');";
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
    if (server.hasArg("statusPeriod")) {
        config.statusPeriod = server.arg("statusPeriod").toInt() * 1000UL;
    }
    
    saveConfiguration();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='2;url=/settings'>";
    html += "<title>Налаштування збережені</title>";
    html += "<style>body { font-family: Arial; text-align: center; padding: 50px; }</style>";
    html += "</head><body>";
    html += "<h1>✅ Налаштування успішно збережені!</h1>";
    html += "<p>Перенаправлення обернено на сторінку налаштувань...</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА КЕРУВАННЯ
// ============================================================================

void handleControlPage() {
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Керування</title>";
    html += "<style>";
    html += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');";
    html += "body { font-family: 'Roboto', sans-serif; margin: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
    html += ".slider { width: 100%; margin: 10px 0; }";
    html += ".btn { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; margin: 5px; cursor: pointer; transition: all 0.3s; }";
    html += ".btn:hover { background: #45a049; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }";
    html += ".slider-value { display: inline-block; width: 50px; text-align: center; font-weight: bold; }";
    html += ".control-section { margin-bottom: 30px; padding: 20px; background: #f9f9f9; border-radius: 8px; }";
    html += ".mode-buttons { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; }";
    html += ".mode-btn { padding: 15px 20px; border: 2px solid #ddd; background: #f5f5f5; color: #333; border-radius: 8px; cursor: pointer; font-size: 16px; font-weight: 500; transition: all 0.3s; position: relative; overflow: hidden; }";
    html += ".mode-btn:hover { transform: translateY(-3px); box-shadow: 0 6px 12px rgba(0,0,0,0.15); }";
    html += ".mode-btn.active { background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%); color: white; border-color: #4CAF50; box-shadow: 0 4px 15px rgba(76, 175, 80, 0.4); animation: pulse 2s infinite; }";
    html += "@keyframes pulse { 0%, 100% { box-shadow: 0 4px 15px rgba(76, 175, 80, 0.4); } 50% { box-shadow: 0 6px 20px rgba(76, 175, 80, 0.6); } }";
    html += ".mode-btn.active::before { content: '✓ '; font-weight: bold; margin-right: 5px; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class='container'>";
    html += "<h1>🎛️ ПАНЕЛЬ КЕРУВАННЯ СИСТЕМОЮ</h1>";
    html += "<p><a href='/'>← На головну</a></p>";
    
    html += "<div class='control-section'>";
    html += "<h3>НАСОС (A)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String((heatingState.pumpPower * 100) / 255) + "' class='slider' id='pumpSlider' oninput='updatePump(this.value)'>";
    html += "<span class='slider-value' id='pumpValue'>" + String((heatingState.pumpPower * 100) / 255) + "%</span>";
    html += "<button class='btn' onclick=\"sendCmd('pump 0')\">ВИМК</button>";
    html += "<button class='btn' onclick=\"sendCmd('pump 30')\">30%</button>";
    html += "<button class='btn' onclick=\"sendCmd('pump 50')\">50%</button>";
    html += "<button class='btn' onclick=\"sendCmd('pump 80')\">80%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>ВЕНТИЛЯТОР (B)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String((heatingState.fanPower * 100) / 255) + "' class='slider' id='fanSlider' oninput='updateFan(this.value)'>";
    html += "<span class='slider-value' id='fanValue'>" + String((heatingState.fanPower * 100) / 255) + "%</span>";
    html += "<button class='btn' onclick=\"sendCmd('fan 0')\">ВИМК</button>";
    html += "<button class='btn' onclick=\"sendCmd('fan 30')\">30%</button>";
    html += "<button class='btn' onclick=\"sendCmd('fan 50')\">50%</button>";
    html += "<button class='btn' onclick=\"sendCmd('fan 80')\">80%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>ВИТЯЖКА (C)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String((heatingState.extractorPower * 100) / 255) + "' class='slider' id='extractorSlider' oninput='updateExtractor(this.value)'>";
    html += "<span class='slider-value' id='extractorValue'>" + String((heatingState.extractorPower * 100) / 255) + "%</span>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 0')\">ВИМК</button>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 30')\">30%</button>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 50')\">50%</button>";
    html += "<button class='btn' onclick=\"sendCmd('extractor 80')\">80%</button>";
    html += "</div>";
    
    html += "<div class='control-section'>";
    html += "<h3>РЕЖИМИ РОБОТИ</h3>";
    html += "<div class='mode-buttons'>";
    html += "<button class='mode-btn" + String(!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode ? " active" : "") + "' onclick=\"sendCmd('auto')\" data-mode='auto'>🤖 АВТО</button>";
    html += "<button class='mode-btn" + String(heatingState.manualMode ? " active" : "") + "' onclick=\"sendCmd('manual')\" data-mode='manual'>✋ РУЧНИЙ</button>";
    html += "</div>";
    html += "<div style='margin-top: 15px; padding: 12px; background: #fff3cd; border-left: 4px solid #ffc107; border-radius: 5px; font-size: 14px;'>";
    html += "<strong>ℹ️ Автоматичні режими:</strong><br>";
    html += "⚡ <strong>ФОРСАЖ</strong> - вмикається при температурі < 20°C (80% потужність)<br>";
    html += "🚨 <strong>АВАРІЯ</strong> - вмикається при температурі < 18°C (100% потужність)";
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
    
    html += "<script>";
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
    html += "    }";
    html += "  });";
    html += "}";
    html += "</script>";
    
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
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleWiFiPage() {
    String html = "";
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 600px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>📶 WI-FI ІНФОРМАЦІЯ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>SSID:</strong> " + WiFi.SSID() + "</p>";
    html += "<p><strong>IP адреса:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC адреса:</strong> " + WiFi.macAddress() + "</p>";
    html += "<p><strong>Сигнал (RSSI):</strong> " + String(WiFi.RSSI()) + " dBm</p>";
    html += "<p><strong>Статус:</strong> " + String(WiFi.status() == WL_CONNECTED ? "Підключено" : "Відключено") + "</p>";
    html += "</div>";
    html += "<p><a href='/'>← На головну</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleHistoryPage() {
    String html = "";
    html = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>";
    html += "<div style='max-width: 800px; margin: 0 auto; padding: 20px;'>";
    html += "<h1>📈 ІСТОРІЯ ДАНИХ</h1>";
    html += "<div style='background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0;'>";
    html += "<p><strong>Всього записів:</strong> " + String(historyIndex) + "</p>";
    html += "<p><strong>Розмір буфера:</strong> " + String(HISTORY_BUFFER_SIZE) + " записів</p>";
    html += "<p><strong>Ініціалізовано:</strong> " + String(historyInitialized ? "Так" : "Ні") + "</p>";
    html += "<p><em>Повний перегляд історії буде доступний в майбутніх версіях</em></p>";
    html += "</div>";
    html += "<p><a href='/'>← На головну</a></p>";
    html += "</div>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleDebugPage() {
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
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА СИСТЕМИ НАВЧАННЯ
// ============================================================================

void handleLearningPage() {
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
    html += ".angle-display { font-size: 48px; font-weight: bold; text-align: center; color: #2196F3; margin: 20px 0; }";
    html += ".nav { text-align: center; margin-top: 20px; }";
    html += ".nav a { color: #2196F3; text-decoration: none; margin: 0 10px; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>⚙ Калібрування серво</h1>";
    
    html += "<div class='status'>";
    html += "<div class='status-item'><span>Поточний кут:</span><span id='current'>" + String(ventState.currentAngle) + "°</span></div>";
    html += "<div class='status-item'><span>Закрито:</span><span id='closed'>" + String(config.servoClosedAngle) + "°</span></div>";
    html += "<div class='status-item'><span>Відкрито:</span><span id='open'>" + String(config.servoOpenAngle) + "°</span></div>";
    html += "<div class='status-item'><span>Вимикач:</span><span id='switch' style='font-weight:bold;color:";
    html += ventState.switchState ? "#4CAF50'>УВІМКНЕНО" : "#f44336'>ВИМКНЕНО";
    html += "</span></div>";
    html += "<div class='status-item'><span>Режим:</span><span id='mode' style='font-weight:bold;color:";
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
// ЗАВДАННЯ ВЕБ-СЕРВЕРА
// ============================================================================

void webTask(void *parameter) {
    Serial.println("✅ Веб-завдання запущено");
    
    while (1) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}