// ============================================================================
// WEB_PAGES_CONTROL.CPP - Сторінки керування та часу
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Керування системою (handleControlPage)
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include "web_common.h"
#include "global_declarations.h"

extern WebServer server;
extern SystemConfig config;
extern HeatingState heatingState;
extern PowerOutageState powerOutageState;
extern SensorData sensorData;
extern SemaphoreHandle_t getSensorMutex();

// Forward declarations
extern bool checkAuth();
extern String getUkraineMarquee();

// ============================================================================
// СТОРІНКА КЕРУВАННЯ
// ============================================================================

void handleControlPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) return;

    String html = getHtmlHead("Керування");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("🎛️ ПАНЕЛЬ КЕРУВАННЯ СИСТЕМОЮ");

    // ДАТЧИКИ - показники
    float tempRoom = 0, tempCarrier = 0, tempBME = 0, humidity = 0, pressure = 0;
    bool roomValid = false, carrierValid = false, bmeValid = false;

    if (xSemaphoreTake(getSensorMutex(), pdMS_TO_TICKS(100))) {
        tempRoom = sensorData.tempRoom;
        tempCarrier = sensorData.tempCarrier;
        tempBME = sensorData.tempBME;
        humidity = sensorData.humidity;
        pressure = sensorData.pressure;
        roomValid = sensorData.roomValid;
        carrierValid = sensorData.carrierValid;
        bmeValid = sensorData.bmeValid;
        xSemaphoreGive(getSensorMutex());
    }

    float targetTemp = (config.tempMin + config.tempMax) / 2.0f;

    html += "<div class='control-section' style='border-left-color: #2196F3;'>";
    html += "<h3>📊 ПОКАЗНИКИ ДАТЧИКІВ</h3>";
    html += "<div style='display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px;'>";

    // Температура кімнати
    html += "<div class='sensor-tile" + String(roomValid ? "" : " err") + "' style='border-top-color:#fb7185'>";
    html += "<div class='lbl'>🏠 Кімната</div><div class='val' style='color:" + String(roomValid ? "#fb7185" : "var(--e)") + "'>";
    html += roomValid ? String(tempRoom, 1) + "°C" : "---";
    html += "</div></div>";

    // Температура теплоносія
    html += "<div class='sensor-tile" + String(carrierValid ? "" : " err") + "' style='border-top-color:#f59e0b'>";
    html += "<div class='lbl'>🔥 Теплоносій</div><div class='val' style='color:" + String(carrierValid ? "#f59e0b" : "var(--e)") + "'>";
    html += carrierValid ? String(tempCarrier, 1) + "°C" : "---";
    html += "</div></div>";

    // Вологість
    html += "<div class='sensor-tile" + String(bmeValid ? "" : " err") + "' style='border-top-color:#38bdf8'>";
    html += "<div class='lbl'>💧 Вологість</div><div class='val' style='color:" + String(bmeValid ? "#38bdf8" : "var(--e)") + "'>";
    html += bmeValid ? String(humidity, 0) + "%" : "---";
    html += "</div></div>";

    // Тиск
    html += "<div class='sensor-tile" + String(bmeValid ? "" : " err") + "' style='border-top-color:#a78bfa'>";
    html += "<div class='lbl'>🌡️ Тиск</div><div class='val' style='color:" + String(bmeValid ? "#a78bfa" : "var(--e)") + "'>";
    html += bmeValid ? String(pressure, 0) + " hPa" : "---";
    html += "</div></div>";

    html += "</div>";

    // Цільова температура
    html += "<div style='margin-top: 14px; padding: 10px; background: var(--s); border: 1px solid var(--b); border-radius: 8px; text-align: center;'>";
    html += "<span style='color: var(--m);'>🎯 Ціль: </span>";
    html += "<strong style='color: var(--ok);'>" + String(targetTemp, 1) + "°C</strong>";
    html += "<span style='color: var(--m); margin-left: 10px;'>(діапазон " + String(config.tempMin, 1) + " - " + String(config.tempMax, 1) + "°C)</span>";
    html += "</div>";
    html += "</div>";

    // НАСОС
    html += "<div class='control-section'>";
    html += "<h3>🔵 НАСОС (A)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String(round(heatingState.pumpPower * 100.0 / 255.0)) + "' class='slider' id='pumpSlider' oninput='updatePump(this.value)'>";
    html += "<span class='slider-value' id='pumpValue'>" + String(round(heatingState.pumpPower * 100.0 / 255.0)) + "%</span>";
    html += "<div style='margin-top: 10px;'>";
    html += "<button class='btn' onclick=\"setPower('pump', 0)\">ВИМК</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 30)\">30%</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 50)\">50%</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 80)\">80%</button>";
    html += "<button class='btn' onclick=\"setPower('pump', 100)\">100%</button>";
    html += "</div>";
    html += "</div>";

    // ВЕНТИЛЯТОР
    html += "<div class='control-section'>";
    html += "<h3>🟠 ВЕНТИЛЯТОР (B)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String(round(heatingState.fanPower * 100.0 / 255.0)) + "' class='slider' id='fanSlider' oninput='updateFan(this.value)'>";
    html += "<span class='slider-value' id='fanValue'>" + String(round(heatingState.fanPower * 100.0 / 255.0)) + "%</span>";
    html += "<div style='margin-top: 10px;'>";
    html += "<button class='btn' onclick=\"setPower('fan', 0)\">ВИМК</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 30)\">30%</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 50)\">50%</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 80)\">80%</button>";
    html += "<button class='btn' onclick=\"setPower('fan', 100)\">100%</button>";
    html += "</div>";
    html += "</div>";

    // ВИТЯЖКА
    html += "<div class='control-section'>";
    html += "<h3>🟢 ВИТЯЖКА (C)</h3>";
    html += "<input type='range' min='0' max='100' value='" + String(round(heatingState.extractorPower * 100.0 / 255.0)) + "' class='slider' id='extractorSlider' oninput='updateExtractor(this.value)'>";
    html += "<span class='slider-value' id='extractorValue'>" + String(round(heatingState.extractorPower * 100.0 / 255.0)) + "%</span>";
    html += "<div style='margin-top: 10px;'>";
    html += "<button class='btn' onclick=\"setPower('extractor', 0)\">ВИМК</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 30)\">30%</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 50)\">50%</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 80)\">80%</button>";
    html += "<button class='btn' onclick=\"setPower('extractor', 100)\">100%</button>";
    html += "</div>";
    html += "</div>";

    // РЕЖИМИ РОБОТИ
    html += "<div class='control-section'>";
    html += "<h3>⚙️ РЕЖИМИ РОБОТИ</h3>";
    html += "<div class='mode-buttons'>";
    html += "<button class='mode-btn" + String(!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode ? " active" : "") + "' onclick=\"sendCmd('auto')\" data-mode='auto'>🤖 АВТО</button>";
    html += "<button class='mode-btn" + String(heatingState.manualMode ? " active" : "") + "' onclick=\"sendCmd('manual')\" data-mode='manual'>✋ РУЧНИЙ</button>";
    html += "</div>";

    // Опція блокування ручного режиму
    html += "<div style='margin-top: 20px; padding: 15px; background: rgba(16,185,129,.14); border: 1px solid var(--ok); border-left: 4px solid var(--ok); border-radius: 10px;'>";
    html += "<label style='display: flex; align-items: center; cursor: pointer; font-size: 16px; font-weight: 500;'>";
    html += "<input type='checkbox' id='lockManualMode' " + String(heatingState.manualModeLocked ? "checked" : "") + " onchange=\"toggleLockManual()\" style='width: 20px; height: 20px; margin-right: 10px; cursor: pointer;'>";
    html += "<span>🔒 Блокувати ручний режим (без автоповернення)</span>";
    html += "</label>";
    html += "<div style='margin-top: 8px; font-size: 13px; color: var(--m);'>";
    html += "Коли увімкнено: ручний режим не скасується автоматично через 15 хвилин";
    html += "</div>";
    html += "</div>";

    // Кнопка скидання аварійного режиму
    if (powerOutageState.detected || powerOutageState.emergencyHeatingActive) {
        html += "<div style='margin-top: 15px;'>";
        html += "<button class='btn btn-danger' style='width: 100%; padding: 15px; font-size: 16px; font-weight: bold;' onclick=\"resetEmergency()\">🔄 СКИНУТИ АВАРІЙНИЙ РЕЖИМ</button>";
        html += "</div>";
    }

    html += "<div style='margin-top: 15px; padding: 12px; background: rgba(245,158,11,.14); border: 1px solid var(--w); border-left: 4px solid var(--w); border-radius: 10px; font-size: 14px;'>";
    html += "<strong>ℹ️ Автоматичні режими:</strong><br>";
    html += "⚡ <strong>ФОРСАЖ</strong> - вмикається при температурі < 20°C (80% потужність)<br>";
    html += "🚨 <strong>АВАРІЯ</strong> - вмикається при температурі < 18°C (100% потужність)<br>";
    html += "<span style='color: #d9534f;'><strong>⚠️ Коли блокування ручного режиму УВІМКНЕНО:</strong> форсаж та аварія НЕ активуються</span>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // container

    // JavaScript
    html += "<script>";
    html += "function setPower(device, value) {";
    html += "  sendCmd('manual');";
    html += "  const slider = document.getElementById(device + 'Slider');";
    html += "  const display = document.getElementById(device + 'Value');";
    html += "  if (slider) slider.value = value;";
    html += "  if (display) display.textContent = value + '%';";
    html += "  sendCmd(device + ' ' + value);";
    html += "}";
    html += "var _tP,_tF,_tE;";
    html += "function updatePump(v) { document.getElementById('pumpValue').textContent = v + '%'; clearTimeout(_tP); _tP = setTimeout(function(){ sendCmd('manual'); sendCmd('pump ' + v); }, 150); }";
    html += "function updateFan(v) { document.getElementById('fanValue').textContent = v + '%'; clearTimeout(_tF); _tF = setTimeout(function(){ sendCmd('manual'); sendCmd('fan ' + v); }, 150); }";
    html += "function updateExtractor(v) { document.getElementById('extractorValue').textContent = v + '%'; clearTimeout(_tE); _tE = setTimeout(function(){ sendCmd('manual'); sendCmd('extractor ' + v); }, 150); }";
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
    html += "function toggleLockManual() {";
    html += "  sendCmd('lock');";
    html += "}";
    html += "</script>";

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

