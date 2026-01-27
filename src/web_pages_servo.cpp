// ============================================================================
// WEB_PAGES_SERVO.CPP - Сторінки калібрування серво
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Калібрування серво (handleServoPage, handleServoAPI)
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include "web_common.h"
#include "global_declarations.h"

extern WebServer server;
extern SystemConfig config;
extern VentilationState ventState;

// Forward declarations
extern bool checkAuth();
extern String getUkraineMarquee();
extern void moveServoSmooth(int targetAngle);
extern void saveConfiguration();

// ============================================================================
// СТОРІНКА КАЛІБРУВАННЯ СЕРВО
// ============================================================================

void handleServoPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("⚙ Калібрування серво");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("⚙ КАЛІБРУВАННЯ СЕРВО");

    // Додаткові стилі
    html += "<style>";
    html += ".status { background: #e3f2fd; padding: 15px; border-radius: 8px; margin: 15px 0; border-left: 4px solid #2196F3; }";
    html += ".status-item { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #e0e0e0; }";
    html += ".status-item:last-child { border-bottom: none; }";
    html += ".buttons { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin: 20px 0; }";
    html += ".btn-servo { padding: 15px; font-size: 16px; border: none; border-radius: 8px; cursor: pointer; transition: all 0.3s; font-weight: 500; }";
    html += ".btn-servo:active { transform: scale(0.95); }";
    html += ".btn-servo:disabled { opacity: 0.5; cursor: not-allowed; }";
    html += ".btn-servo:disabled:active { transform: none; }";
    html += ".btn-small { background: #2196F3; color: white; }";
    html += ".btn-small:hover { background: #1976D2; }";
    html += ".btn-big { background: #FF9800; color: white; }";
    html += ".btn-big:hover { background: #F57C00; }";
    html += ".btn-save { background: #4CAF50; color: white; grid-column: span 2; }";
    html += ".btn-save:hover { background: #388E3C; }";
    html += ".btn-test { background: #9C27B0; color: white; grid-column: span 2; }";
    html += ".btn-test:hover { background: #7B1FA2; }";
    html += ".btn-calib { background: #FF9800; color: white; grid-column: span 2; }";
    html += ".btn-calib:hover { background: #F57C00; }";
    html += ".btn-goto { padding: 15px; font-size: 16px; border: none; border-radius: 8px; cursor: pointer; transition: all 0.3s; }";
    html += ".btn-open { background: #4CAF50; color: white; }";
    html += ".btn-open:hover { background: #388E3C; }";
    html += ".btn-close { background: #f44336; color: white; }";
    html += ".btn-close:hover { background: #D32F2F; }";
    html += ".angle-display { font-size: 48px; font-weight: 700; text-align: center; color: #2196F3; margin: 20px 0; padding: 20px; background: #f5f5f5; border-radius: 10px; }";

    // Мобільна оптимізація
    html += "@media (max-width: 768px) {";
    html += "  .btn-servo { padding: 18px; font-size: 18px; }";
    html += "  .angle-display { font-size: 56px; }";
    html += "}";
    html += "@media (max-width: 480px) {";
    html += "  .buttons { gap: 8px; }";
    html += "  .btn-servo { padding: 15px 10px; font-size: 15px; }";
    html += "}";
    html += "</style>";

    // Статус
    html += "<div class='status'>";
    html += "<div class='status-item'><span><strong>Поточний кут:</strong></span><span id='current' style='font-weight: 600; color: #2196F3;'>" + String(ventState.currentAngle) + "°</span></div>";
    html += "<div class='status-item'><span><strong>Закрито:</strong></span><span id='closed'>" + String(config.servoClosedAngle) + "°</span></div>";
    html += "<div class='status-item'><span><strong>Відкрито:</strong></span><span id='open'>" + String(config.servoOpenAngle) + "°</span></div>";
    html += "<div class='status-item'><span><strong>Вимикач:</strong></span><span id='switch' style='font-weight: 600; color: ";
    html += ventState.switchState ? "#4CAF50;'>УВІМКНЕНО" : "#f44336;'>ВИМКНЕНО";
    html += "</span></div>";
    html += "<div class='status-item'><span><strong>Режим:</strong></span><span id='mode' style='font-weight: 600; color: ";
    html += ventState.calibrationMode ? "#FF9800;'>КАЛІБРУВАННЯ" : "#2196F3;'>НОРМАЛЬНИЙ";
    html += "</span></div>";
    html += "</div>";

    // Поточний кут
    html += "<div class='angle-display' id='angle'>" + String(ventState.currentAngle) + "°</div>";

    // Кнопки керування
    html += "<div class='buttons'>";

    // Режим калібрування
    html += "<button class='btn-servo btn-calib' onclick='toggleCalibration()' id='calibBtn'>";
    html += ventState.calibrationMode ? "🔓 ВИЙТИ З КАЛІБРУВАННЯ" : "🔒 УВІЙТИ В КАЛІБРУВАННЯ";
    html += "</button>";

    // Кнопки руху
    html += "<button class='btn-servo btn-small' onclick='moveServo(1)'>▲ +1°</button>";
    html += "<button class='btn-servo btn-big' onclick='moveServo(5)'>▲▲ +5°</button>";
    html += "<button class='btn-servo btn-small' onclick='moveServo(-1)'>▼ -1°</button>";
    html += "<button class='btn-servo btn-big' onclick='moveServo(-5)'>▼▼ -5°</button>";

    // Швидкі позиції
    html += "<button class='btn-servo btn-goto btn-open' onclick='gotoPosition(\"open\")'>➤ Відкрити</button>";
    html += "<button class='btn-servo btn-goto btn-close' onclick='gotoPosition(\"closed\")'>➤ Закрити</button>";

    // Збереження позицій
    html += "<button class='btn-servo btn-save' onclick='savePosition(\"closed\")'>💾 Зберегти як ЗАКРИТО</button>";
    html += "<button class='btn-servo btn-save' onclick='savePosition(\"open\")'>💾 Зберегти як ВІДКРИТО</button>";

    // Тест
    html += "<button class='btn-servo btn-test' onclick='testServo()'>🔧 ТЕСТ (відкрити → закрити)</button>";

    html += "</div>";

    html += "</div>"; // container

    // JavaScript
    html += "<script>";
    html += "function sendCommand(cmd) {";
    html += "  console.log('📤 Команда:', cmd);";
    html += "  // Блокуємо всі кнопки під час виконання";
    html += "  document.querySelectorAll('.btn-servo').forEach(btn => btn.disabled = true);";
    html += "  fetch('/servo/api', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'cmd=' + encodeURIComponent(cmd)";
    html += "  }).then(r => r.text()).then(data => {";
    html += "    console.log('📥 Відповідь:', data);";
    html += "    if(data.startsWith('ANGLE:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('angle').innerText = angle + '°';";
    html += "      document.getElementById('current').innerText = angle + '°';";
    html += "    } else if(data.startsWith('CLOSED:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('closed').innerText = angle + '°';";
    html += "      alert('✅ Закрите положення збережено: ' + angle + '°');";
    html += "    } else if(data.startsWith('OPEN:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('open').innerText = angle + '°';";
    html += "      alert('✅ Відкрите положення збережено: ' + angle + '°');";
    html += "    } else if(data == 'TEST_OK') {";
    html += "      alert('✅ Тест завершено');";
    html += "      setTimeout(() => location.reload(), 1000);";
    html += "    } else if(data.indexOf('CALIB:') === 0) {";
    html += "      console.log('CALIB response: [' + data + ']');";
    html += "      var isOn = (data.trim() === 'CALIB:ON');";
    html += "      console.log('isOn:', isOn);";
    html += "      var calibBtn = document.getElementById('calibBtn');";
    html += "      var modeSpan = document.getElementById('mode');";
    html += "      console.log('calibBtn:', calibBtn, 'modeSpan:', modeSpan);";
    html += "      if(calibBtn) { calibBtn.innerHTML = isOn ? '🔓 ВИЙТИ З КАЛІБРУВАННЯ' : '🔒 УВІЙТИ В КАЛІБРУВАННЯ'; console.log('Button updated'); }";
    html += "      if(modeSpan) { modeSpan.innerHTML = isOn ? 'КАЛІБРУВАННЯ' : 'НОРМАЛЬНИЙ'; modeSpan.style.color = isOn ? '#FF9800' : '#2196F3'; }";
    html += "      alert(isOn ? '✅ Режим калібрування УВІМКНЕНО' : '✅ Режим калібрування ВИМКНЕНО');";
    html += "    } else {";
    html += "      console.warn('⚠️ Невідома відповідь:', data);";
    html += "    }";
    html += "    // Розблоковуємо всі кнопки після отримання відповіді";
    html += "    document.querySelectorAll('.btn-servo').forEach(btn => btn.disabled = false);";
    html += "  }).catch(err => {";
    html += "    console.error('❌ Помилка:', err);";
    html += "    alert('❌ Помилка зв\\'язку: ' + err.message);";
    html += "    // Розблоковуємо кнопки при помилці";
    html += "    document.querySelectorAll('.btn-servo').forEach(btn => btn.disabled = false);";
    html += "  });";
    html += "}";
    html += "function moveServo(delta) { ";
    html += "  console.log('🎯 Рух серво:', delta);";
    html += "  sendCommand('move:' + (delta > 0 ? '+' : '') + delta); ";
    html += "}";
    html += "function savePosition(type) { sendCommand('save:' + type); }";
    html += "function gotoPosition(type) { sendCommand('goto:' + type); }";
    html += "function toggleCalibration() { ";
    html += "  sendCommand('calibration:toggle');";
    html += "}";
    html += "function testServo() {";
    html += "  if(confirm('Тест відкриє і закриє заслонку. Продовжити?')) {";
    html += "    sendCommand('test');";
    html += "  }";
    html += "}";
    html += "</script>";

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

// ============================================================================
// API СЕРВО
// ============================================================================

void handleServoAPI() {
    if (!server.hasArg("cmd")) {
        server.send(400, "text/plain", "No command");
        return;
    }

    String cmd = server.arg("cmd");
    Serial.println("WEB SERVO CMD: " + cmd);

    if (cmd.startsWith("move:")) {
        String deltaStr = cmd.substring(5);
        int delta = 0;
        if (deltaStr.length() > 0) {
            if (deltaStr[0] == '+') {
                delta = deltaStr.substring(1).toInt();
            } else if (deltaStr[0] == '-') {
                delta = -deltaStr.substring(1).toInt();
            } else {
                delta = deltaStr.toInt();
            }
        }
        int newAngle = constrain(ventState.currentAngle + delta, 0, 180);
        Serial.printf("SERVO MOVE: delta=%d, current=%d, new=%d, calibMode=%d\n", delta, ventState.currentAngle, newAngle, ventState.calibrationMode);
        ventState.moving = true;
        moveServoSmooth(newAngle);
        String response = "ANGLE:" + String(ventState.currentAngle);
        Serial.printf("SERVO RESPONSE: %s\n", response.c_str());
        server.send(200, "text/plain", response);
    }
    else if (cmd == "goto:open") {
        ventState.moving = true;  // Дозволяємо рух для API команд
        moveServoSmooth(config.servoOpenAngle);
        server.send(200, "text/plain", "ANGLE:" + String(config.servoOpenAngle));
    }
    else if (cmd == "goto:closed") {
        ventState.moving = true;  // Дозволяємо рух для API команд
        moveServoSmooth(config.servoClosedAngle);
        server.send(200, "text/plain", "ANGLE:" + String(config.servoClosedAngle));
    }
    else if (cmd == "calibration:toggle") {
        ventState.calibrationMode = !ventState.calibrationMode;

        // При виході з калібрування зберігаємо поточний стан вимикача
        // щоб controlVentilation() правильно синхронізувався
        if (!ventState.calibrationMode) {
            ventState.switchState = digitalRead(VENT_SWITCH_PIN);
            Serial.printf("Вихід з калібрування, запам'ятовано вимикач: %d\n", ventState.switchState);
        }

        String response = ventState.calibrationMode ? "CALIB:ON" : "CALIB:OFF";
        Serial.printf("Режим калібрування: %s\n", ventState.calibrationMode ? "УВІМКНЕНО" : "ВИМКНЕНО");
        server.send(200, "text/plain", response);
    }
    else if (cmd == "save:closed") {
        config.servoClosedAngle = ventState.currentAngle;
        saveConfiguration();
        Serial.printf("💾 Закрито (нижнє) = %d°\n", config.servoClosedAngle);
        server.send(200, "text/plain", "CLOSED:" + String(config.servoClosedAngle));
    }
    else if (cmd == "save:open") {
        config.servoOpenAngle = ventState.currentAngle;
        saveConfiguration();
        Serial.printf("💾 Відкрито (верхнє) = %d°\n", config.servoOpenAngle);
        server.send(200, "text/plain", "OPEN:" + String(config.servoOpenAngle));
    }
    else if (cmd == "test") {
        ventState.moving = true;
        moveServoSmooth(config.servoOpenAngle);
        ventState.moving = true;
        // Коротка затримка для прямого ефекту (не блокує вебсервер)
        delay(500);
        moveServoSmooth(config.servoClosedAngle);
        server.send(200, "text/plain", "TEST_OK");
    }
    else {
        server.send(400, "text/plain", "Unknown command");
    }
}
