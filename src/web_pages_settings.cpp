// ============================================================================
// WEB_PAGES_SETTINGS.CPP - Сторінки налаштувань
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Налаштування системи (handleSettingsPage, handleSaveSettings)
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include "web_common.h"
#include "global_declarations.h"

extern WebServer server;
extern Preferences prefs;
extern SystemConfig config;
extern VentilationState ventState;

// Forward declarations
extern bool checkAuth();
extern bool checkCSRF();
extern void saveConfiguration();
extern String getUkraineMarquee();

// ============================================================================
// СТОРІНКА НАЛАШТУВАНЬ (з вкладками)
// ============================================================================

void handleSettingsPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("Налаштування системи");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ СИСТЕМИ");

    // Стилі для вкладок
    html += "<style>";
    html += ".tabs { display: flex; gap: 5px; margin-bottom: 20px; flex-wrap: wrap; border-bottom: 2px solid #ddd; }";
    html += ".tab { padding: 12px 20px; background: #e0e0e0; border: none; border-radius: 8px 8px 0 0; cursor: pointer; font-size: 15px; font-weight: 500; transition: all 0.3s; color: #555; }";
    html += ".tab:hover { background: #d0d0d0; }";
    html += ".tab.active { background: #4CAF50; color: white; box-shadow: 0 -2px 8px rgba(76, 175, 80, 0.3); }";
    html += ".tab-content { display: none; animation: fadeIn 0.3s; }";
    html += ".tab-content.active { display: block; }";
    html += "@keyframes fadeIn { from { opacity: 0; transform: translateY(-10px); } to { opacity: 1; transform: translateY(0); } }";
    html += ".section { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 25px; border-left: 4px solid #3498db; }";
    html += ".section h3 { margin-top: 0; color: #2980b9; }";
    html += ".form-group { margin-bottom: 20px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #34495e; }";
    html += "input[type='number'], input[type='text'], input[type='password'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; font-size: 16px; }";
    html += ".btn-secondary { background: #3498db; margin-left: 10px; }";
    html += ".btn-secondary:hover { background: #2980b9; }";

    // Мобільні стилі для вкладок
    html += "@media (max-width: 768px) {";
    html += "  .tabs { gap: 3px; }";
    html += "  .tab { padding: 10px 12px; font-size: 13px; }";
    html += "  .section { padding: 15px; }";
    html += "  .btn-secondary { margin-left: 0; margin-top: 10px; }";
    html += "}";
    html += "@media (max-width: 480px) {";
    html += "  .tab { padding: 8px 10px; font-size: 12px; flex: 1 1 calc(25% - 3px); text-align: center; }";
    html += "}";
    html += "</style>";

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
    html += "<button type='button' class='tab' onclick='switchTab(8)'>📊 Sync</button>";
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
    html += "</div>"; // tab0

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
    html += "</div>"; // tab1

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
    html += "</div>"; // tab2

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
    html += "</div>"; // tab3

    // TAB 4: АВАРІЯ
    html += "<div class='tab-content' id='tab4'>";

    // Пороги температури для форсажу та аварії
    html += "<div class='section'>";
    html += "<h3>⚡ ПОРОГИ ФОРСАЖУ ТА АВАРІЇ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Поріг ФОРСАЖУ - критично низька температура (°C):</label>";
    html += "<input type='number' step='0.5' name='tempCriticalLow' value='" + String(config.tempCriticalLow, 1) + "' min='15.0' max='25.0'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Якщо температура кімнати ≤ цього порогу, вмикається ФОРСАЖ (насос 80%, вентилятор 80%)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Поріг АВАРІЇ - аварійна температура (°C):</label>";
    html += "<input type='number' step='0.5' name='tempEmergencyLow' value='" + String(config.tempEmergencyLow, 1) + "' min='10.0' max='22.0'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Якщо температура кімнати ≤ цього порогу, вмикається АВАРІЯ (насос 100%, вентилятор 100%)</small>";
    html += "</div>";
    html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-top: 10px; border-left: 4px solid #ffc107;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Поріг аварії має бути НИЖЧЕ за поріг форсажу!";
    html += "</div>";
    html += "</div>";

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
    html += "</div>"; // tab4

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
    html += "<label><input type='checkbox' name='autoStatus'" + String(config.autoStatusEnabled ? " checked" : "") + "> Автоматичний вивід статусу в Serial</label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='use24h'" + String(config.use24hFormat ? " checked" : "") + "> Використовувати 24-годинний формат часу</label>";
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
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум адаптивного режиму (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='adaptiveMax' value='" + String(config.a_adaptive_max) + "' min='50' max='100'>";
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
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Нормальний діапазон макс (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='normalRangeMax' value='" + String(config.a_normal_range_max) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Стартова потужність насоса (%) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='bStartPercent' value='" + String(config.b_start_percent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Вікно аналізу тренду (секунди) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='trendWindow' value='" + String(config.trend_window_seconds) + "' min='60' max='600'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Поріг падіння температури (°C) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='tempDropThreshold' value='" + String(config.temp_drop_threshold, 1) + "' min='0.1' max='5.0' step='0.1'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Інтервал перевірки обігріву (секунди) <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='heatingCheckInt' value='" + String(config.heating_check_interval) + "' min='30' max='600'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Розмір історії даних <span style='color:#999;font-size:0.85em;'>[резерв]</span>:</label>";
    html += "<input type='number' name='historySize' value='" + String(config.history_size) + "' min='100' max='2000'>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab5

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
    html += "<div style='margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;width:100%;' onclick='toggleServoCalibration()' id='servoCalibBtn'>";
    html += ventState.calibrationMode ? "🔓 ВИЙТИ З КАЛІБРУВАННЯ" : "🔒 УВІЙТИ В КАЛІБРУВАННЯ";
    html += "</button>";
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
    html += "<button type='button' class='btn btn-success' onclick='gotoServoPosition(\"open\")'>➤ Відкрити</button>";
    html += "<button type='button' class='btn btn-danger' onclick='gotoServoPosition(\"closed\")'>➤ Закрити</button>";
    html += "</div>";

    // Збереження позицій
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn btn-success' onclick='saveServoPosition(\"closed\")'>💾 Зберегти як ЗАКРИТО</button>";
    html += "<button type='button' class='btn btn-success' onclick='saveServoPosition(\"open\")'>💾 Зберегти як ВІДКРИТО</button>";
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
    html += "<label><input type='checkbox' name='manualVent'" + String(config.manualVentControl ? " checked" : "") + "> Ручне керування заслонкою</label>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Вимкнути автоматичне регулювання заслонки (тільки вимикач)</small>";
    html += "</div>";

    html += "<div style='background: #fff3cd; padding: 10px; border-radius: 5px; margin-top: 10px;'>";
    html += "<small style='color: #856404;'>💡 <strong>Порада:</strong> Використовуйте кнопки для точного калібрування в реальному часі, або введіть значення вручну та збережіть через кнопку внизу форми.</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab6

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

    // Статична IP
    html += "<div class='section'>";
    html += "<h3>🌐 СТАТИЧНА IP (опціонально)</h3>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='use_static_ip' id='use_static_ip'";
    if (config.useStaticIP) html += " checked";
    html += " onchange='toggleStaticIP()'> Використовувати статичну IP адресу</label>";
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
    html += "<label><input type='checkbox' name='use_auth' id='use_auth'";
    if (config.useAuth) html += " checked";
    html += " onchange='toggleAuth()'> Увімкнути автентифікацію (логін/пароль)</label>";
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

    html += "</div>"; // tab7

    // TAB 8: GOOGLE SHEETS SYNC
    html += "<div class='tab-content' id='tab8'>";

    // Поріг логування
    html += "<div class='section'>";
    html += "<h3>📊 ПОРІГ ЗАПИСУ ДАНИХ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна зміна температури кімнати для запису (°C):</label>";
    html += "<input type='number' step='0.1' name='logTempThreshold' value='" + String(config.logTempThreshold, 1) + "' min='0.0' max='5.0'>";
    html += "<small style='color: #666; display: block; margin-top: 5px;'>Якщо температура змінилась менше ніж на цей поріг, рядок не буде записано. 0 = записувати всі зміни. Типове значення: 0.5°C</small>";
    html += "</div>";
    html += "</div>";

    // Інформація про синхронізацію
    html += "<div class='section'>";
    html += "<h3>ℹ️ ІНФОРМАЦІЯ</h3>";
    html += "<div style='background: #e8f5e9; padding: 15px; border-radius: 5px; border-left: 4px solid #4CAF50;'>";
    html += "<p><strong>Як працює фільтрація:</strong></p>";
    html += "<ul style='margin: 10px 0; padding-left: 20px;'>";
    html += "<li>Дані записуються в буфер тільки якщо температура кімнати змінилась на вказаний поріг</li>";
    html += "<li>Це зменшує кількість записів при стабільних умовах</li>";
    html += "<li>Менший поріг = більше записів, більший поріг = менше записів</li>";
    html += "</ul>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // tab8

    // Кнопки збереження
    html += "<div style='margin-top: 30px;'>";
    html += "<button type='submit' class='btn'>💾 ЗБЕРЕГТИ НАЛАШТУВАННЯ</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='window.location.href=\"/\"'>← НА ГОЛОВНУ</button>";
    html += "</div>";

    html += "</form>";

    // JavaScript
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

    html += "</div>"; // container

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

// ============================================================================
// ЗБЕРЕЖЕННЯ НАЛАШТУВАНЬ
// ============================================================================

void handleSaveSettings() {
    if (!checkAuth()) return;

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

    // Вологість
    if (server.hasArg("humMin")) {
        config.humidityConfig.minHumidity = server.arg("humMin").toFloat();
    }
    if (server.hasArg("humMax")) {
        config.humidityConfig.maxHumidity = server.arg("humMax").toFloat();
    }
    if (server.hasArg("humHyst")) {
        config.humidityConfig.hysteresis = server.arg("humHyst").toInt();
    }

    // Пристрої
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
    if (server.hasArg("fanMin")) {
        config.fanMinPercent = server.arg("fanMin").toInt();
    }

    // Таймер витяжки
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
    config.extractorTimer.enabled = server.hasArg("extEnabled");

    // Серво
    if (server.hasArg("servoSpeed")) {
        config.servoSpeed = constrain(server.arg("servoSpeed").toInt(), 5, 50);
    }
    if (server.hasArg("statusPeriod")) {
        config.statusPeriod = server.arg("statusPeriod").toInt() * 1000UL;
    }

    // Системні налаштування
    config.autoStatusEnabled = server.hasArg("autoStatus");
    config.use24hFormat = server.hasArg("use24h");
    config.manualVentControl = server.hasArg("manualVent");

    // Розширені параметри
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
        config.temp_drop_threshold = constrain(server.arg("tempDropThreshold").toFloat(), 0.1f, 5.0f);
    }
    if (server.hasArg("heatingCheckInt")) {
        config.heating_check_interval = constrain(server.arg("heatingCheckInt").toInt(), 30, 600);
    }
    if (server.hasArg("historySize")) {
        config.history_size = constrain(server.arg("historySize").toInt(), 100, 2000);
    }

    // Калібрування серво
    if (server.hasArg("servoClosed")) {
        config.servoClosedAngle = constrain(server.arg("servoClosed").toInt(), 0, 180);
    }
    if (server.hasArg("servoOpen")) {
        config.servoOpenAngle = constrain(server.arg("servoOpen").toInt(), 0, 180);
    }

    // Сезонні режими
    config.coolingMode = server.hasArg("coolingMode");
    config.seasonalHeatingDisable = server.hasArg("seasonalDisable");

    // Поріг логування для Google Sheets
    if (server.hasArg("logTempThreshold")) {
        config.logTempThreshold = constrain(server.arg("logTempThreshold").toFloat(), 0.0f, 5.0f);
    }

    // Пороги для режимів форсаж та аварія
    if (server.hasArg("tempCriticalLow")) {
        config.tempCriticalLow = constrain(server.arg("tempCriticalLow").toFloat(), 15.0f, 25.0f);
    }
    if (server.hasArg("tempEmergencyLow")) {
        config.tempEmergencyLow = constrain(server.arg("tempEmergencyLow").toFloat(), 10.0f, 22.0f);
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

    String html = getHtmlHead("Налаштування збережені");

    if (wifiChanged) {
        html += "<meta http-equiv='refresh' content='3;url=/'>";
    } else {
        html += "<meta http-equiv='refresh' content='2;url=/settings'>";
    }

    html += "<div class='container' style='text-align: center; padding: 50px;'>";
    html += "<h1>✅ Налаштування успішно збережені!</h1>";
    if (wifiChanged) {
        html += "<p><strong>⚠️ WiFi налаштування змінено! Пристрій перезавантажується...</strong></p>";
        html += "<p>Після перезавантаження підключіться до нової мережі та перейдіть за адресою пристрою.</p>";
    } else {
        html += "<p>Перенаправлення назад на сторінку налаштувань...</p>";
    }
    html += "</div>";
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);

    // Якщо змінились WiFi налаштування, перезавантажуємо пристрій
    if (wifiChanged) {
        delay(2000);
        ESP.restart();
    }
}
