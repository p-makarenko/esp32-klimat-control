// ============================================================================
// WEB_PAGES_MAIN.CPP - Головна сторінка та статус
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Функції: handleRoot(), handleStatus()
// ============================================================================

#include "web_interface.h"
#include "web_common.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "global_declarations.h"
#include "data_logger.h"
#include "co2_sensor.h"
#include "plant_fan.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>

// Forward declarations
extern String getTimeString();
extern String getUkraineMarquee();

// ============================================================================
// ГОЛОВНА СТОРІНКА
// ============================================================================

void handleRoot() {
    if (!checkAuth()) return;

    // Перевірка WiFi перед відправкою великої відповіді
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    String html = getHtmlHead("Клімат-контроль");

    // Додаткові стилі специфічні для головної сторінки
    html += "<style>";
    html += ".mode-badge { display: inline-block; padding: 10px 20px; background: #4CAF50; color: white; border-radius: 20px; font-weight: 600; margin-top: 10px; }";
    html += ".emergency-alert { background: #ffebee; border: 2px solid #f44336; padding: 15px; border-radius: 8px; margin-top: 15px; }";
    html += ".emergency-alert h3 { color: #d32f2f; margin: 0 0 10px 0; }";
    html += ".info-banner { background: #e8f5e9; padding: 10px 15px; border-radius: 5px; margin: 10px 0; font-size: 0.85em; }";
    html += ".android-tip { background: #fff3cd; padding: 12px 15px; border-radius: 5px; margin: 10px 0; border-left: 4px solid #ff9800; display: flex; align-items: center; gap: 15px; flex-wrap: wrap; }";
    html += ".update-info { text-align: center; color: #777; margin-top: 20px; padding-top: 20px; border-top: 1px solid #eee; }";
    html += "</style>";

    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>🌡️ КЛІМАТ-КОНТРОЛЬ <span onclick='showVersion()' style='cursor: pointer; color: #2196F3;'>" VERSION "</span></h1>";

    // JavaScript для показу версії
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
    html += "<div class='info-banner'>";
    html += "📡 <strong>Підключено до:</strong> " + htmlEscape(WiFi.SSID()) + " | ";
    html += "<strong>IP:</strong> " + htmlEscape(WiFi.localIP().toString()) + " | ";
    html += "<strong>⏰</strong> <span id='currentTime'>" + htmlEscape(getTimeString()) + "</span>";
    html += "</div>";

    // Підказка для Android з QR кодом
    html += "<div class='android-tip'>";
    html += "<div style='flex: 1;'>";
    html += "📱 <strong>Для Android:</strong> Використовуйте IP адресу:<br>";
    html += "<a href='http://" + WiFi.localIP().toString() + "' style='color: #d84315; font-weight: 600; text-decoration: underline; font-size: 1.1em;'>";
    html += "http://" + WiFi.localIP().toString();
    html += "</a>";
    html += "<br><small style='color: #856404;'>Android не підтримує klimat.local - збережіть IP в закладки!</small>";
    html += "</div>";
    // QR код
    String qrUrl = "http://" + WiFi.localIP().toString();
    html += "<div style='text-align: center;'>";
    html += "<img src='https://api.qrserver.com/v1/create-qr-code/?size=100x100&data=" + qrUrl + "' alt='QR код' style='border: 2px solid #ff9800; border-radius: 5px;'>";
    html += "<br><small style='color: #856404;'>Скануй для підключення</small>";
    html += "</div>";
    html += "</div>";

    // Режим роботи
    html += "<div class='mode-badge'>";
    html += "Режим: <span id='currentMode'>";
    html += heatingState.emergencyMode ? "🚨 АВАРІЯ" : (heatingState.forceMode ? "⚡ ФОРСАЖ" : (heatingState.manualMode ? "✋ РУЧНИЙ" : "🤖 АВТО"));
    html += "</span></div>";

    // Контейнер для аварійного повідомлення
    html += "<div id='emergencyAlert'>";
    if (powerOutageState.detected || powerOutageState.emergencyHeatingActive) {
        html += "<div class='emergency-alert'>";
        html += "<h3>🚨 ВІДСУТНІСТЬ 220В!</h3>";
        html += "<p style='margin: 5px 0;'>Напруга: " + String(powerOutageState.lastVoltage, 1) + " В</p>";
        html += "<button class='btn btn-danger' style='margin-top: 10px; padding: 12px 20px; font-weight: bold;' onclick='resetEmergency()'>🔄 СКИНУТИ АВАРІЙНИЙ РЕЖИМ</button>";
        html += "</div>";
    }
    html += "</div>";

    html += "</div>"; // .header

    // Power indicators
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

    // Status grid
    html += "<div class='status-grid'>";

    // Температура
    html += "<div class='card'>";
    html += "<h3>🌡️ ТЕМПЕРАТУРА</h3>";
    html += "<div class='status-value temp-status' id='tempRoom'>";
    html += sensorData.roomValid ? String(sensorData.tempRoom, 1) + "°C" : "🚨 ПОМИЛКА";
    html += "</div>";
    html += "<div>Теплоносій: <span id='tempCarrier'>" + (sensorData.carrierValid ? String(sensorData.tempCarrier, 1) + "°C" : "🚨 ПОМИЛКА") + "</span></div>";
    html += "<div>BME280: <span id='tempBME'>" + (sensorData.bmeValid ? String(sensorData.tempBME, 1) + "°C" : "🚨 ПОМИЛКА") + "</span></div>";
    html += "<div>Ціль: " + String(config.tempMin, 1) + "-" + String(config.tempMax, 1) + "°C</div>";
    html += "</div>";

    // Вологість
    html += "<div class='card'>";
    html += "<h3>💧 ВОЛОГІСТЬ</h3>";
    html += "<div class='status-value hum-status' id='humidity'>" + (sensorData.bmeValid ? String(sensorData.humidity, 1) + "%" : "🚨 ПОМИЛКА") + "</div>";
    html += "<div>Ціль: " + String(config.humidityConfig.minHumidity, 1) + "-" + String(config.humidityConfig.maxHumidity, 1) + "%</div>";
    html += "<div>Зволожувач: <span id='humidifierStatus'>" + String(humidifierState.active ? "ВКЛ" : "ВИМК") + "</span></div>";
    html += "<div>Тиск: <span id='pressure'>" + (sensorData.bmeValid ? String(sensorData.pressure, 1) + " hPa" : "🚨 ПОМИЛКА") + "</span></div>";
    html += "</div>";

    // CO2 (SCD30)
    html += "<div class='card'>";
    html += "<h3>💨 CO2 (SCD30)</h3>";
    html += "<div class='status-value' id='co2Level' style='color: " + String(sensorData.co2Valid ? "#4CAF50" : "#f44336") + ";'>";
    html += sensorData.co2Valid ? String(sensorData.co2Level, 0) + " ppm" : "🚨 ПОМИЛКА";
    html += "</div>";
    html += "<div>Статус: <span id='co2Status'>" + String(sensorData.co2Valid ? "✓ OK" : "✗ НЕ ВІДПОВІДАЄ") + "</span></div>";
    html += "<div>T: <span id='co2Temp'>" + (sensorData.co2Valid ? String(getSCD30Temperature(), 1) + "°C" : "N/A") + "</span> | ";
    html += "H: <span id='co2Hum'>" + (sensorData.co2Valid ? String(getSCD30Humidity(), 1) + "%" : "N/A") + "</span></div>";
    html += "</div>";

    // Система
    html += "<div class='card'>";
    html += "<h3>⚙️ СИСТЕМА</h3>";
    html += "<div>Режим: <span class='sys-status' id='mode'>" + String(heatingState.manualMode ? "РУЧНИЙ" : (heatingState.forceMode ? "ФОРСАЖ" : "АВТО")) + "</span></div>";
    html += "<div>Wi-Fi: " + htmlEscape(WiFi.SSID()) + " (" + String(WiFi.RSSI()) + " dBm)</div>";
    html += "<div>Пам'ять: <span id='memory'>" + String(ESP.getFreeHeap() / 1024) + " KB</span></div>";
    html += "<div>Час роботи: <span id='uptime'>" + String(millis() / 1000) + " сек</span></div>";
    LoggerStats mainStats = getLoggerStats();
    html += "<div>Записів: <span id='historyCount'>" + String(mainStats.totalRecordsRAM) + "</span></div>";
    html += "</div>";

    html += "</div>"; // .status-grid

    // Командна строка
    html += "<div class='command-section'>";
    html += "<h3>💬 КОМАНДНА СТРОКА (аналогічно Serial Monitor)</h3>";
    html += "<div style='display: flex; gap: 10px; margin-bottom: 15px; flex-wrap: wrap;'>";
    html += "<input type='text' id='commandInput' list='commandList' placeholder='Почніть вводити або виберіть команду...' style='flex: 1; min-width: 200px;'>";
    html += "<datalist id='commandList'>";
    html += "<option value='status'>status - статус системи</option>";
    html += "<option value='menu'>menu - показати всі команди</option>";
    html += "<option value='pump '>pump XX - встановити насос 0-100%</option>";
    html += "<option value='fan '>fan XX - встановити вентилятор 0-100%</option>";
    html += "<option value='extractor '>extractor XX - встановити витяжку 0-100%</option>";
    html += "<option value='auto'>auto - автоматичний режим</option>";
    html += "<option value='manual'>manual - ручний режим</option>";
    html += "<option value='force'>force - форсований режим</option>";
    html += "<option value='save'>save - зберегти налаштування</option>";
    html += "<option value='sheets-sync'>sheets-sync - синхронізація з Google Sheets</option>";
    html += "<option value='sheets-stats'>sheets-stats - статистика синхронізації</option>";
    html += "</datalist>";
    html += "<button class='btn' onclick='executeCommand()'>ВИКОНАТИ</button>";
    html += "<button class='btn btn-danger' onclick='clearOutput()'>ОЧИСТИТИ</button>";
    html += "</div>";

    // Quick buttons
    html += "<div class='quick-buttons'>";
    html += "<button class='quick-btn' onclick=\"quickCommand('a30')\">a30 (Насос)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('b40')\">b40 (Вент.)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('c50')\">c50 (Вит.)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('c0')\">c0 (Вит.ВИМК)</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('pump 30')\">Насос 30%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('fan 40')\">Вент 40%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('extractor 50')\">Вит 50%</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('extractor 0')\">Вит ВИМК</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('auto')\">АВТО</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('manual')\">РУЧНИЙ</button>";
    html += "</div>";

    // Manual lock checkbox
    html += "<div style='margin: 10px 0; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<label style='display: flex; align-items: center; gap: 8px; cursor: pointer;'>";
    html += "<input type='checkbox' id='manualLock' " + String(heatingState.manualModeLocked ? "checked" : "") + " onchange='toggleManualLock()' style='width: 18px; height: 18px;'>";
    html += "<span>🔒 Блокувати ручний режим (без автоповернення)</span>";
    html += "</label>";
    html += "</div>";

    // Другий ряд кнопок
    html += "<div class='quick-buttons'>";
    html += "<button class='quick-btn' onclick=\"quickCommand('status')\">СТАТУС</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('save')\">ЗБЕРЕГТИ</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('sheets-sync')\" style='background: #4285f4; color: white;'>📤 SYNC</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('sheets-stats')\" style='background: #34a853; color: white;'>📊 STATS</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('quiet')\" style='background: #ff9800; color: white;'>🔇 ВИМК</button>";
    html += "<button class='quick-btn' onclick=\"quickCommand('verbose')\" style='background: #4caf50; color: white;'>🔊 ВКЛ</button>";
    html += "</div>";

    html += "<div id='commandOutput' style='margin-top: 15px;'>> Готово до команд...</div>";
    html += "</div>";

    // Навігація
    html += getMainNavigation();

    // Update info
    html += "<div class='update-info'>";
    html += "Оновлено: <span id='lastUpdate'>--:--:--</span>";
    html += " | Наступне оновлення через: <span id='nextUpdate'>3 сек</span>";
    html += "</div>";

    html += "</div>"; // .container

    // Ukraine marquee
    html += getUkraineMarquee();

    // JavaScript
    html += "<script>";
    html += "let updateInterval = 3000;";
    html += "let lastUpdateTime = new Date();";

    // Update mode buttons
    html += "function updateModeButtons(activeMode) {";
    html += "  const btns = document.querySelectorAll('.mode-btn');";
    html += "  if (btns.length === 0) return;";
    html += "  btns.forEach(btn => {";
    html += "    btn.classList.remove('active');";
    html += "    if (btn.getAttribute('data-mode') === activeMode) btn.classList.add('active');";
    html += "  });";
    html += "}";

    // Update status via AJAX
    html += "function updateStatus() {";
    html += "  fetch('/status')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (data.tempRoom !== undefined && !isNaN(data.tempRoom)) document.getElementById('tempRoom').textContent = data.tempRoom.toFixed(1) + '°C';";
    html += "      else document.getElementById('tempRoom').textContent = '🚨 ПОМИЛКА';";
    html += "      if (data.tempCarrier !== undefined && !isNaN(data.tempCarrier)) document.getElementById('tempCarrier').textContent = data.tempCarrier.toFixed(1) + '°C';";
    html += "      else document.getElementById('tempCarrier').textContent = '🚨 ПОМИЛКА';";
    html += "      if (data.tempBME !== undefined && !isNaN(data.tempBME)) document.getElementById('tempBME').textContent = data.tempBME.toFixed(1) + '°C';";
    html += "      else document.getElementById('tempBME').textContent = '🚨 ПОМИЛКА';";
    html += "      if (data.humidity !== undefined && !isNaN(data.humidity)) document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';";
    html += "      else document.getElementById('humidity').textContent = '🚨 ПОМИЛКА';";
    html += "      if (data.pressure !== undefined && !isNaN(data.pressure)) document.getElementById('pressure').textContent = data.pressure.toFixed(1) + ' hPa';";
    html += "      if (data.co2Level !== undefined && !isNaN(data.co2Level)) {";
    html += "        document.getElementById('co2Level').textContent = data.co2Level.toFixed(0) + ' ppm';";
    html += "        document.getElementById('co2Status').textContent = '✓ OK';";
    html += "      } else {";
    html += "        document.getElementById('co2Level').textContent = '🚨 ПОМИЛКА';";
    html += "        document.getElementById('co2Status').textContent = '✗ НЕ ВІДПОВІДАЄ';";
    html += "      }";
    html += "      if (data.co2Temp !== undefined && !isNaN(data.co2Temp)) document.getElementById('co2Temp').textContent = data.co2Temp.toFixed(1) + '°C';";
    html += "      if (data.co2Humidity !== undefined && !isNaN(data.co2Humidity)) document.getElementById('co2Hum').textContent = data.co2Humidity.toFixed(1) + '%';";
    html += "      if (data.pumpPower !== undefined) document.getElementById('pumpPower').textContent = Math.round(data.pumpPower) + '%';";
    html += "      if (data.fanPower !== undefined) document.getElementById('fanPower').textContent = Math.round(data.fanPower) + '%';";
    html += "      if (data.extractorPower !== undefined) document.getElementById('extractorPower').textContent = Math.round(data.extractorPower) + '%';";
    html += "      if (data.mode) {";
    html += "        document.getElementById('mode').textContent = data.mode;";
    html += "        let modeIcon = '🤖';";
    html += "        if (data.mode === 'АВАРІЯ') modeIcon = '🚨';";
    html += "        else if (data.mode === 'ФОРСАЖ') modeIcon = '⚡';";
    html += "        else if (data.mode === 'РУЧНИЙ') modeIcon = '✋';";
    html += "        document.getElementById('currentMode').textContent = modeIcon + ' ' + data.mode;";
    html += "      }";
    html += "      if (data.memory) document.getElementById('memory').textContent = data.memory + ' KB';";
    html += "      if (data.time) document.getElementById('currentTime').textContent = data.time;";
    html += "      if (data.historyCount !== undefined) document.getElementById('historyCount').textContent = data.historyCount;";
    html += "      if (data.manualModeLocked !== undefined) {";
    html += "        const lockCheckbox = document.getElementById('manualLock');";
    html += "        if (lockCheckbox) lockCheckbox.checked = data.manualModeLocked;";
    html += "      }";
    // Emergency alert handling
    html += "      const emergencyAlert = document.getElementById('emergencyAlert');";
    html += "      if (emergencyAlert) {";
    html += "        if (data.powerOutageActive) {";
    html += "          emergencyAlert.innerHTML = \"<div class='emergency-alert'><h3>🚨 ВІДСУТНІСТЬ 220В!</h3><p>Напруга: \" + (data.lastVoltage||0).toFixed(1) + \" В</p><button class='btn btn-danger' onclick='resetEmergency()'>🔄 СКИНУТИ</button></div>\";";
    html += "        } else emergencyAlert.innerHTML = '';";
    html += "      }";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "      lastUpdateTime = new Date();";
    html += "    })";
    html += "    .catch(error => console.error('Помилка оновлення:', error));";
    html += "}";

    // Execute command
    html += "function executeCommand() {";
    html += "  const input = document.getElementById('commandInput');";
    html += "  const command = input.value.trim();";
    html += "  if (!command) return;";
    html += "  const output = document.getElementById('commandOutput');";
    html += "  output.innerHTML += '\\n> ' + command + '\\n[Виконується...]';";
    html += "  output.scrollTop = output.scrollHeight;";
    html += "  fetch('/command', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'cmd=' + encodeURIComponent(command) })";
    html += "    .then(response => response.text())";
    html += "    .then(text => {";
    html += "      output.innerHTML = output.innerHTML.replace('[Виконується...]', text);";
    html += "      output.scrollTop = output.scrollHeight;";
    html += "      input.value = '';";
    html += "      setTimeout(updateStatus, 1000);";
    html += "    })";
    html += "    .catch(error => { output.innerHTML = output.innerHTML.replace('[Виконується...]', '❌ Помилка: ' + error); });";
    html += "}";

    html += "function quickCommand(cmd) { document.getElementById('commandInput').value = cmd; executeCommand(); }";
    html += "function clearOutput() { document.getElementById('commandOutput').innerHTML = '> Готово до команд...'; }";

    // Reset emergency
    html += "function resetEmergency() {";
    html += "  if (confirm('Скинути аварійний режим і повернутись до AUTO?')) {";
    html += "    fetch('/command', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'cmd=reset' })";
    html += "      .then(response => response.text())";
    html += "      .then(text => { alert('✅ ' + text); setTimeout(() => location.reload(), 500); })";
    html += "      .catch(error => alert('❌ Помилка: ' + error));";
    html += "  }";
    html += "}";

    // Toggle manual lock
    html += "function toggleManualLock() {";
    html += "  fetch('/command', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'cmd=lock' })";
    html += "    .then(response => response.text())";
    html += "    .then(text => console.log('Lock toggle:', text))";
    html += "    .catch(error => console.error('Lock toggle error:', error));";
    html += "}";

    // Enter key handler
    html += "document.getElementById('commandInput').addEventListener('keypress', function(e) { if (e.key === 'Enter') executeCommand(); });";

    // Update timer display
    html += "function updateNextUpdateTimer() {";
    html += "  const now = new Date();";
    html += "  const timeSinceUpdate = now - lastUpdateTime;";
    html += "  const timeLeft = Math.max(0, updateInterval - timeSinceUpdate);";
    html += "  document.getElementById('nextUpdate').textContent = Math.round(timeLeft/1000) + ' сек';";
    html += "}";

    // DOMContentLoaded
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  updateStatus();";
    html += "  setInterval(updateStatus, updateInterval);";
    html += "  setInterval(updateNextUpdateTimer, 1000);";
    // Uptime counter
    html += "  let uptimeSeconds = " + String(millis() / 1000) + ";";
    html += "  setInterval(() => {";
    html += "    uptimeSeconds++;";
    html += "    const hours = Math.floor(uptimeSeconds / 3600);";
    html += "    const minutes = Math.floor((uptimeSeconds % 3600) / 60);";
    html += "    const seconds = Math.floor(uptimeSeconds % 60);";
    html += "    document.getElementById('uptime').textContent = hours.toString().padStart(2, '0') + ':' + minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0');";
    html += "  }, 1000);";
    html += "});";
    html += "</script>";

    html += getHtmlFooter();

    // Перевірка WiFi перед відправкою
    if (WiFi.status() == WL_CONNECTED) {
        server.send(200, "text/html", html);
    }
}

// ============================================================================
// JSON API ДЛЯ СТАТУСУ
// ============================================================================

void handleStatus() {
    if (!checkAuth()) return;

    JsonDocument doc;

    doc["tempRoom"] = sensorData.roomValid ? sensorData.tempRoom : (float)NAN;
    doc["tempCarrier"] = sensorData.carrierValid ? sensorData.tempCarrier : (float)NAN;
    doc["tempBME"] = sensorData.bmeValid ? sensorData.tempBME : (float)NAN;
    doc["humidity"] = sensorData.bmeValid ? sensorData.humidity : (float)NAN;
    doc["pressure"] = sensorData.bmeValid ? sensorData.pressure : (float)NAN;
    doc["co2Level"] = sensorData.co2Valid ? sensorData.co2Level : (float)NAN;
    doc["co2Temp"] = sensorData.co2Valid ? getSCD30Temperature() : (float)NAN;
    doc["co2Humidity"] = sensorData.co2Valid ? getSCD30Humidity() : (float)NAN;
    doc["pumpPower"] = round(heatingState.pumpPower * 100.0 / 255.0);
    doc["fanPower"] = round(heatingState.fanPower * 100.0 / 255.0);
    doc["extractorPower"] = round(heatingState.extractorPower * 100.0 / 255.0);
    doc["extractorTimer"] = config.extractorTimer.enabled;
    doc["plantFanPower"] = plantFanGetCurrentPower();
    doc["plantFanOn"]    = plantFanGetTimerState();

    if (heatingState.emergencyMode) doc["mode"] = "АВАРІЯ";
    else if (heatingState.forceMode) doc["mode"] = "ФОРСАЖ";
    else if (heatingState.manualMode) doc["mode"] = "РУЧНИЙ";
    else doc["mode"] = "АВТО";

    // Інформація про аварію
    doc["powerOutageActive"] = powerOutageState.emergencyHeatingActive || powerOutageState.detected;
    doc["lastVoltage"] = powerOutageState.lastVoltage;

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
