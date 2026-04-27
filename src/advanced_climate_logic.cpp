#include "advanced_climate_logic.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "system_core.h"
#include "data_storage.h"
#include "global_declarations.h"
#include "google_sheets_sync.h"
#include "energy_monitor.h"
#include <Arduino.h>
#include "utility_functions.h"

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ ДЛЯ ІНТЕЛЕКТУАЛЬНОГО КЕРУВАННЯ
// ============================================================================

bool compactMode = true;
unsigned long lastModeSwitch = 0;
unsigned long lastPowerUpdate = 0;
const unsigned long POWER_UPDATE_INTERVAL = 30000;  // 30 секунд - щоб гасло встигло пробігти

// ============================================================================
// ДРУК ДЛЯ SERIAL MONITOR
// ============================================================================

void printCompactMode() {
    Serial.println("\n══════════════════════════════════════════════════════════");
    Serial.println("        РЕЖИМ ШВИДКОГО УПРАВЛІННЯ");
    Serial.println("══════════════════════════════════════════════════════════");
    Serial.println("ШВИДКІ КОМАНДИ (буква + число):");
    Serial.println("  aXX - насос XX%        (приклад: a50)");
    Serial.println("  bXX - вентилятор XX%   (приклад: b40)");
    Serial.println("  cXX - витяжка XX%      (приклад: c30, c0)");
    Serial.println("\nНАЛАШТУВАННЯ:");
    Serial.println("  tmin XX - мін. темп.   (приклад: tmin 25)");
    Serial.println("  tmax XX - макс. темп.  (приклад: tmax 26)");
    Serial.println("  hmin XX - мін. вл.     (приклад: hmin 65)");
    Serial.println("  hmax XX - макс. вл.    (приклад: hmax 70)");
    Serial.println("\nТАЙМЕРИ:");
    Serial.println("  ton XX  - вкл на XX хв (приклад: ton 30)");
    Serial.println("  toff    - вимкнути таймер");
    Serial.println("  tcycle X:Y Z:W - цикл X хв Y сек вкл, Z хв W сек викл");
    Serial.println("\nСЕЗОННІ РЕЖИМИ:");
    Serial.println("  cool   - перемкнути охолодження/обігрів");
    Serial.println("  season - увімк/вимк сезонне відключення");
    Serial.println("\nСЕРВІС:");
    Serial.println("  s      - статус системи");
    Serial.println("  m      - повне меню");
    Serial.println("  mode   - змінити режим (компактний/повний)");
    Serial.println("  reset  - скинути аварійний режим");
    Serial.println("  quiet  - вимкнути авто-вивід статусу");
    Serial.println("  verbose- увімкнути авто-вивід статусу");
    Serial.println("  web    - інформація про веб-інтерфейс");
    Serial.println("══════════════════════════════════════════════════════════\n");
}

void printExtendedMode() {
    Serial.println("\n══════════════════════════════════════════════════════════");
    Serial.println("        ПОВНИЙ РЕЖИМ КЕРУВАННЯ v" VERSION);
    Serial.println("══════════════════════════════════════════════════════════");
    Serial.println("СЕРВІСНІ КОМАНДИ:");
    Serial.println("  status      - статус системи");
    Serial.println("  menu        - показати меню");
    Serial.println("  compact     - перейти в компактний режим");
    Serial.println("  web         - інформація про веб-інтерфейс");
    Serial.println("  save        - зберегти налаштування");
    Serial.println("  reboot      - перезавантаження");    Serial.println("  quiet       - вимкнути авто-вивід статусу");
    Serial.println("  verbose     - увімкнути авто-вивід статусу");    
    Serial.println("\nУПРАВЛІННЯ ПРИЛАДАМИ:");
    Serial.println("  pump XX     - насос (0-100%)");
    Serial.println("  fan XX      - вентилятор (0-100%)");
    Serial.println("  extractor XX- витяжка (0-100%)");
    Serial.println("  auto        - автоматичний режим");
    Serial.println("  manual      - ручний режим");
    Serial.println("  force       - форсований режим");
    
    Serial.println("\nНАЛАШТУВАННЯ ТЕМПЕРАТУРИ:");
    Serial.println("  tmin XX     - мінімальна температура");
    Serial.println("  tmax XX     - максимальна температура");
    Serial.println("  temp XX     - встановити обидва границі (мін=макс)");
    
    Serial.println("\nНАЛАШТУВАННЯ ВОЛОГОСТІ:");
    Serial.println("  hmin XX     - мінімальна вологість");
    Serial.println("  hmax XX     - максимальна вологість");
    Serial.println("  hum XX      - встановити обидва границі (мін=макс)");
    
    Serial.println("\nТАЙМЕРИ:");
    Serial.println("  timer on XX - увімкнути таймер на XX хвилин");
    Serial.println("  timer off   - вимкнути таймер");
    Serial.println("  timer set XX YY - встановити таймер (вкл/викл)");
    Serial.println("  timer power XX - потужність таймера (10-100%)");
    
    Serial.println("\nРЕЖИМИ РОБОТИ:");
    Serial.println("  mode auto   - автоматичний режим");
    Serial.println("  mode manual - ручний режим");
    Serial.println("  mode compact- компактний режим Serial");
    Serial.println("  mode full   - повний режим Serial");
    
    Serial.println("\nСЕЗОННІ РЕЖИМИ:");
    Serial.println("  cooling     - перемкнути режим охолодження/обігріву");
    Serial.println("  seasonal    - увімкнути/вимкнути сезонне відключення");
    Serial.println("\nАВАРІЙНІ КОМАНДИ:");
    Serial.println("  reset       - скинути аварійний режим");
    Serial.println("\nТЕСТИ:");
    Serial.println("  test vent   - тест вентиляції");
    Serial.println("  test pump   - тест насоса (10 сек)");
    Serial.println("  test fan    - тест вентилятора (10 сек)");
    Serial.println("\nSYNC З GOOGLE SHEETS:");
    Serial.println("  sheets-sync - синхронізувати дані з Google Sheets");
    Serial.println("  sheets-stats- статистика синхронізації");
    Serial.println("  sheets-reset- скинути лічильник (відправити всі дані знову)");
    Serial.println("══════════════════════════════════════════════════════════\n");
}

void toggleMode() {
    compactMode = !compactMode;
    if (compactMode) {
        Serial.println("\n✓ Переключено в КОМПАКТНИЙ РЕЖИМ (швидке управління)");
        printCompactMode();
    } else {
        Serial.println("\n✓ Переключено в ПОВНИЙ РЕЖИМ (повні налаштування)");
        printExtendedMode();
    }
    lastModeSwitch = millis();
}

// ============================================================================
// ОБРОБКА КОМАНД У КОМПАКТНОМУ РЕЖИМІ
// ============================================================================

void processCompactCommand(String command) {
    command.toLowerCase();
    command.trim();
    
    if (command.length() < 1) return;
    
    // Команди типу "a50", "b40", "c30"
    if (command.length() >= 2 && command.length() <= 4) {
        char device = command[0];
        String valueStr = command.substring(1);
        int value = valueStr.toInt();
        
        if (value < 0 || value > 100) {
            Serial.println("✗ Помилка: значення повинно бути 0-100");
            return;
        }
        
        switch(device) {
            case 'a':
                setPumpPercent(value);
                Serial.printf("Насос: %d%%\n", value);
                break;
            case 'b':
                setFanPercent(value);
                Serial.printf("Вентилятор: %d%%\n", value);
                break;
            case 'c':
                setExtractorPercent(value);
                Serial.printf("Витяжка: %d%%\n", value);
                break;
            default:
                break;
        }
        return;
    }
    
    // Команди налаштувань
    if (command.startsWith("tmin ")) {
        float temp = command.substring(5).toFloat();
        config.tempMin = temp;
        Serial.printf("✓ Мін. температура: %.1f°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("tmax ")) {
        float temp = command.substring(5).toFloat();
        config.tempMax = temp;
        Serial.printf("✓ Макс. температура: %.1f°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("hmin ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.minHumidity = hum;
        Serial.printf("✓ Мін. вологість: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("hmax ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.maxHumidity = hum;
        Serial.printf("✓ Макс. вологість: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("ton ")) {
        int minutes = command.substring(4).toInt();
        minutes = constrain(minutes, 1, 240);
        config.extractorTimer.onMinutes = minutes;
        config.extractorTimer.enabled = true;
        config.extractorTimer.cycleStart = millis();
        Serial.printf("✓ Таймер витяжки: ВКЛ на %d хв\n", minutes);
        saveConfiguration();
    }
    else if (command == "toff") {
        config.extractorTimer.enabled = false;
        setExtractorPercent(0);
        Serial.println("✓ Таймер витяжки: ВИМК");
        saveConfiguration();
    }
    else if (command.startsWith("tcycle ")) {
        String params = command.substring(7);
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
            
            Serial.printf("✓ Таймер: %d:%02d ВКЛ / %d:%02d ВИМК\n", 
                         onMin, onSec, offMin, offSec);
            saveConfiguration();
        }
    }
    else if (command == "s") {
        autoPrintStatus();
    }
    else if (command == "m") {
        toggleMode();
    }
    else if (command == "mode") {
        toggleMode();
    }
    else if (command == "quiet") {
        config.autoStatusEnabled = false;
        saveConfiguration();
        Serial.println("✓ Автоматичний вивід статусу ВИМКНЕНО");
    }
    else if (command == "verbose") {
        config.autoStatusEnabled = true;
        saveConfiguration();
        Serial.println("✓ Автоматичний вивід статусу УВІМКНЕНО");
    }
    else if (command == "web") {
        Serial.println("\n📡 ВЕБ-ІНТЕРФЕЙС:");
        Serial.printf("  Адреса: http://%s\n", WiFi.localIP().toString().c_str());
        Serial.println("  Командна сторінка доступна на головній сторінці");
        Serial.println("  Налаштування -> /settings");
        Serial.println("  Управління -> /control");
    }
    else if (command == "cool") {
        config.coolingMode = !config.coolingMode;
        saveConfiguration();
        Serial.printf("✓ Режим охолодження: %s\n", config.coolingMode ? "УВІМК" : "ВИМК");
    }
    else if (command == "season") {
        config.seasonalHeatingDisable = !config.seasonalHeatingDisable;
        saveConfiguration();
        Serial.printf("✓ Сезонне відключення: %s\n", config.seasonalHeatingDisable ? "УВІМК" : "ВИМК");
    }
    else if (command == "reset") {
        // Скидання аварійного режиму
        powerOutageState.detected = false;
        powerOutageState.emergencyHeatingActive = false;
        heatingState.emergencyMode = false;
        heatingState.forceMode = false;
        Serial.println("✓ Аварійний режим скинуто. Повернення до AUTO режиму");
    }
    else {
        Serial.println("✗ Невідома команда. Введіть 'm' для меню");
    }
}

// ============================================================================
// ОБРОБКА КОМАНД У ПОВНОМУ РЕЖИМІ
// ============================================================================

void processExtendedCommand(String command) {
    command.toLowerCase();
    command.trim();
    
    if (command == "status" || command == "s") {
        autoPrintStatus();
    }
    else if (command == "menu" || command == "m") {
        printExtendedMode();
    }
    else if (command == "compact") {
        compactMode = true;
        Serial.println("\n✓ Переключено в КОМПАКТНИЙ РЕЖИМ");
        printCompactMode();
    }
    else if (command == "web") {
        Serial.println("\n📡 ВЕБ-ІНТЕРФЕЙС:");
        Serial.printf("  Адреса: http://%s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  Статус: /status\n");
        Serial.printf("  Управління: /control\n");
        Serial.printf("  Налаштування: /settings\n");
        Serial.printf("  Історія: /history\n");
    }
    else if (command == "save") {
        saveConfiguration();
        Serial.println("✓ Налаштування збережені");
    }
    else if (command == "quiet") {
        config.autoStatusEnabled = false;
        saveConfiguration();
        Serial.println("✓ Автоматичний вивід статусу ВИМКНЕНО");
    }
    else if (command == "verbose") {
        config.autoStatusEnabled = true;
        saveConfiguration();
        Serial.println("✓ Автоматичний вивід статусу УВІМКНЕНО");
    }
    else if (command == "servo") {
        Serial.println("\n⛔ КАЛІБРУВАННЯ СЕРВО:");
        Serial.printf("  Поточне положення: %d\u00b0\n", ventState.currentAngle);
        Serial.printf("  Закрито: %d\u00b0\n", config.servoClosedAngle);
        Serial.printf("  Відкрито: %d\u00b0\n", config.servoOpenAngle);
        Serial.println("\nКоманди:");        Serial.println("  ↑ / +      - збільшити кут на 1°");
        Serial.println("  ↓ / -      - зменшити кут на 1°");
        Serial.println("  > / ]      - збільшити кут на 5°");
        Serial.println("  < / [      - зменшити кут на 5°");        Serial.println("  servo move <кут>    - перемістити в кут (0-180)");
        Serial.println("  servo set closed - зберегти поточне як закрито");
        Serial.println("  servo set open   - зберегти поточне як відкрито");
        Serial.println("  servo test       - тест відкриття/закриття");
    }
    else if (command.startsWith("servo move ")) {
        int angle = command.substring(11).toInt();
        angle = constrain(angle, 0, 180);
        moveServoSmooth(angle);
        Serial.printf("✓ Серво переміщено в %d\u00b0\n", angle);
    }
    else if (command == "servo set closed") {
        config.servoClosedAngle = ventState.currentAngle;
        saveConfiguration();
        Serial.printf("✓ Закрите положення: %d\u00b0\n", config.servoClosedAngle);
    }
    else if (command == "servo set open") {
        config.servoOpenAngle = ventState.currentAngle;
        saveConfiguration();
        Serial.printf("✓ Відкрите положення: %d\u00b0\n", config.servoOpenAngle);
    }
    else if (command == "servo test") {
        Serial.println("🔧 Тест серво...");
        Serial.printf("  Відкриваю (%d\u00b0)...\n", config.servoOpenAngle);
        moveServoSmooth(config.servoOpenAngle);
        delay(2000);
        Serial.printf("  Закриваю (%d\u00b0)...\n", config.servoClosedAngle);
        moveServoSmooth(config.servoClosedAngle);
        Serial.println("✓ Тест завершено");
    }    else if (command == "+" || command == "up") {
        int newAngle = constrain(ventState.currentAngle + 1, 0, 180);
        moveServoSmooth(newAngle);
        Serial.printf("→ %d°\n", newAngle);
    }
    else if (command == "-" || command == "down") {
        int newAngle = constrain(ventState.currentAngle - 1, 0, 180);
        moveServoSmooth(newAngle);
        Serial.printf("→ %d°\n", newAngle);
    }
    else if (command == ">" || command == "]") {
        int newAngle = constrain(ventState.currentAngle + 5, 0, 180);
        moveServoSmooth(newAngle);
        Serial.printf("→ %d°\n", newAngle);
    }
    else if (command == "<" || command == "[") {
        int newAngle = constrain(ventState.currentAngle - 5, 0, 180);
        moveServoSmooth(newAngle);
        Serial.printf("→ %d°\n", newAngle);
    }    else if (command == "reboot") {
        Serial.println("🔄 Перезавантаження системи...");
        delay(1000);
        ESP.restart();
    }
    // Компактні команди: a100, b50, c30 (насос/вентилятор/витяжка)
    else if (command.length() >= 2 && command.length() <= 4 &&
             (command[0] == 'a' || command[0] == 'b' || command[0] == 'c')) {
        char device = command[0];
        int value = command.substring(1).toInt();
        if (value >= 0 && value <= 100) {
            heatingState.manualMode = true;
            heatingState.manualModeStartTime = millis();
            heatingState.forceMode = false;
            heatingState.emergencyMode = false;
            switch(device) {
                case 'a':
                    setPumpPercent(value);
                    Serial.printf("✓ Насос (A): %d%% [РУЧНИЙ]\n", value);
                    break;
                case 'b':
                    setFanPercent(value);
                    Serial.printf("✓ Вентилятор (B): %d%% [РУЧНИЙ]\n", value);
                    break;
                case 'c':
                    setExtractorPercent(value);
                    Serial.printf("✓ Витяжка (C): %d%% [РУЧНИЙ]\n", value);
                    break;
            }
        } else {
            Serial.println("❌ Значення має бути 0-100");
        }
    }
    else if (command.startsWith("pump ")) {
        int percent = command.substring(5).toInt();
        percent = constrain(percent, 0, 100);
        setPumpPercent(percent);
        Serial.printf("✓ Насос: %d%%\n", percent);
    }
    else if (command.startsWith("fan ")) {
        int percent = command.substring(4).toInt();
        percent = constrain(percent, 0, 100);
        setFanPercent(percent);
        Serial.printf("✓ Вентилятор: %d%%\n", percent);
    }
    else if (command.startsWith("extractor ")) {
        int percent = command.substring(10).toInt();
        percent = constrain(percent, 0, 100);
        setExtractorPercent(percent);
        Serial.printf("✓ Витяжка: %d%%\n", percent);
    }
    else if (command == "auto") {
        heatingState.manualMode = false;
        heatingState.manualModeLocked = false;
        heatingState.manualModeStartTime = 0;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        Serial.println("✓ Режим: АВТОМАТИЧНИЙ");
    }
    else if (command == "manual") {
        heatingState.manualMode = true;
        heatingState.manualModeStartTime = millis();
        heatingState.forceMode = false;
        Serial.println("✓ Режим: РУЧНИЙ (автоповернення через 15 хв)");
    }
    else if (command == "lock") {
        heatingState.manualModeLocked = !heatingState.manualModeLocked;
        Serial.printf("🔒 Блокування ручного режиму: %s\n",
                      heatingState.manualModeLocked ? "УВІМКНЕНО" : "ВИМКНЕНО");
    }
    else if (command == "lock_manual_on") {
        heatingState.manualModeLocked = true;
        Preferences prefs;
        prefs.begin("climate", false);
        prefs.putBool("manualModeLocked", true);
        prefs.end();
        Serial.println("🔒 Блокування ручного режиму: УВІМКНЕНО");
    }
    else if (command == "lock_manual_off") {
        heatingState.manualModeLocked = false;
        Preferences prefs;
        prefs.begin("climate", false);
        prefs.putBool("manualModeLocked", false);
        prefs.end();
        Serial.println("🔒 Блокування ручного режиму: ВИМКНЕНО");
    }
    else if (command == "force") {
        heatingState.forceMode = true;
        heatingState.manualMode = false;
        Serial.println("⚡ Режим: ФОРСОВАНИЙ");
    }
    else if (command.startsWith("tmin ")) {
        float temp = command.substring(5).toFloat();
        config.tempMin = temp;
        Serial.printf("✓ Мін. температура: %.1f°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("tmax ")) {
        float temp = command.substring(5).toFloat();
        config.tempMax = temp;
        Serial.printf("✓ Макс. температура: %.1f°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("temp ")) {
        float temp = command.substring(5).toFloat();
        config.tempMin = temp;
        config.tempMax = temp + 1.0f;
        Serial.printf("✓ Температура: %.1f-%.1f°C\n", temp, temp + 1.0f);
        saveConfiguration();
    }
    else if (command.startsWith("hmin ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.minHumidity = hum;
        Serial.printf("✓ Мін. вологість: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("hmax ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.maxHumidity = hum;
        Serial.printf("✓ Макс. вологість: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("hum ")) {
        float hum = command.substring(4).toFloat();
        config.humidityConfig.minHumidity = hum;
        config.humidityConfig.maxHumidity = hum + 5.0f;
        Serial.printf("✓ Вологість: %.1f-%.1f%%\n", hum, hum + 5.0f);
        saveConfiguration();
    }
    else if (command.startsWith("timer on ")) {
        int minutes = command.substring(9).toInt();
        minutes = constrain(minutes, 1, 240);
        config.extractorTimer.enabled = true;
        config.extractorTimer.onMinutes = minutes;
        config.extractorTimer.offMinutes = 0;
        config.extractorTimer.cycleStart = millis();
        Serial.printf("✓ Таймер витяжки: ВКЛ на %d хв\n", minutes);
        saveConfiguration();
    }
    else if (command == "timer off") {
        config.extractorTimer.enabled = false;
        setExtractorPercent(0);
        Serial.println("✓ Таймер витяжки: ВИМК");
        saveConfiguration();
    }
    else if (command.startsWith("timer set ")) {
        String params = command.substring(10);
        int spaceIndex = params.indexOf(' ');
        if (spaceIndex > 0) {
            int onTime = params.substring(0, spaceIndex).toInt();
            int offTime = params.substring(spaceIndex + 1).toInt();
            
            onTime = constrain(onTime, 1, 120);
            offTime = constrain(offTime, 1, 120);
            
            config.extractorTimer.onMinutes = onTime;
            config.extractorTimer.offMinutes = offTime;
            config.extractorTimer.enabled = true;
            config.extractorTimer.cycleStart = millis();
            
            Serial.printf("✓ Таймер: %d хв ВКЛ / %d хв ВИМК\n", onTime, offTime);
            saveConfiguration();
        }
    }
    else if (command.startsWith("timer power ")) {
        int power = command.substring(12).toInt();
        power = constrain(power, 10, 100);
        config.extractorTimer.powerPercent = power;
        Serial.printf("✓ Потужність таймера: %d%%\n", power);
        saveConfiguration();
    }
    else if (command.startsWith("mode ")) {
        String mode = command.substring(5);
        if (mode == "auto") {
            heatingState.manualMode = false;
            heatingState.manualModeLocked = false;
            heatingState.manualModeStartTime = 0;
            heatingState.forceMode = false;
            Serial.println("✓ Режим: АВТОМАТИЧНИЙ");
        }
        else if (mode == "manual") {
            heatingState.manualMode = true;
            heatingState.manualModeStartTime = millis();
            heatingState.forceMode = false;
            Serial.println("✓ Режим: РУЧНИЙ (автоповернення через 15 хв)");
        }
        else if (mode == "compact") {
            compactMode = true;
            Serial.println("✓ Режим Serial: КОМПАКТНИЙ");
            printCompactMode();
        }
        else if (mode == "full") {
            compactMode = false;
            Serial.println("✓ Режим Serial: ПОВНИЙ");
            printExtendedMode();
        }
    }
    else if (command == "test vent") {
        testVentilation();
    }
    else if (command == "test pump") {
        Serial.println("🔧 Тест насоса: 10 секунд на 50%");
        setPumpPercent(50);
        delay(10000);
        setPumpPercent(0);
        Serial.println("✓ Тест завершений");
    }
    else if (command == "test fan") {
        Serial.println("🔧 Тест вентилятора: 10 секунд на 50%");
        setFanPercent(50);
        delay(10000);
        setFanPercent(0);
        Serial.println("✓ Тест завершений");
    }
    else if (command == "cooling") {
        config.coolingMode = !config.coolingMode;
        saveConfiguration();
        Serial.printf("✓ Режим охолодження: %s\n", config.coolingMode ? "УВІМК" : "ВИМК");
        if (config.coolingMode) {
            Serial.println("  ❄️ Літній режим: холодна вода в теплоносії");
            Serial.println("  📈 Вентилятор збільшує швидкість при підвищенні температури");
        } else {
            Serial.println("  🔥 Зимовий режим: гаряча вода в теплоносії");
            Serial.println("  📉 Вентилятор збільшує швидкість при зниженні температури");
        }
    }
    else if (command == "seasonal") {
        config.seasonalHeatingDisable = !config.seasonalHeatingDisable;
        saveConfiguration();
        Serial.printf("✓ Сезонне відключення: %s\n", config.seasonalHeatingDisable ? "УВІМК" : "ВИМК");
        if (config.seasonalHeatingDisable) {
            Serial.println("  📅 Обігрів автоматично вимикається в теплі місяці (травень-вересень)");
        } else {
            Serial.println("  📅 Обігрів працює цілий рік");
        }
    }
    else if (command == "reset emergency" || command == "reset") {
        // Скидання аварійного режиму
        powerOutageState.detected = false;
        powerOutageState.emergencyHeatingActive = false;
        heatingState.emergencyMode = false;
        heatingState.forceMode = false;

        Serial.println("\n╔═══════════════════════════════════════════════════════╗");
        Serial.println("║  ✓ АВАРІЙНИЙ РЕЖИМ СКИНУТО                           ║");
        Serial.println("╠═══════════════════════════════════════════════════════╣");
        Serial.println("║  Всі аварійні прапорці очищено                       ║");
        Serial.println("║  Система повернулась в AUTO режим                    ║");
        Serial.println("╚═══════════════════════════════════════════════════════╝\n");
    }
    else if (command == "sheets-sync" || command == "sheets sync") {
        Serial.println("📤 Запуск синхронізації з Google Sheets...");
        if (syncToGoogleSheets()) {
            Serial.println("✅ Дані успішно відправлено в Google Sheets");
        } else {
            Serial.println("❌ Помилка синхронізації");
        }
    }
    else if (command == "sheets-stats" || command == "sheets stats") {
        printSyncInfo();
    }
    else if (command == "sheets-reset") {
        setLastSentSequence(0);
        Serial.println("✅ Sequence скинуто — наступна синхронізація відправить всі дані");
    }
    else if (command == "wifi reset" || command == "reset wifi") {
        Serial.println("\n🔄 Очищення WiFi налаштувань...");
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();
        Serial.println("✅ WiFi налаштування очищені");
        Serial.println("🔄 Перезавантаження...");
        delay(2000);
        ESP.restart();
    }
    else if (command == "format" || command == "format spiffs") {
        Serial.println("\n🔄 Форматування SPIFFS...");
        if (SPIFFS.format()) {
            Serial.println("✅ SPIFFS відформатовано");
        } else {
            Serial.println("❌ Помилка форматування SPIFFS");
        }
        Serial.println("🔄 Перезавантаження...");
        delay(2000);
        ESP.restart();
    }
    else if (command == "config reset" || command == "reset config") {
        Serial.println("\n🔄 Очищення всієї конфігурації...");
        Preferences prefs;
        prefs.begin("climate", false);
        prefs.clear();
        prefs.end();
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();
        prefs.begin("sheets_sync", false);
        prefs.clear();
        prefs.end();
        Serial.println("✅ Конфігурація очищена");
        Serial.println("🔄 Перезавантаження...");
        delay(2000);
        ESP.restart();
    }
    else {
        Serial.println("✗ Невідома команда. Введіть 'menu' для списку команд");
    }
}

// ============================================================================
// ОСНОВНА ОБРОБКА СЕРІЙНИХ КОМАНД
// ============================================================================

void processAdvancedSerialCommand() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.length() == 0) return;
    
    Serial.println("> " + command);
    
    if (compactMode) {
        processCompactCommand(command);
    } else {
        processExtendedCommand(command);
    }
}

// ============================================================================
// СЕЗОННЕ ВІДКЛЮЧЕННЯ ОБІГРІВУ
// ============================================================================

/**
 * Перевіряє чи зараз сезон опалення
 * Опалювальний сезон: жовтень (10) - квітень (4)
 * Неопалювальний сезон: травень (5) - вересень (9)
 * @return true якщо зараз сезон опалення (або режим охолодження увімкнено)
 */
bool isHeatingSeasonActive() {
    // Якщо ввімкнено режим охолодження - завжди дозволяємо роботу
    // (охолодження працює замість обігріву влітку)
    if (config.coolingMode) {
        return true;
    }

    // Якщо функція сезонного відключення вимкнена - завжди дозволяємо обігрів
    if (!config.seasonalHeatingDisable) {
        return true;
    }

    // Отримуємо поточний місяць (1-12)
    struct tm* timeInfo = getTimeInfo();
    if (timeInfo == nullptr) {
        // Якщо немає синхронізації часу - дозволяємо обігрів (безпечніше)
        return true;
    }

    int month = timeInfo->tm_mon + 1; // tm_mon: 0-11, конвертуємо в 1-12

    // Опалювальний сезон: жовтень (10), листопад (11), грудень (12),
    // січень (1), лютий (2), березень (3), квітень (4)
    if (month >= 10 || month <= 4) {
        return true; // Сезон опалення
    }

    // Травень (5), червень (6), липень (7), серпень (8), вересень (9)
    static bool seasonalDisableNotified = false;
    if (!seasonalDisableNotified) {
        Serial.println("\n╔═══════════════════════════════════════════════════════╗");
        Serial.println("║  ☀️ НЕОПАЛЮВАЛЬНИЙ СЕЗОН                              ║");
        Serial.println("╠═══════════════════════════════════════════════════════╣");
        Serial.printf("║  Поточний місяць: %d                                  ║\n", month);
        Serial.println("║  Обігрів автоматично відключено до жовтня            ║");
        Serial.println("║  Для охолодження увімкніть режим 'Cooling Mode'     ║");
        Serial.println("╚═══════════════════════════════════════════════════════╝\n");
        seasonalDisableNotified = true;
    }

    return false; // Неопалювальний сезон
}

// ============================================================================
// АДАПТИВНЕ КЕРУВАННЯ ОПАЛЕННЯМ
// ============================================================================

void smartHeatingControl() {
    if (!config.heatingEnabled) return;

    // Перевірка сезону опалення
    if (!isHeatingSeasonActive()) {
        // Неопалювальний сезон і режим охолодження вимкнено
        setPumpPercent(0);
        setFanPercent(config.fanMinPercent); // Мінімальна вентиляція
        return;
    }

    // Якщо увімкнено режим охолодження - інверсна логіка
    if (config.coolingMode) {
        smartCoolingControl();
        return;
    }

    unsigned long now = millis();

    float tempRoom = 0, tempCarrier = 0, humidity = 0;
    bool roomValid = false, carrierValid = false, bmeValid = false;
    float tempBME = 0;

    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempRoom = sensorData.tempRoom;
        tempCarrier = sensorData.tempCarrier;
        humidity = sensorData.humidity;
        roomValid = sensorData.roomValid;
        carrierValid = sensorData.carrierValid;
        bmeValid = sensorData.bmeValid;
        tempBME = sensorData.tempBME;
        xSemaphoreGive(getSensorMutex());
    }

    // Перевірка валідності датчиків кімнати
    bool roomTempAvailable = false;

    if (roomValid) {
        roomTempAvailable = true;
    } else if (bmeValid) {
        // Якщо основний датчик кімнати несправний - використовуємо BME280 як резервний
        tempRoom = getAdjustedBmeTemperature();
        roomTempAvailable = true;

        static unsigned long lastBmeWarning = 0;
        if (now - lastBmeWarning > 60000) {  // Повідомлення раз на хвилину
            Serial.println("⚠️ Використовую BME280 як резервний датчик кімнати");
            lastBmeWarning = now;
        }
    } else {
        // Обидва датчики кімнати несправні - система не може працювати
        static unsigned long lastErrorWarning = 0;
        if (now - lastErrorWarning > 10000) {  // Повідомлення раз на 10 секунд
            Serial.println("🚨 КРИТИЧНА ПОМИЛКА: ВСІ ДАТЧИКИ КІМНАТИ НЕСПРАВНІ!");
            Serial.println("   Система зупинена до відновлення датчиків");
            lastErrorWarning = now;
        }

        // Зупиняємо всі виконавчі механізми для безпеки
        setPumpPercent(0);
        setFanPercent(config.fanMinPercent);  // Мінімальна циркуляція для запобігання застою
        return;
    }

    // Перевіряємо аварійні режими (тільки в автоматичному режимі та без блокування)
    if (!heatingState.manualMode && !heatingState.manualModeLocked) {
        if (tempRoom <= config.tempEmergencyLow) {
            if (!heatingState.emergencyMode) {
                setEmergencyStartTime(millis());
                setEmergencyStartTempCarrier(tempCarrier);
                setEmergencyStartTempRoom(tempRoom);
            }

            heatingState.emergencyMode = true;
            heatingState.forceMode = false;
            setPumpPercent(100);
            setFanPercent(100);
            Serial.println("⚠ АВАРІЙНИЙ РЕЖИМ: КРИТИЧНО НИЗЬКА ТЕМПЕРАТУРА!");
            return;
        }

        if (tempRoom <= config.tempCriticalLow) {
            heatingState.forceMode = true;
            heatingState.emergencyMode = false;
            setPumpPercent(80);
            setFanPercent(80);
            Serial.println("‼ ФОРСОВАНИЙ РЕЖИМ: НИЗЬКА ТЕМПЕРАТУРА!");
            return;
        }

        // Якщо температура нормальна - скидаємо спеціальні режими
        if (heatingState.forceMode || heatingState.emergencyMode) {
            heatingState.forceMode = false;
            heatingState.emergencyMode = false;
            Serial.println("✓ Повернення до нормального АВТО режиму");
        }
    }
    
    // ШТАТНИЙ РЕЖИМ: Насос завжди вимкнений (0%), працює лише вентилятор
    // Ціль - плавно підтримувати температуру навколо середнього значення діапазону [tempMin; tempMax]

    // Насос ЗАВЖДИ 0% в штатному режимі (працює лише при аваріях)
    // ⚠️ У аварійному режимі насос залишається на 100% (не вмикаємо його на 0%)
    if (!heatingState.emergencyMode) {
      setPumpPercent(0);
    }

    // Розрахунок цільової температури (середина діапазону)
    float targetTemp = (config.tempMin + config.tempMax) / 2.0f;
    float tempDiff = targetTemp - tempRoom;  // Позитивне = потрібен обігрів
    float halfRange = (config.tempMax - config.tempMin) / 2.0f;

    // Вентилятор керує обігрівом через теплоносій - ПЛАВНА регуляція
    int fanPower;

    if (tempRoom < config.tempMin) {
        // Температура нижче мінімуму - максимальний обігрів
        fanPower = config.fanMaxPercent;
    } else if (tempRoom > config.tempMax) {
        // Температура вище максимуму - мінімальна циркуляція
        fanPower = config.fanMinPercent;
    } else {
        // Температура в межах діапазону - плавна регуляція до середнього значення
        // Чим далі від середини вниз - тим більше потужність
        // Чим далі від середини вгору - тим менше потужність

        if (halfRange > 0.1f) {
            // Нормалізуємо відхилення від середини до діапазону [-1; +1]
            // tempDiff > 0 означає tempRoom < targetTemp (потрібен обігрів)
            // tempDiff < 0 означає tempRoom > targetTemp (не потрібен обігрів)
            float normalizedDiff = tempDiff / halfRange;  // від -1 до +1
            normalizedDiff = constrain(normalizedDiff, -1.0f, 1.0f);

            // Перетворюємо в потужність вентилятора:
            // normalizedDiff = +1 (tempRoom = tempMin) -> fanMaxPercent
            // normalizedDiff =  0 (tempRoom = targetTemp) -> середня потужність
            // normalizedDiff = -1 (tempRoom = tempMax) -> fanMinPercent
            int midFan = (config.fanMinPercent + config.fanMaxPercent) / 2;
            int fanRange = (config.fanMaxPercent - config.fanMinPercent) / 2;

            fanPower = midFan + (int)(normalizedDiff * fanRange);
        } else {
            // Занадто вузький діапазон - використовуємо середню потужність
            fanPower = (config.fanMinPercent + config.fanMaxPercent) / 2;
        }
    }

    fanPower = constrain(fanPower, config.fanMinPercent, config.fanMaxPercent);
    setFanPercent(fanPower);

    if (config.autoStatusEnabled && now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
        Serial.printf("🏠 [%s] Штатний режим: T_кімн=%.1f°C (ціль %.1f°C, діапазон %.1f-%.1f°C), T_тепл=%.1f°C, Вентилятор=%d%%\n",
                     getFormattedTime().c_str(), tempRoom, targetTemp, config.tempMin, config.tempMax, tempCarrier, fanPower);
        lastPowerUpdate = now;
    }
}

// ============================================================================
// РЕЖИМ ОХОЛОДЖЕННЯ (ЛІТО: ХОЛОДНА ВОДА В ТЕПЛОНОСІЇ)
// ============================================================================

void smartCoolingControl() {
    unsigned long now = millis();

    float tempRoom = 0;
    bool roomValid = false, bmeValid = false;

    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempRoom = sensorData.tempRoom;
        roomValid = sensorData.roomValid;
        bmeValid = sensorData.bmeValid;
        xSemaphoreGive(getSensorMutex());
    }

    // Перевірка валідності датчиків кімнати
    bool roomTempAvailable = false;

    if (roomValid) {
        roomTempAvailable = true;
    } else if (bmeValid) {
        // Якщо основний датчик кімнати несправний - використовуємо BME280 як резервний
        tempRoom = getAdjustedBmeTemperature();
        roomTempAvailable = true;

        static unsigned long lastBmeWarning = 0;
        if (now - lastBmeWarning > 60000) {  // Повідомлення раз на хвилину
            Serial.println("⚠️ [ОХОЛОДЖЕННЯ] Використовую BME280 як резервний датчик");
            lastBmeWarning = now;
        }
    } else {
        // Обидва датчики кімнати несправні - система не може працювати
        static unsigned long lastErrorWarning = 0;
        if (now - lastErrorWarning > 10000) {  // Повідомлення раз на 10 секунд
            Serial.println("🚨 [ОХОЛОДЖЕННЯ] КРИТИЧНА ПОМИЛКА: ВСІ ДАТЧИКИ НЕСПРАВНІ!");
            Serial.println("   Система зупинена до відновлення датчиків");
            lastErrorWarning = now;
        }

        // Зупиняємо всі виконавчі механізми для безпеки
        setPumpPercent(0);
        setFanPercent(config.fanMinPercent);  // Мінімальна циркуляція
        return;
    }

    // РЕЖИМ ОХОЛОДЖЕННЯ: Насос завжди 0%, вентилятор регулює охолодження
    // Ціль - середнє значення між tempMin і tempMax
    float targetTemp = (config.tempMin + config.tempMax) / 2.0f;
    float tempDiff = tempRoom - targetTemp; // Інверсія: позитивне = перегрів

    // Насос ЗАВЖДИ 0% (працює зовнішня система подачі холодної води)
    setPumpPercent(0);

    // Вентилятор: чим тепліше - тим більше охолоджуємо
    int fanPower;

    if (tempDiff > 2.0f) {
        // Дуже жарко - максимальне охолодження
        fanPower = config.fanMaxPercent;
    } else if (tempRoom < config.tempMin) {
        // Жарко - активне охолодження
        fanPower = map(constrain(tempDiff * 100, 100, 200), 100, 200,
                      (config.fanMinPercent + config.fanMaxPercent) / 2, config.fanMaxPercent);
    } else if (tempRoom > config.tempMax) {
        // Злегка тепло - помірне охолодження
        fanPower = map(constrain(tempDiff * 100, 50, 100), 50, 100,
                      config.fanMinPercent + 10, (config.fanMinPercent + config.fanMaxPercent) / 2);
    } else {
        // Температура в нормі - мінімальна циркуляція
        fanPower = config.fanMinPercent;
    }

    fanPower = constrain(fanPower, config.fanMinPercent, config.fanMaxPercent);
    setFanPercent(fanPower);

    if (config.autoStatusEnabled && now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
        Serial.printf("❄️ Режим охолодження: T=%.1f°C (діапазон %.1f-%.1f°C), Насос=0%%, Вентилятор=%d%%\n",
                     tempRoom, targetTemp, fanPower);
        lastPowerUpdate = now;
    }
}

// ============================================================================
// АДАПТИВНЕ КЕРУВАННЯ ВОЛОГІСТЮ
// ============================================================================

void advancedHumidityControl(float humidity, float tempRoom) {
    if (!config.humidifierEnabled || !config.humidityConfig.enabled) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifierState.active = false;
        return;
    }
    
    unsigned long now = millis();
    
    float adaptiveHumMin, adaptiveHumMax;
    calculateAdaptiveHumidity(tempRoom, adaptiveHumMin, adaptiveHumMax);
    
    if (!humidifierState.active &&
        now - humidifierState.lastCycle < config.humidityConfig.minInterval) {
        return;
    }
    
    if (!humidifierState.active && humidity < adaptiveHumMin) {
        digitalWrite(HUMIDIFIER_PIN, HIGH);
        humidifierState.active = true;
        humidifierState.cyclesToday++;
        Serial.printf("✓ Зволожувач: ВКЛ (Вологість: %.1f%, Ціль: %.1f%)\n", 
                     humidity, adaptiveHumMin);
    } 
    else if (humidifierState.active && humidity > adaptiveHumMax) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifierState.active = false;
        humidifierState.lastCycle = now;
        Serial.printf("✓ Зволожувач: ВИМК (Вологість: %.1f%, Ціль: %.1f%)\n", 
                     humidity, adaptiveHumMax);
    }
}

// ============================================================================
// АДАПТИВНЕ УПРАВЛІННЯ ТАЙМЕРОМ
// ============================================================================

void advancedUpdateExtractorTimer() {
    if (!config.extractorTimer.enabled) {
        return;
    }
    
    unsigned long now = millis();
    
    if (config.extractorTimer.cycleStart == 0) {
        config.extractorTimer.cycleStart = now;
        config.extractorTimer.state = true;
        config.extractorTimer.lastChange = now;
        
        setExtractorPercent(config.extractorTimer.powerPercent);
        Serial.println("🔧 Таймер витяжки: Запущений, витяжка увімкнена");
    }
    
    unsigned long cycleTime = now - config.extractorTimer.cycleStart;
    
    // Перераховуємо час у мілісекундах з хвилин і секунд
    unsigned long onDuration = (config.extractorTimer.onMinutes * 60 + config.extractorTimer.onSeconds) * 1000;
    unsigned long offDuration = (config.extractorTimer.offMinutes * 60 + config.extractorTimer.offSeconds) * 1000;
    unsigned long cycleDuration = onDuration + offDuration;
    
    bool shouldBeOn = true;
    
    if (cycleDuration == 0) {
        if (cycleTime > onDuration) {
            config.extractorTimer.cycleStart = now;
            cycleTime = 0;
        }
        shouldBeOn = true;
    } else {
        if (cycleTime >= cycleDuration) {
            config.extractorTimer.cycleStart = now;
            cycleTime = 0;
        }
        
        shouldBeOn = (cycleTime < onDuration);
    }
    
    if (shouldBeOn != config.extractorTimer.state) {
        config.extractorTimer.state = shouldBeOn;
        config.extractorTimer.lastChange = now;

        if (shouldBeOn) {
            setExtractorPercent(config.extractorTimer.powerPercent);
            Serial.println("🔧 Таймер витяжки: Витяжка увімкнена");
        } else {
            setExtractorPercent(0);
            Serial.println("🔧 Таймер витяжки: Витяжка вимкнена");
        }
    } else if (shouldBeOn) {
        // Оновлюємо потужність якщо вона змінилася у налаштуваннях
        static uint8_t lastPowerPercent = 0;
        if (lastPowerPercent != config.extractorTimer.powerPercent) {
            setExtractorPercent(config.extractorTimer.powerPercent);
            lastPowerPercent = config.extractorTimer.powerPercent;
        }
    }
}

// ============================================================================
// МОНІТОРИНГ СТАНУ СИСТЕМИ
// ============================================================================

void monitorSystemHealth() {
    static unsigned long lastHealthCheck = 0;
    unsigned long now = millis();

    if (now - lastHealthCheck < 60000) return;
    lastHealthCheck = now;

    // Виводимо версію та реальний час
    Serial.println("\n╔═══════════════════════════════════════════════════════╗");
    Serial.printf("║  📋 СТАТУС СИСТЕМИ - %s                        ║\n", VERSION);
    Serial.println("╠═══════════════════════════════════════════════════════╣");
    Serial.printf("║  🕐 Час: %-44s ║\n", getFormattedTime().c_str());

    // Температури
    float tempCarrier = 0, tempRoom = 0, tempBME = 0, humidity = 0, pressure = 0;
    bool carrierValid = false, roomValid = false, bmeValid = false;

    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempCarrier = sensorData.tempCarrier;
        tempRoom = sensorData.tempRoom;
        tempBME = sensorData.tempBME;
        humidity = sensorData.humidity;
        pressure = sensorData.pressure;
        carrierValid = sensorData.carrierValid;
        roomValid = sensorData.roomValid;
        bmeValid = sensorData.bmeValid;
        xSemaphoreGive(getSensorMutex());
    }

    // Фіксована ширина для кирилиці (UTF-8 займає більше байтів)
    if (carrierValid) {
        Serial.printf("║  🌡️  Теплоносій: %-35s║\n", (String(tempCarrier, 1) + "°C").c_str());
    } else {
        Serial.println("║  🌡️  Теплоносій: 🚨 ПОМИЛКА                          ║");
    }

    if (roomValid) {
        Serial.printf("║  🏠 Кімната: %-39s║\n", (String(tempRoom, 1) + "°C").c_str());
    } else {
        Serial.println("║  🏠 Кімната: 🚨 ПОМИЛКА                              ║");
    }

    if (bmeValid) {
        Serial.printf("║  💧 Вологість: %-37s║\n", (String(humidity, 1) + "%").c_str());
    } else {
        Serial.println("║  💧 Вологість: 🚨 ПОМИЛКА                            ║");
    }

    // Режим роботи
    String mode;
    if (powerOutageState.emergencyHeatingActive) {
        mode = "🚨 НЕМАЄ 220В (" + String(powerOutageState.lastVoltage, 0) + "В)";
    } else if (heatingState.emergencyMode) {
        mode = "🚨 АВАРІЯ";
    } else if (heatingState.forceMode) {
        mode = "⚡ ФОРСАЖ";
    } else if (heatingState.manualMode) {
        mode = "✋ РУЧНИЙ";
    } else if (config.coolingMode) {
        mode = "❄️ ОХОЛОДЖЕННЯ";
    } else {
        mode = "🤖 АВТО";
    }
    Serial.printf("║  🎛️  Режим: %-38s ║\n", mode.c_str());

    // Потужності пристроїв
    Serial.printf("║  💧 Насос: %3d%% | 🌪️ Вентилятор: %3d%% | 💨 Витяжка: %3d%% ║\n",
                 (heatingState.pumpPower * 100) / 255,
                 (heatingState.fanPower * 100) / 255,
                 (heatingState.extractorPower * 100) / 255);

    Serial.println("╠═══════════════════════════════════════════════════════╣");

    // Перевірка проблем
    bool hasProblems = false;

    if (!carrierValid || !roomValid) {
        Serial.println("║  ⚠️  Проблема з датчиками температури!               ║");
        hasProblems = true;
    }

    if (!bmeValid) {
        Serial.println("║  ⚠️  Проблема з датчиком BME280!                     ║");
        hasProblems = true;
    }

    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {
        Serial.printf("║  ⚠️  Мало пам'яті: %d байт                          ║\n", freeHeap);
        hasProblems = true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("║  ⚠️  Втрачено з'єднання Wi-Fi!                       ║");
        hasProblems = true;
    }

    if (!hasProblems) {
        Serial.println("║  ✅ Всі системи працюють нормально                   ║");
    }

    // Статистика
    extern int historyIndex;
    unsigned long uptimeSeconds = now / 1000;
    unsigned long hours = uptimeSeconds / 3600;
    unsigned long minutes = (uptimeSeconds % 3600) / 60;

    Serial.println("╠═══════════════════════════════════════════════════════╣");
    Serial.printf("║  📊 Пам'ять: %dKB | Записів: %d | Uptime: %luh %luм  ║\n",
                 freeHeap / 1024, historyIndex, hours, minutes);
    Serial.println("╚═══════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ РОЗШИРЕНОЇ ЛОГІКИ
// ============================================================================

void initAdvancedLogic() {
    Serial.println("✓ Розширена логіка ініціалізована");
    
    compactMode = true;
    lastModeSwitch = millis();
    lastPowerUpdate = millis();
    
    config.extractorTimer.cycleStart = 0;
    config.extractorTimer.state = false;
    config.extractorTimer.lastChange = 0;
    
    Serial.println("✓ Новий режим Serial Monitor готовий до роботи");
    printCompactMode();
}

// ============================================================================
// МОНІТОРИНГ ВІДКЛЮЧЕННЯ ЖИВЛЕННЯ (ПО НАПРУЗІ PZEM)
// ============================================================================

void monitorPowerOutage() {
    unsigned long now = millis();

    // Перевіряємо з інтервалом
    static unsigned long lastCheck = 0;
    if (now - lastCheck < POWER_OUTAGE_CHECK_INTERVAL) {
        return;
    }
    lastCheck = now;

    // Читаємо напругу з PZEM
    EnergyMeasurements energy = getEnergyMeasurements();
    float voltage = energy.voltage;
    powerOutageState.lastVoltage = voltage;

    // Визначаємо наявність живлення: 175-255В = норма
    bool powerOk = !energy.error && (voltage >= POWER_OUTAGE_VOLTAGE_MIN) && (voltage <= POWER_OUTAGE_VOLTAGE_MAX);

    if (!powerOk && !powerOutageState.detected) {
        // Живлення зникло — входимо в аварійний режим
        if (heatingState.manualModeLocked) {
            Serial.println("🔒 Відключення 220В виявлено, але блокування ручного режиму АКТИВНО");
            return;
        }

        powerOutageState.detected = true;
        powerOutageState.detectionTime = now;
        powerOutageState.emergencyHeatingActive = true;

        Serial.println("\n╔═══════════════════════════════════════════════════════╗");
        Serial.println("║  🚨 АВАРІЯ: ВІДСУТНІСТЬ 220В!                        ║");
        Serial.println("╠═══════════════════════════════════════════════════════╣");
        Serial.printf("║  ⚡ Напруга: %.1f В (норма: %.0f-%.0f В)             ║\n",
                       voltage, POWER_OUTAGE_VOLTAGE_MIN, POWER_OUTAGE_VOLTAGE_MAX);
        Serial.println("║  🔥 Аварійний режим обігріву активовано              ║");
        Serial.println("╚═══════════════════════════════════════════════════════╝\n");

    } else if (powerOk && powerOutageState.detected) {
        // Живлення повернулось — виходимо з аварії
        powerOutageState.detected = false;
        powerOutageState.emergencyHeatingActive = false;

        Serial.println("\n╔═══════════════════════════════════════════════════════╗");
        Serial.println("║  ✅ ЖИВЛЕННЯ 220В ВІДНОВЛЕНО!                        ║");
        Serial.println("╠═══════════════════════════════════════════════════════╣");
        Serial.printf("║  ⚡ Напруга: %.1f В                                  ║\n", voltage);
        Serial.println("║  🔄 Повернення до штатного режиму                   ║");
        Serial.println("╚═══════════════════════════════════════════════════════╝\n");
    }
}


// ============================================================================
// АДАПТИВНЕ ЗНИЖЕННЯ ПОРОГІВ ТЕМПЕРАТУРИ/ВОЛОГОСТІ
// ============================================================================

/**
 * Перевіряє чи система може підтримувати задані пороги.
 * Якщо протягом тривалого часу (30 хв) температура/вологість не досягає цілі,
 * автоматично знижує пороги на один крок (adaptive_temp_step / adaptive_hum_step).
 *
 * Після відновлення нормальних умов (наприклад, повернення світла) пороги
 * автоматично підвищуються назад до оригінальних значень.
 */
void checkAdaptiveThresholds() {
    // Перевірка не частіше ніж раз на хвилину
    static unsigned long lastCheck = 0;
    unsigned long now = millis();

    if (now - lastCheck < 60000) {
        return;
    }
    lastCheck = now;

    // Отримуємо поточні значення
    float tempRoom = 0, humidity = 0;
    bool roomValid = false;

    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempRoom = sensorData.tempRoom;
        humidity = sensorData.humidity;
        roomValid = sensorData.roomValid;
        xSemaphoreGive(getSensorMutex());
    }

    if (!roomValid) {
        return; // Немає валідних даних
    }

    // Обчислюємо цільові значення (середні між min і max)
    float targetTemp = (config.tempMin + config.tempMax) / 2.0f;
    float targetHum = (config.humidityConfig.minHumidity + config.humidityConfig.maxHumidity) / 2.0f;

    // Перевіряємо чи система справляється
    float tempDeficit = targetTemp - tempRoom;
    float humDeficit = targetHum - humidity;

    // Константи для адаптивного режиму
    const float TEMP_CRITICAL_DEFICIT = 2.0f; // Якщо більше 2°C нижче цілі
    const float HUM_CRITICAL_DEFICIT = 10.0f; // Якщо більше 10% нижче цілі
    const unsigned long ADAPTIVE_ACTIVATION_TIME = 1800000; // 30 хвилин
    const unsigned long RECOVERY_CHECK_TIME = 600000; // 10 хвилин для перевірки відновлення

    // === АКТИВАЦІЯ АДАПТИВНОГО РЕЖИМУ ===
    if (!heatingState.adaptive_heating_active) {
        // Перевіряємо чи потрібно активувати адаптивний режим
        if (tempDeficit >= TEMP_CRITICAL_DEFICIT || humDeficit >= HUM_CRITICAL_DEFICIT) {
            // Проблема виявлена
            if (heatingState.adaptive_start_time == 0) {
                // Перший раз виявили проблему - запускаємо таймер
                heatingState.adaptive_start_time = now;
                Serial.println("⚠️ Система не досягає цільових порогів. Моніторинг почато...");
                if (tempDeficit >= TEMP_CRITICAL_DEFICIT)
                    Serial.printf("   Дефіцит температури: %.1f°C\n", tempDeficit);
                if (humDeficit >= HUM_CRITICAL_DEFICIT)
                    Serial.printf("   Дефіцит вологості: %.1f%%\n", humDeficit);
            } else if (now - heatingState.adaptive_start_time >= ADAPTIVE_ACTIVATION_TIME) {
                // Проблема тривала 30 хвилин - активуємо адаптивний режим
                heatingState.adaptive_heating_active = true;
                heatingState.stage_start_time = now;

                // Зберігаємо оригінальні значення
                heatingState.original_temp_target = targetTemp;
                heatingState.original_hum_target_min = config.humidityConfig.minHumidity;
                heatingState.original_hum_target_max = config.humidityConfig.maxHumidity;

                // Знижуємо пороги на один крок
                if (tempDeficit >= TEMP_CRITICAL_DEFICIT) {
                    float tempStep = config.adaptive_temp_step / 10.0f; // Конвертуємо 0.1°C у °C
                    config.tempMin = max(config.tempMin - tempStep, 18.0f); // Не нижче 18°C
                    config.tempMax = max(config.tempMax - tempStep, 19.0f); // Не нижче 19°C
                }

                if (humDeficit >= HUM_CRITICAL_DEFICIT) {
                    float humStep = config.adaptive_hum_step; // Вже в %
                    config.humidityConfig.minHumidity = max(config.humidityConfig.minHumidity - humStep, 40.0f);
                    config.humidityConfig.maxHumidity = max(config.humidityConfig.maxHumidity - humStep, 45.0f);
                }

                Serial.println("\n╔═══════════════════════════════════════════════════════╗");
                Serial.println("║  🔽 АДАПТИВНИЙ РЕЖИМ АКТИВОВАНО                      ║");
                Serial.println("╠═══════════════════════════════════════════════════════╣");
                Serial.printf("║  Нова ціль температури: %.1f-%.1f°C                  ║\n",
                             config.tempMin, config.tempMax);
                Serial.printf("║  Нова ціль вологості: %.1f-%.1f%%                     ║\n",
                             config.humidityConfig.minHumidity, config.humidityConfig.maxHumidity);
                Serial.println("║  Причина: система не може підтримувати оригінальні   ║");
                Serial.println("║  пороги через зовнішні обставини                     ║");
                Serial.println("╚═══════════════════════════════════════════════════════╝\n");

                saveConfiguration(); // Зберігаємо нові пороги
            }
        } else {
            // Проблеми немає - скидаємо таймер
            if (heatingState.adaptive_start_time != 0) {
                Serial.println("✓ Система відновила досягнення цільових порогів");
                heatingState.adaptive_start_time = 0;
            }
        }
    }
    // === ВІДНОВЛЕННЯ З АДАПТИВНОГО РЕЖИМУ ===
    else {
        // Адаптивний режим активний - перевіряємо чи можна повернутись до оригінальних порогів
        if (now - heatingState.stage_start_time >= RECOVERY_CHECK_TIME) {
            // Кожні 10 хвилин перевіряємо чи система справляється

            // Якщо температура стабільна близько до поточної цілі - пробуємо підняти пороги
            float currentTargetTemp = (config.tempMin + config.tempMax) / 2.0f;
            float currentTempDeficit = currentTargetTemp - tempRoom;

            if (currentTempDeficit < 0.5f) { // Система справляється з запасом
                // Підвищуємо пороги на один крок назад до оригіналу
                float tempStep = config.adaptive_temp_step / 10.0f;
                float humStep = config.adaptive_hum_step;

                bool tempRestored = false;
                bool humRestored = false;

                if (config.tempMin < heatingState.original_temp_target - 0.5f) {
                    config.tempMin = min(config.tempMin + tempStep, heatingState.original_temp_target - 0.5f);
                    config.tempMax = min(config.tempMax + tempStep, heatingState.original_temp_target + 0.5f);
                    Serial.printf("🔼 Підвищено пороги температури до %.1f-%.1f°C\n",
                                 config.tempMin, config.tempMax);
                } else {
                    tempRestored = true;
                }

                if (config.humidityConfig.minHumidity < heatingState.original_hum_target_min) {
                    config.humidityConfig.minHumidity = min(config.humidityConfig.minHumidity + humStep,
                                                            heatingState.original_hum_target_min);
                    config.humidityConfig.maxHumidity = min(config.humidityConfig.maxHumidity + humStep,
                                                            heatingState.original_hum_target_max);
                    Serial.printf("🔼 Підвищено пороги вологості до %.1f-%.1f%%\n",
                                 config.humidityConfig.minHumidity, config.humidityConfig.maxHumidity);
                } else {
                    humRestored = true;
                }

                // Якщо все відновлено - вимикаємо адаптивний режим
                if (tempRestored && humRestored) {
                    heatingState.adaptive_heating_active = false;
                    heatingState.adaptive_start_time = 0;

                    Serial.println("\n╔═══════════════════════════════════════════════════════╗");
                    Serial.println("║  ✅ АДАПТИВНИЙ РЕЖИМ ДЕАКТИВОВАНО                    ║");
                    Serial.println("╠═══════════════════════════════════════════════════════╣");
                    Serial.println("║  Оригінальні пороги повністю відновлені              ║");
                    Serial.println("║  Система працює в нормальному режимі                 ║");
                    Serial.println("╚═══════════════════════════════════════════════════════╝\n");
                }

                saveConfiguration();
                heatingState.stage_start_time = now;
            } else if (currentTempDeficit >= TEMP_CRITICAL_DEFICIT) {
                // Система знову не справляється - знижуємо пороги ще більше
                float tempStep = config.adaptive_temp_step / 10.0f;
                config.tempMin = max(config.tempMin - tempStep, 18.0f);
                config.tempMax = max(config.tempMax - tempStep, 19.0f);

                Serial.println("⚠️ Система все ще не справляється. Додаткове зниження порогів...");
                Serial.printf("   Нові пороги: %.1f-%.1f°C\n", config.tempMin, config.tempMax);

                saveConfiguration();
                heatingState.stage_start_time = now;
            }
        }
    }
}

/**
 * Перевіряє чи вентилятор працює на максимальній потужності тривалий час
 * без суттєвого підвищення температури. Якщо так - знижує пороги на 1°C.
 * Викликається ТІЛЬКИ коли змінюється потужність вентилятора.
 */
void checkFanMaxPowerEfficiency() {
    if (!heatingState.active) {
        // Скидаємо лічильник якщо обігрів вимкнений
        heatingState.fan_max_power_start_time = 0;
        return;
    }

    // Отримуємо поточні дані
    float tempRoom = 0;
    bool roomValid = false;

    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempRoom = sensorData.tempRoom;
        roomValid = sensorData.roomValid;
        xSemaphoreGive(getSensorMutex());
    }

    if (!roomValid) {
        return;
    }

    unsigned long now = millis();
    bool fanAtMax = (heatingState.fanPower >= config.fanMaxPercent);

    if (fanAtMax) {
        // Вентилятор на максимумі
        if (heatingState.fan_max_power_start_time == 0) {
            // Тільки що вийшов на максимум - запам'ятовуємо
            heatingState.fan_max_power_start_time = now;
            heatingState.fan_max_power_initial_temp = tempRoom;
            Serial.printf("🔥 Вентилятор на максимумі (%u%%). Моніторинг ефективності...\n", heatingState.fanPower);
        } else {
            // Перевіряємо чи пройшло 2 хвилини
            unsigned long timeAtMax = now - heatingState.fan_max_power_start_time;

            if (timeAtMax >= 120000) { // 2 хвилини = 120000 мс
                // Перевіряємо приріст температури
                float tempRise = tempRoom - heatingState.fan_max_power_initial_temp;

                Serial.printf("📊 Вентилятор на максимумі %lu сек. Приріст температури: %.2f°C\n",
                             timeAtMax / 1000, tempRise);

                // Якщо приріст менше 0.5°C за 2 хвилини - система не справляється
                if (tempRise < 0.5f) {
                    Serial.println("\n╔═══════════════════════════════════════════════════════╗");
                    Serial.println("║  ⚡ ВЕНТИЛЯТОР НА МАКСИМУМІ БЕЗ ЕФЕКТУ                ║");
                    Serial.println("╠═══════════════════════════════════════════════════════╣");
                    Serial.printf("║  Час на максимумі: %lu сек                            ║\n", timeAtMax / 1000);
                    Serial.printf("║  Приріст температури: %.2f°C (менше 0.5°C)           ║\n", tempRise);
                    Serial.println("║  Дія: зниження порогів на 1°C                        ║");

                    // Знижуємо пороги на 1°C
                    config.tempMin = max(config.tempMin - 1.0f, 18.0f);
                    config.tempMax = max(config.tempMax - 1.0f, 19.0f);

                    Serial.printf("║  Нові пороги: %.1f-%.1f°C                            ║\n",
                                 config.tempMin, config.tempMax);
                    Serial.println("╚═══════════════════════════════════════════════════════╝\n");

                    saveConfiguration();

                    // Скидаємо лічильник щоб не знижувати пороги знову відразу
                    heatingState.fan_max_power_start_time = 0;
                }
            }
        }
    } else {
        // Вентилятор не на максимумі - скидаємо лічильник
        if (heatingState.fan_max_power_start_time != 0) {
            Serial.println("✓ Вентилятор знизив потужність");
            heatingState.fan_max_power_start_time = 0;
        }
    }
}

// ============================================================================
// ОСНОВНА ЗАДАЧА РОЗШИРЕНОЇ ЛОГІКИ
// ============================================================================

void advancedLogicTask(void *parameter) {
    Serial.println("✓ Задача розширеної логіки запущена");

    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        float tempRoom = 0, tempCarrier = 0, humidity = 0;

        if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
            tempRoom = sensorData.tempRoom;
            tempCarrier = sensorData.tempCarrier;
            humidity = sensorData.humidity;
            xSemaphoreGive(getSensorMutex());
        }

        // Перевірка таймауту ручного режиму (автоповернення до авто через 15 хв)
        if (heatingState.manualMode && !heatingState.manualModeLocked) {
            const unsigned long MANUAL_MODE_TIMEOUT = 15UL * 60UL * 1000UL; // 15 хвилин

            if (heatingState.manualModeStartTime > 0 &&
                (millis() - heatingState.manualModeStartTime) > MANUAL_MODE_TIMEOUT) {
                heatingState.manualMode = false;
                heatingState.manualModeStartTime = 0;
                Serial.println("⏰ Ручний режим: автоповернення до АВТО (таймаут 15 хв)");
            }
        }

        // ЗАБЕЗПЕЧЕННЯ: Коли блокування ручного режиму активне - скидаємо аварію
        if (heatingState.manualModeLocked && powerOutageState.emergencyHeatingActive) {
            powerOutageState.emergencyHeatingActive = false;
            powerOutageState.detected = false;
            Serial.println("🔒 Блокування ручного режиму активне - зупинена аварійна логіка живлення");
        }

        // ПРІОРИТЕТ 1: Моніторинг живлення 220В (через PZEM)
        monitorPowerOutage();

        // ПРІОРИТЕТ 2: Автоматичне керування (АВТО режим включає підрежими ФОРСАЖ і АВАРІЯ)
        if (!heatingState.manualMode && !powerOutageState.emergencyHeatingActive) {
            smartHeatingControl();
        }

        // 🚨 АВАРІЙНИЙ РЕЖИМ: При відключенні 220В включаємо насос ТІЛЬКИ якщо температура низька
        if (powerOutageState.emergencyHeatingActive) {
            // Включаємо обігрів тільки якщо температура нижче мінімуму
            if (tempRoom < config.tempMin) {
                setPumpPercent(100);
                setFanPercent(100);
            } else {
                // Температура в нормі - вимикаємо системи для економії батареї
                setPumpPercent(0);
                setFanPercent(0);
            }
        }

        // ПРІОРИТЕТ 4: Адаптивне зниження порогів (якщо система не справляється)
        checkAdaptiveThresholds();

        advancedHumidityControl(humidity, tempRoom);

        advancedUpdateExtractorTimer();

        monitorSystemHealth();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}