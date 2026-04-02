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
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("Керування");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("🎛️ ПАНЕЛЬ КЕРУВАННЯ СИСТЕМОЮ");

    // Додаткові стилі для слайдерів та режимів
    html += "<style>";
    // Покращені слайдери
    html += ".slider { -webkit-appearance: none; appearance: none; width: 100%; height: 8px; border-radius: 5px; background: #ddd; outline: none; margin: 15px 0; transition: background 0.3s; }";
    html += ".slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 28px; height: 28px; border-radius: 50%; background: #4CAF50; cursor: pointer; box-shadow: 0 2px 6px rgba(0,0,0,0.2); transition: all 0.3s; }";
    html += ".slider::-webkit-slider-thumb:hover { transform: scale(1.2); box-shadow: 0 3px 10px rgba(76, 175, 80, 0.5); }";
    html += ".slider::-webkit-slider-thumb:active { transform: scale(1.1); }";
    html += ".slider::-moz-range-thumb { width: 28px; height: 28px; border-radius: 50%; background: #4CAF50; cursor: pointer; border: none; box-shadow: 0 2px 6px rgba(0,0,0,0.2); }";
    html += ".slider:hover { background: #ccc; }";
    html += ".slider-value { display: inline-block; min-width: 50px; text-align: center; font-weight: 700; font-size: 20px; color: #4CAF50; margin-left: 10px; }";

    // Секції керування
    html += ".control-section { margin-bottom: 30px; padding: 20px; background: #f9f9f9; border-radius: 8px; border-left: 4px solid #4CAF50; }";
    html += ".control-section h3 { margin-top: 0; color: #2c3e50; font-size: 18px; }";

    // Кнопки режимів
    html += ".mode-buttons { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; }";
    html += ".mode-btn { padding: 15px 20px; border: 2px solid #ddd; background: #f5f5f5; color: #333; border-radius: 8px; cursor: pointer; font-size: 16px; font-weight: 500; transition: all 0.3s; position: relative; overflow: hidden; }";
    html += ".mode-btn:hover { transform: translateY(-3px); box-shadow: 0 6px 12px rgba(0,0,0,0.15); }";
    html += ".mode-btn.active { background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%); color: white; border-color: #4CAF50; box-shadow: 0 4px 15px rgba(76, 175, 80, 0.4); animation: pulse 2s infinite; }";
    html += "@keyframes pulse { 0%, 100% { box-shadow: 0 4px 15px rgba(76, 175, 80, 0.4); } 50% { box-shadow: 0 6px 20px rgba(76, 175, 80, 0.6); } }";
    html += ".mode-btn.active::before { content: '✓ '; font-weight: 600; margin-right: 5px; }";

    // Мобільна оптимізація
    html += "@media (max-width: 768px) {";
    html += "  .control-section { padding: 15px; }";
    html += "  .slider { height: 12px; margin: 20px 0; }";
    html += "  .slider::-webkit-slider-thumb { width: 36px; height: 36px; }";
    html += "  .slider::-moz-range-thumb { width: 36px; height: 36px; }";
    html += "  .slider-value { font-size: 24px; display: block; margin: 10px 0; }";
    html += "  .control-section h3 { font-size: 16px; }";
    html += "  .mode-btn { font-size: 14px; padding: 12px 15px; }";
    html += "}";

    // Додаткова оптимізація для маленьких екранів
    html += "@media (max-width: 480px) {";
    html += "  .btn { display: inline-block; width: calc(33.33% - 6px); margin: 3px; padding: 10px 5px; font-size: 12px; }";
    html += "}";
    html += "</style>";

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
    html += "<div style='padding: 12px; background: " + String(roomValid ? "#e3f2fd" : "#ffebee") + "; border-radius: 8px; text-align: center;'>";
    html += "<div style='font-size: 12px; color: #666;'>🏠 Кімната</div>";
    html += "<div style='font-size: 24px; font-weight: bold; color: " + String(roomValid ? "#1976D2" : "#c62828") + ";'>";
    html += roomValid ? String(tempRoom, 1) + "°C" : "---";
    html += "</div></div>";

    // Температура теплоносія
    html += "<div style='padding: 12px; background: " + String(carrierValid ? "#fff3e0" : "#ffebee") + "; border-radius: 8px; text-align: center;'>";
    html += "<div style='font-size: 12px; color: #666;'>🔥 Теплоносій</div>";
    html += "<div style='font-size: 24px; font-weight: bold; color: " + String(carrierValid ? "#ef6c00" : "#c62828") + ";'>";
    html += carrierValid ? String(tempCarrier, 1) + "°C" : "---";
    html += "</div></div>";

    // Вологість
    html += "<div style='padding: 12px; background: " + String(bmeValid ? "#e8f5e9" : "#ffebee") + "; border-radius: 8px; text-align: center;'>";
    html += "<div style='font-size: 12px; color: #666;'>💧 Вологість</div>";
    html += "<div style='font-size: 24px; font-weight: bold; color: " + String(bmeValid ? "#388E3C" : "#c62828") + ";'>";
    html += bmeValid ? String(humidity, 0) + "%" : "---";
    html += "</div></div>";

    // Тиск
    html += "<div style='padding: 12px; background: " + String(bmeValid ? "#f3e5f5" : "#ffebee") + "; border-radius: 8px; text-align: center;'>";
    html += "<div style='font-size: 12px; color: #666;'>🌡️ Тиск</div>";
    html += "<div style='font-size: 24px; font-weight: bold; color: " + String(bmeValid ? "#7B1FA2" : "#c62828") + ";'>";
    html += bmeValid ? String(pressure, 0) + " hPa" : "---";
    html += "</div></div>";

    html += "</div>";

    // Цільова температура
    html += "<div style='margin-top: 15px; padding: 10px; background: #e8f5e9; border-radius: 5px; text-align: center;'>";
    html += "<span style='color: #555;'>🎯 Ціль: </span>";
    html += "<strong style='color: #2e7d32;'>" + String(targetTemp, 1) + "°C</strong>";
    html += "<span style='color: #888; margin-left: 10px;'>(діапазон " + String(config.tempMin, 1) + " - " + String(config.tempMax, 1) + "°C)</span>";
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
    html += "<div style='margin-top: 20px; padding: 15px; background: #e8f5e9; border-left: 4px solid #4CAF50; border-radius: 5px;'>";
    html += "<label style='display: flex; align-items: center; cursor: pointer; font-size: 16px; font-weight: 500;'>";
    html += "<input type='checkbox' id='lockManualMode' " + String(heatingState.manualModeLocked ? "checked" : "") + " onchange=\"toggleLockManual()\" style='width: 20px; height: 20px; margin-right: 10px; cursor: pointer;'>";
    html += "<span>🔒 Блокувати ручний режим (без автоповернення)</span>";
    html += "</label>";
    html += "<div style='margin-top: 8px; font-size: 13px; color: #555;'>";
    html += "Коли увімкнено: ручний режим не скасується автоматично через 15 хвилин";
    html += "</div>";
    html += "</div>";

    // Кнопка скидання аварійного режиму
    if (powerOutageState.detected || powerOutageState.emergencyHeatingActive) {
        html += "<div style='margin-top: 15px;'>";
        html += "<button class='btn btn-danger' style='width: 100%; padding: 15px; font-size: 16px; font-weight: bold;' onclick=\"resetEmergency()\">🔄 СКИНУТИ АВАРІЙНИЙ РЕЖИМ</button>";
        html += "</div>";
    }

    html += "<div style='margin-top: 15px; padding: 12px; background: #fff3cd; border-left: 4px solid #ffc107; border-radius: 5px; font-size: 14px;'>";
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
    html += "function toggleLockManual() {";
    html += "  const checkbox = document.getElementById('lockManualMode');";
    html += "  if (checkbox.checked) {";
    html += "    sendCmd('lock_manual_on');";
    html += "  } else {";
    html += "    sendCmd('lock_manual_off');";
    html += "  }";
    html += "}";
    html += "</script>";

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

