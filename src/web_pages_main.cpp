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
extern const char* resetReasonToStr(int r);
extern int getDiagLastResetReason();
extern uint32_t getDiagBootCount();
extern uint32_t getDiagCrashCount();

// ============================================================================
// ГОЛОВНА СТОРІНКА
// ============================================================================

void handleRoot() {
    if (!checkAuth()) return;

    // Перевірка WiFi перед відправкою великої відповіді
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        return;
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    String html;
    html.reserve(4096);
    auto flushChunk = [&]() {
        if (html.length() > 0) {
            server.sendContent(html);
            html = "";
            yield();
        }
    };

    html = getHtmlHead("Клімат-контроль");
    html += "<div class='container'>";
    html += getNavHeader("");
    html += "<div class='header'>";
    uint32_t sketchKB = ESP.getSketchSize() / 1024;
    uint32_t freeSketchKB = ESP.getFreeSketchSpace() / 1024;
    String verTitle = String(VERSION) + " | Прошивка: " + String(sketchKB) + " KB | Вільно OTA: " + String(freeSketchKB) + " KB | Рядків: " + String(TOTAL_CODE_LINES);
    html += "<h1>🌡️ КЛІМАТ-КОНТРОЛЬ <span onclick='showVersion()' title='" + verTitle + "' style='cursor: pointer; color: var(--a);'>" VERSION "</span></h1>";

    // JavaScript для показу версії
    html += "<script>";
    html += "function showVersion() {";
    html += "  var msg = '" VERSION "';";
    html += "  msg += '\\n" VERSION_COMMENT "';";
    html += "  msg += '\\n\\nЗібрано: " BUILD_DATE " " BUILD_TIME "';";
    html += "  msg += '\\nРядків коду: " + String(TOTAL_CODE_LINES) + "';";
    html += "  msg += '\\nРозмір прошивки: " + String(sketchKB) + " KB (вільно: " + String(freeSketchKB) + " KB)';";
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

    // Контейнер для аварійного/ЕКО повідомлення та швидкого перемикача
    html += "<div style='margin-top: 10px; display: flex; gap: 8px; flex-wrap: wrap; align-items: center;'>";
    html += "<button type='button' id='ecoToggleBtn' class='btn' style='margin:0; padding:8px 14px; font-size:13.5px; background:";
    html += (config.powerSave.activationMode == ECO_MODE_FORCE_ON) ? "#f59e0b;color:#fff;" : "#334155;color:#fff;";
    html += "' onclick='toggleEcoForce()'>";
    html += (config.powerSave.activationMode == ECO_MODE_FORCE_ON) ? "🔋 ЕКО: ПРИМУСОВО (перемкнути на АВТО)" : "🔋 Увімкнути ЕКО-режим вручну";
    html += "</button>";
    html += "<a href='/settings' onclick=\"localStorage.setItem('settingsTab','12')\" style='font-size:13px; color: var(--a); text-decoration:none;'>⚙️ Налаштування ЕКО</a>";
    html += "</div>";

    html += "<div id='emergencyAlert'>";
    if (isEcoModeActive()) {
        html += "<div class='emergency-alert'>";
        html += powerOutageState.detected ? "<h3>🔋 БЛЕКАУТ — ЕКО-РЕЖИМ АКТИВНИЙ!</h3>" : "<h3>🔋 РУЧНИЙ ЕКО-РЕЖИМ АКТИВНИЙ</h3>";
        html += "<p style='margin: 5px 0;'>Напруга 220В: " + String(powerOutageState.lastVoltage, 1) + " В</p>";
        html += "<button class='btn btn-danger' style='margin-top: 10px; padding: 10px 16px; font-weight: bold;' onclick='resetEmergency()'>🔄 СКИНУТИ</button>";
        html += "</div>";
    }
    html += "</div>";

    html += "</div>"; // .header
    flushChunk();

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
    flushChunk();
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
    html += "<option value='eco on'>eco on - примусовий ЕКО-режим</option>";
    html += "<option value='eco auto'>eco auto - авто ЕКО по 220В</option>";
    html += "<option value='eco off'>eco off - вимкнути ЕКО</option>";
    html += "<option value='eco status'>eco status - статус ЕКО-профілю</option>";
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
    flushChunk();

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

    // Update status via AJAX (з захистом від накладання запитів та таймаутом)
    html += "function fin(v) { return v !== undefined && v !== null && isFinite(v); }";
    html += "let _statusBusy = false;";
    html += "function updateStatus() {";
    html += "  if (_statusBusy) return;";
    html += "  _statusBusy = true;";
    html += "  const ctrl = new AbortController();";
    html += "  const tid = setTimeout(() => ctrl.abort(), 4500);";
    html += "  fetch('/status', {cache: 'no-store', signal: ctrl.signal})";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      if (fin(data.tempRoom)) document.getElementById('tempRoom').textContent = data.tempRoom.toFixed(1) + '°C';";
    html += "      else document.getElementById('tempRoom').textContent = '🚨 ПОМИЛКА';";
    html += "      if (fin(data.tempCarrier)) document.getElementById('tempCarrier').textContent = data.tempCarrier.toFixed(1) + '°C';";
    html += "      else document.getElementById('tempCarrier').textContent = '🚨 ПОМИЛКА';";
    html += "      if (fin(data.tempBME)) document.getElementById('tempBME').textContent = data.tempBME.toFixed(1) + '°C';";
    html += "      else document.getElementById('tempBME').textContent = '🚨 ПОМИЛКА';";
    html += "      if (fin(data.humidity)) document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';";
    html += "      else document.getElementById('humidity').textContent = '🚨 ПОМИЛКА';";
    html += "      if (fin(data.pressure)) document.getElementById('pressure').textContent = data.pressure.toFixed(1) + ' hPa';";
    html += "      else document.getElementById('pressure').textContent = '🚨 ПОМИЛКА';";
    html += "      if (data.humidifierActive !== undefined) { const el = document.getElementById('humidifierStatus'); if (el) el.textContent = data.humidifierActive ? 'ВКЛ' : 'ВИМК'; }";
    html += "      if (fin(data.co2Level)) {";
    html += "        document.getElementById('co2Level').textContent = data.co2Level.toFixed(0) + ' ppm';";
    html += "        document.getElementById('co2Status').textContent = '✓ OK';";
    html += "      } else {";
    html += "        document.getElementById('co2Level').textContent = '🚨 ПОМИЛКА';";
    html += "        document.getElementById('co2Status').textContent = '✗ НЕ ВІДПОВІДАЄ';";
    html += "      }";
    html += "      if (fin(data.co2Temp)) document.getElementById('co2Temp').textContent = data.co2Temp.toFixed(1) + '°C';";
    html += "      if (fin(data.co2Humidity)) document.getElementById('co2Hum').textContent = data.co2Humidity.toFixed(1) + '%';";
    html += "      if (fin(data.pumpPower)) { const el = document.getElementById('pumpPower'); if (el) el.textContent = Math.round(data.pumpPower) + '%'; }";
    html += "      if (fin(data.fanPower)) { const el = document.getElementById('fanPower'); if (el) el.textContent = Math.round(data.fanPower) + '%'; }";
    html += "      if (fin(data.extractorPower)) { const el = document.getElementById('extractorPower'); if (el) el.textContent = Math.round(data.extractorPower) + '%'; }";
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
    // Emergency / ECO alert handling
    html += "      const ecoBtn = document.getElementById('ecoToggleBtn');";
    html += "      if (ecoBtn && data.ecoMode !== undefined) {";
    html += "        window._currentEcoMode = data.ecoMode;";
    html += "        if (data.ecoMode === 1) { ecoBtn.textContent = '🔋 ЕКО: ПРИМУСОВО (перемкнути на АВТО)'; ecoBtn.style.background = '#f59e0b'; }";
    html += "        else { ecoBtn.textContent = '🔋 Увімкнути ЕКО-режим вручну'; ecoBtn.style.background = '#334155'; }";
    html += "      }";
    html += "      const emergencyAlert = document.getElementById('emergencyAlert');";
    html += "      if (emergencyAlert) {";
    html += "        if (data.ecoActive || data.powerOutageActive) {";
    html += "          const dur = data.ecoDurationSec || 0;";
    html += "          const hh = String(Math.floor(dur / 3600)).padStart(2, '0');";
    html += "          const mm = String(Math.floor((dur % 3600) / 60)).padStart(2, '0');";
    html += "          const ss = String(dur % 60).padStart(2, '0');";
    html += "          const title = data.powerOutageDetected ? '🔋 БЛЕКАУТ — ЕКО-РЕЖИМ АКТИВНИЙ!' : '🔋 РУЧНИЙ ЕКО-РЕЖИМ АКТИВНИЙ';";
    html += "          emergencyAlert.innerHTML = \"<div class='emergency-alert'><h3>\" + title + \"</h3><p>Напруга 220В: \" + (data.lastVoltage||0).toFixed(1) + \" В | Тривалість: \" + hh + ':' + mm + ':' + ss + \"</p><button class='btn btn-danger' onclick='resetEmergency()'>🔄 СКИНУТИ</button></div>\";";
    html += "        } else emergencyAlert.innerHTML = '';";
    html += "      }";
    html += "      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();";
    html += "      lastUpdateTime = new Date();";
    html += "    })";
    html += "    .catch(error => console.error('Помилка оновлення:', error))";
    html += "    .finally(() => { clearTimeout(tid); _statusBusy = false; });";
    html += "}";

    // Execute command
    html += "function executeCommand() {";
    html += "  const input = document.getElementById('commandInput');";
    html += "  const command = input.value.trim();";
    html += "  if (!command) return;";
    html += "  const output = document.getElementById('commandOutput');";
    html += "  output.textContent += '\\n> ' + command + '\\n[Виконується...]';";
    html += "  output.scrollTop = output.scrollHeight;";
    html += "  fetch('/command', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'cmd=' + encodeURIComponent(command) })";
    html += "    .then(response => response.text())";
    html += "    .then(text => {";
    html += "      output.textContent = output.textContent.replace('[Виконується...]', text);";
    html += "      output.scrollTop = output.scrollHeight;";
    html += "      input.value = '';";
    html += "      setTimeout(updateStatus, 1000);";
    html += "    })";
    html += "    .catch(error => { output.textContent = output.textContent.replace('[Виконується...]', '❌ Помилка: ' + error); });";
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

    // Toggle manual ECO mode
    html += "function toggleEcoForce() {";
    html += "  const nextCmd = (window._currentEcoMode === 1) ? 'eco auto' : 'eco on';";
    html += "  fetch('/command', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'cmd=' + encodeURIComponent(nextCmd) })";
    html += "    .then(r => r.text()).then(t => { console.log(t); updateStatus(); })";
    html += "    .catch(e => alert('❌ Помилка: ' + e));";
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

    flushChunk();
    server.sendContent("");
}

// ============================================================================
// JSON API ДЛЯ СТАТУСУ
// ============================================================================

void handleStatus() {
    if (!checkAuth()) return;

    JsonDocument doc;

    SensorData sdCopy;
    if (xSemaphoreTake(getSensorMutex(), pdMS_TO_TICKS(50))) {
        sdCopy = sensorData;
        xSemaphoreGive(getSensorMutex());
    } else {
        sdCopy = sensorData;
    }

    doc["tempRoom"] = sdCopy.roomValid ? sdCopy.tempRoom : (float)NAN;
    doc["tempCarrier"] = sdCopy.carrierValid ? sdCopy.tempCarrier : (float)NAN;
    doc["tempBME"] = sdCopy.bmeValid ? sdCopy.tempBME : (float)NAN;
    doc["humidity"] = sdCopy.bmeValid ? sdCopy.humidity : (float)NAN;
    doc["pressure"] = sdCopy.bmeValid ? sdCopy.pressure : (float)NAN;
    doc["co2Level"] = sdCopy.co2Valid ? sdCopy.co2Level : (float)NAN;
    doc["co2Temp"] = sdCopy.co2Valid ? getSCD30Temperature() : (float)NAN;
    doc["co2Humidity"] = sdCopy.co2Valid ? getSCD30Humidity() : (float)NAN;
    doc["humidifierActive"] = humidifierState.active;
    if (xSemaphoreTake(getHeatingMutex(), pdMS_TO_TICKS(50))) {
        doc["pumpPower"] = round(heatingState.pumpPower * 100.0 / 255.0);
        doc["fanPower"] = round(heatingState.fanPower * 100.0 / 255.0);
        doc["extractorPower"] = round(heatingState.extractorPower * 100.0 / 255.0);
        xSemaphoreGive(getHeatingMutex());
    } else {
        doc["pumpPower"] = round(heatingState.pumpPower * 100.0 / 255.0);
        doc["fanPower"] = round(heatingState.fanPower * 100.0 / 255.0);
        doc["extractorPower"] = round(heatingState.extractorPower * 100.0 / 255.0);
    }
    doc["extractorTimer"] = config.extractorTimer.enabled;
    doc["plantFanPower"] = plantFanGetCurrentPower();
    doc["plantFanOn"]    = plantFanGetTimerState();

    if (heatingState.emergencyMode) doc["mode"] = "АВАРІЯ";
    else if (heatingState.forceMode) doc["mode"] = "ФОРСАЖ";
    else if (heatingState.manualMode) doc["mode"] = "РУЧНИЙ";
    else doc["mode"] = "АВТО";

    // Інформація про аварію та ЕКО-режим
    doc["powerOutageActive"]   = powerOutageState.emergencyHeatingActive || powerOutageState.detected;
    doc["powerOutageDetected"] = powerOutageState.detected;
    doc["ecoActive"]           = isEcoModeActive();
    doc["ecoMode"]             = config.powerSave.activationMode;
    doc["ecoHeatingOn"]        = powerOutageState.ecoHeatingOn;
    doc["ecoDurationSec"]      = (isEcoModeActive() && powerOutageState.ecoStartTime > 0)
                                 ? (unsigned long)((millis() - powerOutageState.ecoStartTime) / 1000UL)
                                 : 0UL;
    doc["lastVoltage"]         = powerOutageState.lastVoltage;

    // Інформація про блокування ручного режиму
    doc["manualModeLocked"] = heatingState.manualModeLocked;

    doc["time"] = getTimeString();
    doc["memory"] = ESP.getFreeHeap() / 1024;
    doc["minMemory"] = ESP.getMinFreeHeap() / 1024;
    doc["maxBlock"] = ESP.getMaxAllocHeap() / 1024;
    doc["uptime"] = millis() / 1000;
    doc["resetReason"] = resetReasonToStr(getDiagLastResetReason());
    doc["bootCount"] = getDiagBootCount();
    doc["crashCount"] = getDiagCrashCount();

    LoggerStats jsonStats = getLoggerStats();
    doc["historyCount"] = jsonStats.totalRecordsRAM;
    doc["spiffsRecords"] = jsonStats.totalRecordsSPIFFS;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
