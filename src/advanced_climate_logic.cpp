#include "advanced_climate_logic.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "system_core.h"
#include "data_storage.h"
#include <Arduino.h>
#include "utility_functions.h"

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ ДЛЯ ІНТЕЛЕКТУАЛЬНОГО КЕРУВАННЯ
// ============================================================================

bool compactMode = true;
unsigned long lastModeSwitch = 0;
unsigned long lastPowerUpdate = 0;
const unsigned long POWER_UPDATE_INTERVAL = 1000;

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
    Serial.println("  tcycle X Y - цикл X хв вкл, Y хв викл");
    Serial.println("\nСЕРВІС:");
    Serial.println("  s      - статус системи");
    Serial.println("  m      - повне меню");
    Serial.println("  mode   - змінити режим (компактний/повний)");
    Serial.println("  quiet  - вимкнути авто-вивід статусу");
    Serial.println("  verbose- увімкнути авто-вивід статусу");
    Serial.println("  web    - інформація про веб-інтерфейс");
    Serial.println("══════════════════════════════════════════════════════════\n");
}

void printExtendedMode() {
    Serial.println("\n══════════════════════════════════════════════════════════");
    Serial.println("        ПОВНИЙ РЕЖИМ КЕРУВАННЯ v4.0");
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
    
    Serial.println("\nТЕСТИ:");
    Serial.println("  test vent   - тест вентиляції");
    Serial.println("  test pump   - тест насоса (10 сек)");
    Serial.println("  test fan    - тест вентилятора (10 сек)");
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
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        Serial.println("✓ Режим: АВТОМАТИЧНИЙ");
    }
    else if (command == "manual") {
        heatingState.manualMode = true;
        heatingState.forceMode = false;
        Serial.println("✓ Режим: РУЧНИЙ");
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
            heatingState.forceMode = false;
            Serial.println("✓ Режим: АВТОМАТИЧНИЙ");
        }
        else if (mode == "manual") {
            heatingState.manualMode = true;
            heatingState.forceMode = false;
            Serial.println("✓ Режим: РУЧНИЙ");
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
// АДАПТИВНЕ КЕРУВАННЯ ОПАЛЕННЯМ
// ============================================================================

void smartHeatingControl() {
    if (!config.heatingEnabled) return;
    
    unsigned long now = millis();
    
    float tempRoom = 0, tempCarrier = 0, humidity = 0;
    
    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempRoom = sensorData.tempRoom;
        tempCarrier = sensorData.tempCarrier;
        humidity = sensorData.humidity;
        xSemaphoreGive(getSensorMutex());
    }
    
    // Перевіряємо аварійні режими
    if (tempRoom <= TEMP_EMERGENCY_LOW) {
        if (!heatingState.emergencyMode) {
            setEmergencyStartTime(millis());
            setEmergencyStartTempCarrier(tempCarrier);
            setEmergencyStartTempRoom(tempRoom);
        }
        
        heatingState.emergencyMode = true;
        setPumpPercent(100);
        setFanPercent(100);
        Serial.println("⚠ АВАРІЙНИЙ РЕЖИМ: КРИТИЧНО НИЗЬКА ТЕМПЕРАТУРА!");
        return;
    }
    
    if (tempRoom <= TEMP_CRITICAL_LOW) {
        heatingState.forceMode = true;
        setPumpPercent(80);
        setFanPercent(80);
        Serial.println("‼ ФОРСОВАНИЙ РЕЖИМ: НИЗЬКА ТЕМПЕРАТУРА!");
        return;
    }
    
    // Адаптивне управління обігрівом
    if (tempRoom < config.tempMin) {
        float tempDiff = config.tempMin - tempRoom;
        int pumpPower = map(constrain(tempDiff * 10, 0, 20), 0, 20, config.pumpMinPercent, config.pumpMaxPercent);
        int fanPower = map(constrain(tempDiff * 10, 0, 20), 0, 20, config.fanMinPercent, config.fanMaxPercent);
        
        setPumpPercent(pumpPower);
        setFanPercent(fanPower);
        
        if (config.autoStatusEnabled && now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
            Serial.printf("⚡ Обігрів: T=%.1f°C, Насос=%d%, Вентилятор=%d%\n", 
                         tempRoom, pumpPower, fanPower);
            lastPowerUpdate = now;
        }
    }
    else if (tempRoom > config.tempMax) {
        setPumpPercent(0);
        setFanPercent(config.fanMinPercent);
        
        if (config.autoStatusEnabled && now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
            Serial.printf("❄ Охолодження: T=%.1f°C, Насос=0%, Вентилятор=%d%\n", 
                         tempRoom, config.fanMinPercent);
            lastPowerUpdate = now;
        }
    }
    else {
        float tempRange = config.tempMax - config.tempMin;
        float tempPosition = (tempRoom - config.tempMin) / tempRange;
        
        int pumpPower = map(tempPosition * 100, 0, 100, config.pumpMinPercent, config.pumpMinPercent / 2);
        int fanPower = map(tempPosition * 100, 0, 100, 40, config.fanMinPercent);
        
        pumpPower = constrain(pumpPower, config.pumpMinPercent / 2, config.pumpMinPercent);
        fanPower = constrain(fanPower, config.fanMinPercent, 40);
        
        setPumpPercent(pumpPower);
        setFanPercent(fanPower);
        
        if (config.autoStatusEnabled && now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
            Serial.printf("📊 Підтримання: T=%.1f°C, Насос=%d%, Вентилятор=%d%\n", 
                         tempRoom, pumpPower, fanPower);
            lastPowerUpdate = now;
        }
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
    
    if (humidifierState.active && 
        now - humidifierState.startTime > config.humidityConfig.maxRunTime) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifierState.active = false;
        Serial.println("⚠ Зволожувач: автоматично вимкнений через максимальний час роботи");
        return;
    }
    
    if (!humidifierState.active && 
        now - humidifierState.lastCycle < config.humidityConfig.minInterval) {
        return;
    }
    
    if (!humidifierState.active && humidity < adaptiveHumMin) {
        digitalWrite(HUMIDIFIER_PIN, HIGH);
        humidifierState.active = true;
        humidifierState.startTime = now;
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
    unsigned long cycleDuration = (config.extractorTimer.onMinutes + 
                                  config.extractorTimer.offMinutes) * 60000;
    
    bool shouldBeOn = true;
    
    if (cycleDuration == 0) {
        if (cycleTime > config.extractorTimer.onMinutes * 60000) {
            config.extractorTimer.cycleStart = now;
            cycleTime = 0;
        }
        shouldBeOn = true;
    } else {
        if (cycleTime >= cycleDuration) {
            config.extractorTimer.cycleStart = now;
            cycleTime = 0;
        }
        
        shouldBeOn = (cycleTime < (config.extractorTimer.onMinutes * 60000));
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
    
    if (!sensorData.carrierValid || !sensorData.roomValid) {
        Serial.println("⚠ Проблема: Проблема з датчиками температури!");
    }
    
    if (!sensorData.bmeValid) {
        Serial.println("⚠ Проблема: Проблема з датчиком BME280!");
    }
    
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {
        Serial.printf("⚠ Проблема: Мало вільної пам'яті: %d байт\n", freeHeap);
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠ Проблема: Втрачено з'єднання Wi-Fi!");
    }
    
    extern int historyIndex;
    Serial.printf("📈 Статистика: Пам'ять=%dKB, Записів=%d, Час=%luсек\n",
                 freeHeap / 1024, historyIndex, now / 1000);
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
        
        if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
            smartHeatingControl();
        }
        
        advancedHumidityControl(humidity, tempRoom);
        
        advancedUpdateExtractorTimer();
        
        monitorSystemHealth();
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}