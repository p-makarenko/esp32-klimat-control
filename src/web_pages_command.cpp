// ============================================================================
// WEB_PAGES_COMMAND.CPP - Обробка команд із веб-інтерфейсу
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Функції: handleWebCommand(), processWebCommand()
// ============================================================================

#include "web_interface.h"
#include "web_common.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "global_declarations.h"
#include "google_sheets_sync.h"
#include <WiFi.h>
#include <WebServer.h>

// Forward declarations
extern bool checkAuth();
extern bool checkCSRF();
extern void testVentilation();
extern void moveServoSmooth(int angle);

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

// ============================================================================
// ПРОЦЕСОР КОМАНД
// ============================================================================

String processWebCommand(const String& cmd) {
    String lowerCmd = cmd;
    lowerCmd.toLowerCase();

    // ========================================================================
    // РЕЖИМИ РОБОТИ (перевіряємо спочатку, щоб "auto" не розпізнавався як "a0")
    // ========================================================================

    if (lowerCmd == "auto") {
        heatingState.manualMode = false;
        heatingState.manualModeLocked = false;
        heatingState.manualModeStartTime = 0;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        // Скидаємо стан аварії при переході в AUTO
        powerOutageState.detected = false;
        powerOutageState.emergencyHeatingActive = false;
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
        heatingState.emergencyMode = false;
        heatingState.forceMode = false;
        heatingState.manualMode = false;
        return "✅ Аварійний режим скинуто. Повернення до AUTO";
    }

    // ========================================================================
    // КОМПАКТНІ КОМАНДИ (a50, b40, c30)
    // ========================================================================

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

    // ========================================================================
    // КЕРУВАННЯ ПРИСТРОЯМИ
    // ========================================================================

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

    // ========================================================================
    // ТАЙМЕР ВИТЯЖКИ
    // ========================================================================

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

    // ========================================================================
    // НАЛАШТУВАННЯ ТЕМПЕРАТУРИ ТА ВОЛОГОСТІ
    // ========================================================================

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

    // ========================================================================
    // СИСТЕМНІ КОМАНДИ
    // ========================================================================

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
    else if (lowerCmd == "web") {
        return "🌐 Веб-інтерфейс: http://" + WiFi.localIP().toString();
    }

    // ========================================================================
    // ТЕСТУВАННЯ
    // ========================================================================

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

    // ========================================================================
    // СЕРВО
    // ========================================================================

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
        return "⚙️ Серво: поточне=" + String(ventState.currentAngle) + "° закрито=" +
               String(config.servoClosedAngle) + "° відкрито=" + String(config.servoOpenAngle) + "°";
    }

    // ========================================================================
    // MODE КОМАНДА
    // ========================================================================

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

    // ========================================================================
    // GOOGLE SHEETS
    // ========================================================================

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

    // ========================================================================
    // СКИДАННЯ WIFI
    // ========================================================================

    else if (lowerCmd == "wifi reset" || lowerCmd == "reset wifi") {
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();
        Serial.println("🔄 WiFi налаштування очищені. Перезавантаження...");
        delay(2000);
        ESP.restart();
        return "✅ WiFi скинуто";
    }

    // ========================================================================
    // НЕВІДОМА КОМАНДА
    // ========================================================================

    else {
        return "❌ Невідома команда: " + cmd + "\n📋 Доступні команди:\n"
               "pump/fan/extractor XX, timer on/off, tmin/tmax/temp/hmin/hmax/hum XX,\n"
               "auto/manual/force, servo, mode, status, save, quiet, verbose, test, web, reboot,\n"
               "sheets-sync, sheets-stats, wifi reset";
    }
}
