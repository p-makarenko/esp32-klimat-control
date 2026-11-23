#include "actuator_manager.h"
#include "system_core.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#include "advanced_climate_logic.h"
#include "utility_functions.h"

Servo ventServo;

void initPWM() {
  ledcSetup(PUMP_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PUMP_PWM_PIN, PUMP_CHANNEL);
  ledcSetup(FAN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(FAN_PWM_PIN, FAN_CHANNEL);
  ledcSetup(EXTRACTOR_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(EXTRACTOR_PIN, EXTRACTOR_CHANNEL);

  ledcWrite(PUMP_CHANNEL, 0);
  ledcWrite(FAN_CHANNEL, 0);
  ledcWrite(EXTRACTOR_CHANNEL, 0);

  Serial.println("✓ PWM канали ініціалізовано");
}

void initServo() {
  ESP32PWM::allocateTimer(3);
  ventServo.setPeriodHertz(50);
  
  pinMode(VENT_SWITCH_PIN, INPUT_PULLUP);
  ventState.switchState = digitalRead(VENT_SWITCH_PIN);
  ventState.open = (ventState.switchState == HIGH);
  ventState.currentAngle = ventState.open ? config.servoOpenAngle : config.servoClosedAngle;
  ventState.moving = false;
  ventState.servoAttached = false;
  ventState.calibrationMode = false;
  
  ventServo.attach(SERVO_PIN);
  ventServo.write(ventState.currentAngle);
  delay(600);
  ventServo.detach();
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  
  Serial.printf("✓ Сервопривід ініціалізовано: %s (%d°)\n", 
                ventState.open ? "ВІДКРИТА" : "ЗАКРИТА", ventState.currentAngle);
}

void initGPIO() {
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);

  pinMode(VENT_SWITCH_PIN, INPUT_PULLUP);
  pinMode(HUMIDIFIER_PIN, OUTPUT);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  
  pinMode(HEATER_FAN_RELAY_PIN, OUTPUT);
  digitalWrite(HEATER_FAN_RELAY_PIN, HIGH);
  
  bool switchState = digitalRead(VENT_SWITCH_PIN);
  ventState.switchState = switchState;
  ventState.open = (switchState == HIGH);
  ventState.currentAngle = ventState.open ? config.servoOpenAngle : config.servoClosedAngle;
  ventState.moving = false;
  ventState.servoAttached = false;
  ventState.calibrationMode = false;

  Serial.printf("✓ GPIO ініціалізовано. Положення перемикача: %s (%d°)\n",
                ventState.open ? "ВІДКРИТО" : "ЗАКРИТО", ventState.currentAngle);
}

// ============================================================================
// РЕЖИМ ТИШІ (QUIET MODE) - тимчасове вимкнення шуму (вентилятори + зволожувач)
// ============================================================================
static volatile bool _quietModeEnabled = false;
static unsigned long _quietModeUntilMs = 0;

bool isQuietModeActive() {
    if (!_quietModeEnabled) return false;
    if ((long)(millis() - _quietModeUntilMs) >= 0) {
        _quietModeEnabled = false;
        Serial.println("🔊 Режим тиші завершено за таймаутом (авто-повернення)");
        return false;
    }
    return true;
}

uint16_t getQuietModeRemainingSec() {
    if (!isQuietModeActive()) return 0;
    unsigned long rem = _quietModeUntilMs - millis();
    return (uint16_t)((rem + 999) / 1000);
}

void setQuietMode(bool enable, uint16_t ttlSec) {
    if (enable) {
        if (ttlSec < 5) ttlSec = 5;
        if (ttlSec > 300) ttlSec = 300;
        _quietModeUntilMs = millis() + (unsigned long)ttlSec * 1000UL;
        bool wasActive = _quietModeEnabled;
        _quietModeEnabled = true;

        // Миттєво глушимо всі вентилятори та зволожувач
        ledcWrite(FAN_CHANNEL, 0);
        ledcWrite(EXTRACTOR_CHANNEL, 0);
        ledcWrite(PLANT_FAN_CHANNEL, 0);
        digitalWrite(HEATER_FAN_RELAY_PIN, HIGH); // HIGH = реле 220В ВИМКНЕНО
        digitalWrite(HUMIDIFIER_PIN, LOW);        // LOW = зволожувач ВИМКНЕНО
        humidifierState.active = false;

        if (xSemaphoreTake(getHeatingMutex(), pdMS_TO_TICKS(50))) {
            heatingState.fanPower = 0;
            heatingState.extractorPower = 0;
            xSemaphoreGive(getHeatingMutex());
        }

        if (!wasActive) {
            Serial.printf("🔇 Режим тиші УВІМКНЕНО (до %u сек)\n", ttlSec);
        }
    } else {
        if (_quietModeEnabled) {
            _quietModeEnabled = false;
            _quietModeUntilMs = 0;
            Serial.println("🔊 Режим тиші ВИМКНЕНО (повернення штатної роботи)");
        }
    }
}

void setHeatingPower(uint8_t pumpPercent, uint8_t fanPercent, uint8_t extractorPercent) {
    // 0. Режим тиші: примусово глушимо вентилятор обігріву та витяжку
    if (isQuietModeActive()) {
        fanPercent = 0;
        extractorPercent = 0;
    }

    // 1. Повне блокування насоса
    if (config.pumpDisabled) {
        pumpPercent = 0;
    } else if (pumpPercent > 0) {
        pumpPercent = constrain(pumpPercent, config.pumpMinPercent, config.pumpMaxPercent);
    }
    
    // 2. Повне блокування вентилятора обігріву (якщо галочка ВИМКНЕНА)
    if (!config.heaterFanRelayEnabled) {
        fanPercent = 0; // Скидаємо ШІМ у 0%
    } else if (fanPercent > 0) {
        fanPercent = constrain(fanPercent, config.fanMinPercent, config.fanMaxPercent);
    }

    // 3. Обмеження для витяжки
    if (extractorPercent > 0) {
        extractorPercent = constrain(extractorPercent, config.extractorMinPercent, config.extractorMaxPercent);
    }

    // 4. Керування реле 220В вентилятора обігріву (Pin 7) з урахуванням ЕКО-режиму
    bool allowHeaterRelay = config.heaterFanRelayEnabled &&
                            (!isEcoModeActive() || config.powerSave.ecoAllowHeaterRelay);
    if (fanPercent > 0 && allowHeaterRelay) {
        digitalWrite(HEATER_FAN_RELAY_PIN, LOW);
    } else {
        digitalWrite(HEATER_FAN_RELAY_PIN, HIGH);
    }

    // Переведення відсотків у PWM (0-255)
    uint32_t pumpDuty = map(pumpPercent, 0, 100, 0, 255);
    uint32_t fanDuty = map(fanPercent, 0, 100, 0, 255);
    uint32_t extractorDuty = map(extractorPercent, 0, 100, 0, 255);

    // Запис у PWM канали
    ledcWrite(PUMP_CHANNEL, pumpDuty);
    ledcWrite(FAN_CHANNEL, fanDuty);
    ledcWrite(EXTRACTOR_CHANNEL, extractorDuty);

    // Синхронізація стану (інакше веб показує застарілі значення)
    if (xSemaphoreTake(getHeatingMutex(), pdMS_TO_TICKS(50))) {
        heatingState.pumpPower = (uint8_t)pumpDuty;
        heatingState.fanPower = (uint8_t)fanDuty;
        heatingState.extractorPower = (uint8_t)extractorDuty;
        xSemaphoreGive(getHeatingMutex());
    }
}

// Стан неблокуючого пускового імпульсу (Kickstart) для насоса
static bool          s_pumpKickstartActive = false;
static unsigned long s_pumpKickstartEndMs  = 0;
static uint8_t       s_pumpKickstartTarget = 0;

void updatePumpKickstart() {
  if (!s_pumpKickstartActive) return;

  if (config.pumpDisabled) {
    s_pumpKickstartActive = false;
    setPumpPercent(0);
    return;
  }

  if (millis() >= s_pumpKickstartEndMs) {
    uint8_t target = s_pumpKickstartTarget;
    s_pumpKickstartActive = false;
    uint8_t pwmValue = map(target, 0, 100, 0, 255);
    if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
      heatingState.pumpPower = pwmValue;
      ledcWrite(PUMP_CHANNEL, pwmValue);
      xSemaphoreGive(getHeatingMutex());
    }
    Serial.printf("💧 Насос: пуск завершено → робоча потужність %u%%\n", target);
  }
}

void setPumpPercent(uint8_t percent) {
  // ⛔ Якщо насос повністю вимкнено у налаштуваннях — завжди 0%
  if (config.pumpDisabled) {
    s_pumpKickstartActive = false;
    if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
      heatingState.pumpPower = 0;
      ledcWrite(PUMP_CHANNEL, 0);
      xSemaphoreGive(getHeatingMutex());
    }
    return;
  }

  // Тільки в штатному автоматичному режимі застосовувати мін/макс (ФОРСАЖ 80% і АВАРІЯ 100% при 220В мають пріоритет)
  if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
    if (percent > 0 && percent < config.pumpMinPercent) {
      percent = config.pumpMinPercent;
    }
    if (percent > config.pumpMaxPercent) {
      percent = config.pumpMaxPercent;
    }
  } else {
    percent = constrain(percent, 0, 100);
  }

  // Неблокуючий пусковий імпульс (Kickstart): при старті з 0% на низькій потужності
  // спочатку подаємо pumpStartPercent (напр. 45%) на pumpStartDurationSec сек, потім знижуємо до цілі
  if (percent == 0) {
    s_pumpKickstartActive = false;
  } else if (config.pumpStartDurationSec > 0 && percent < config.pumpStartPercent) {
    uint8_t currentPower = 0;
    if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
      currentPower = heatingState.pumpPower;
      xSemaphoreGive(getHeatingMutex());
    }

    if (currentPower == 0 && !s_pumpKickstartActive) {
      // Старт з повної зупинки — запускаємо kickstart
      s_pumpKickstartActive = true;
      s_pumpKickstartTarget = percent;
      s_pumpKickstartEndMs  = millis() + (unsigned long)config.pumpStartDurationSec * 1000UL;
      Serial.printf("🚀 Насос Kickstart: %u%% на %u сек (далі → %u%%)\n",
                    config.pumpStartPercent, config.pumpStartDurationSec, percent);
      percent = config.pumpStartPercent;
    } else if (s_pumpKickstartActive) {
      if (millis() < s_pumpKickstartEndMs) {
        // Триває пусковий імпульс — оновлюємо кінцеву ціль, але тримаємо пускову потужність
        s_pumpKickstartTarget = percent;
        percent = config.pumpStartPercent;
      } else {
        // Час пуску вийшов — переходимо на цільову потужність
        s_pumpKickstartActive = false;
        Serial.printf("💧 Насос: пуск завершено → робоча потужність %u%%\n", percent);
      }
    }
  } else {
    // Ціль >= pumpStartPercent — kickstart не потрібен
    s_pumpKickstartActive = false;
  }

  uint8_t pwmValue = map(percent, 0, 100, 0, 255);

  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.pumpPower = pwmValue;
    ledcWrite(PUMP_CHANNEL, pwmValue);
    xSemaphoreGive(getHeatingMutex());
  }
}

void pumpStartupRamp(uint8_t targetPercent) {
  if (config.pumpDisabled) {
    setPumpPercent(0);
    return;
  }

  if (heatingState.manualMode) {
    setPumpPercent(targetPercent);
    return;
  }

  if (targetPercent >= 50 || targetPercent == 0) {
    setPumpPercent(targetPercent);
    return;
  }

  Serial.printf("🔄 Плавний розгон насоса до %d%%...\n", targetPercent);

  const uint8_t rampPercent = 50;
  const uint16_t rampDurationMs = 3000;
  const uint16_t stepDurationMs = 50;
  const uint8_t totalSteps = rampDurationMs / stepDurationMs;

  uint8_t currentPumpPower = 0;
  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    currentPumpPower = map(heatingState.pumpPower, 0, 255, 0, 100);
    xSemaphoreGive(getHeatingMutex());
  }

  for (uint8_t step = 0; step <= totalSteps; step++) {
    uint8_t percent = map(step, 0, totalSteps, currentPumpPower, rampPercent);

    uint8_t pwmValue = map(percent, 0, 100, 0, 255);
    if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
      heatingState.pumpPower = pwmValue;
      ledcWrite(PUMP_CHANNEL, pwmValue);
      xSemaphoreGive(getHeatingMutex());
    }

    if (step < totalSteps) {
      delay(stepDurationMs);
    }
  }

  Serial.printf("  ✓ Досягнуто 50%% після розгону\n");

  delay(1000);
  Serial.printf("  ✓ Робота на 50%% протягом 1 сек\n");

  const uint16_t downDurationMs = 1000;
  const uint8_t downSteps = downDurationMs / stepDurationMs;

  for (uint8_t step = 0; step <= downSteps; step++) {
    uint8_t percent = map(step, 0, downSteps, rampPercent, targetPercent);

    uint8_t pwmValue = map(percent, 0, 100, 0, 255);
    if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
      heatingState.pumpPower = pwmValue;
      ledcWrite(PUMP_CHANNEL, pwmValue);
      xSemaphoreGive(getHeatingMutex());
    }

    if (step < downSteps) {
      delay(stepDurationMs);
    }
  }

  Serial.printf("  ✓ Насос встановлено на %d%%\n", targetPercent);
}

void setFanPercent(uint8_t percent) {
  if (isQuietModeActive()) {
    percent = 0;
  }
  uint8_t originalPercent = percent;
  
  if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
    if (percent > 0 && percent < config.fanMinPercent) {
      percent = config.fanMinPercent;
    }
    if (percent > config.fanMaxPercent) {
      percent = config.fanMaxPercent;
    }
  } else {
    percent = constrain(percent, 0, 100);
  }
  
  uint8_t pwmValue = map(percent, 0, 100, 0, 255);
  
  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.fanPower = pwmValue;
    
    // 1. Звичайний ШІМ вентилятора обігріву (працює штатно)
    ledcWrite(FAN_CHANNEL, pwmValue);

    // 2. Реле 220В (Pin 7) — слухається своєї галочки та ЕКО-політики
    bool allowHeaterRelay = config.heaterFanRelayEnabled &&
                            (!isEcoModeActive() || config.powerSave.ecoAllowHeaterRelay);
    if (percent > 0 && allowHeaterRelay) {
      digitalWrite(HEATER_FAN_RELAY_PIN, LOW);   // Увімкнути 220В (LOW)
    } else {
      digitalWrite(HEATER_FAN_RELAY_PIN, HIGH);  // Вимкнути 220В (HIGH)
    }

    xSemaphoreGive(getHeatingMutex());
  }

  checkFanMaxPowerEfficiency();
}

void setExtractorPercent(uint8_t percent) {
  if (isQuietModeActive()) {
    percent = 0;
  }
  uint8_t originalPercent = percent;

  if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
    if (percent > 0 && percent < config.extractorMinPercent) {
      percent = config.extractorMinPercent;
    }
    if (percent > config.extractorMaxPercent) {
      percent = config.extractorMaxPercent;
    }
  } else {
    percent = constrain(percent, 0, 100);
  }

  uint8_t pwmValue = map(percent, 0, 100, 0, 255);

  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.extractorPower = pwmValue;
    ledcWrite(EXTRACTOR_CHANNEL, pwmValue);
    xSemaphoreGive(getHeatingMutex());
  }
}

void moveServoSmooth(int targetAngle) {
  if (!ventState.moving && !ventState.calibrationMode) {
    Serial.println("⚠ БЛОКОВАНО: Серво рухається ТІЛЬКИ через механічний вимикач або режим калібрування!");
    return;
  }

  for (int attempt = 0; attempt < 3; attempt++) {
    if (targetAngle == ventState.currentAngle) {
      ventState.open = (targetAngle == config.servoOpenAngle);
      ventState.moving = false;
      return;
    }

    if (!ventState.servoAttached) {
      ventServo.attach(SERVO_PIN);
      ventState.servoAttached = true;
      delay(50);
    }

    int startAngle = ventState.currentAngle;
    int step = (targetAngle > startAngle) ? 1 : -1;

    for (int angle = startAngle; angle != targetAngle; angle += step) {
      ventServo.write(angle);
      delay(config.servoSpeed);
    }

    ventServo.write(targetAngle);
    ventState.currentAngle = targetAngle;
    ventState.open = (targetAngle == config.servoOpenAngle);

    delay(SERVO_DETACH_DELAY);
    ventServo.detach();
    pinMode(SERVO_PIN, OUTPUT);
    digitalWrite(SERVO_PIN, LOW);
    ventState.servoAttached = false;
    ventState.moving = false;

    if (ventState.calibrationMode) {
      return;
    }

    bool currentSwitchState = digitalRead(VENT_SWITCH_PIN);
    if (currentSwitchState != ventState.switchState) {
      Serial.printf("⚠ УВАГА: Вимикач змінився під час руху серво! Стан: %d\n", currentSwitchState);
      ventState.switchState = currentSwitchState;
      delay(50);
      ventState.moving = true;
      targetAngle = (currentSwitchState == LOW) ? config.servoClosedAngle : config.servoOpenAngle;
      continue;
    }
    break;
  }
}

void controlVentilation() {
  if (ventState.calibrationMode) {
    return;
  }
  
  if (ventState.moving) {
    return;
  }
  
  static uint32_t lastDebounceTime = 0;
  static int lastRawState = -1;
  int rawState = digitalRead(VENT_SWITCH_PIN);
  
  if (rawState != lastRawState) {
    lastDebounceTime = millis();
    lastRawState = rawState;
  }
  
  if ((millis() - lastDebounceTime) >= 200) {
    int stableSwitchState = rawState;
    int expectedAngle = (stableSwitchState == LOW) ? config.servoClosedAngle : config.servoOpenAngle;
    bool expectedOpen = (stableSwitchState == HIGH);

    if (stableSwitchState != ventState.switchState || ventState.currentAngle != expectedAngle || ventState.open != expectedOpen) {
      Serial.printf("🔄 Синхронізація заслонки: перемикач=%s, поточний кут=%d°, цільовий=%d° (%s)\n",
                    stableSwitchState == HIGH ? "ВГОРУ(ВІДКРИТИ)" : "ВНИЗ(ЗАКРИТИ)",
                    ventState.currentAngle, expectedAngle, expectedOpen ? "ВІДКРИТО" : "ЗАКРИТО");
      ventState.switchState = stableSwitchState;
      ventState.moving = true;
      moveServoSmooth(expectedAngle);
      ventState.open = expectedOpen;
    }
  }
}

void controlHumidifier(float humidity, float tempRoom) {
  if (isQuietModeActive() ||
      !config.humidityConfig.enabled || !config.humidifierEnabled ||
      (isEcoModeActive() && config.powerSave.humidifierPolicy == ECO_DEV_OFF)) {
    digitalWrite(HUMIDIFIER_PIN, LOW);
    humidifierState.active = false;
    return;
  }
  
  float adaptiveHumMin, adaptiveHumMax;
  if (isEcoModeActive() && config.powerSave.humidifierPolicy == ECO_DEV_LIMITED) {
    adaptiveHumMin = config.powerSave.ecoHumMin;
    adaptiveHumMax = config.powerSave.ecoHumMax;
  } else {
    calculateAdaptiveHumidity(tempRoom, adaptiveHumMin, adaptiveHumMax);
  }
  
  unsigned long now = millis();
  
  if (!humidifierState.active && 
      now - humidifierState.lastCycle < config.humidityConfig.minInterval) {
    return;
  }
  
  if (!humidifierState.active && humidity < adaptiveHumMin) {
    digitalWrite(HUMIDIFIER_PIN, HIGH);
    humidifierState.active = true;
    humidifierState.cyclesToday++;
    Serial.printf("✓ Зволожувач: УВІМКНЕНО (Вологість: %.1f%%, Ціль: %.1f%%)\n", 
                  humidity, adaptiveHumMin);
  } 
  else if (humidifierState.active && humidity > adaptiveHumMax) {
    digitalWrite(HUMIDIFIER_PIN, LOW);
    humidifierState.active = false;
    humidifierState.lastCycle = now;
    Serial.printf("✓ Зволожувач: ВИМКНЕНО (Вологість: %.1f%%, Ціль: %.1f%%)\n", 
                  humidity, adaptiveHumMax);
  }
}

void heatingTask(void *parameter) {
  Serial.println("✓ Задачу обігріву запущено");
  
  while (1) {
    float tempRoom = 0, tempCarrier = 0, humidity = 0;
    
    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
      tempRoom = sensorData.tempRoom;
      tempCarrier = sensorData.tempCarrier;
      humidity = sensorData.humidity;
      xSemaphoreGive(getSensorMutex());
    }
    
    if (config.humidifierEnabled) {
      controlHumidifier(humidity, tempRoom);
    }

    // Завершення неблокуючого пускового імпульсу насоса (Kickstart)
    updatePumpKickstart();
    
    if (heatingState.emergencyMode) {
      if (getEmergencyStartTime() == 0) {
        setEmergencyStartTime(millis());
        setEmergencyStartTempCarrier(tempCarrier);
        setEmergencyStartTempRoom(tempRoom);
        Serial.println("🚨 АВАРИЙНИЙ РЕЖИМ: запущено перевірку прогріву...");
      }
      
      checkEmergencyTimeout();
    } else {
      if (getEmergencyStartTime() > 0) {
        setEmergencyStartTime(0);
        setEmergencyStartTempCarrier(0);
        setEmergencyStartTempRoom(0);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void ventilationTask(void *parameter) {
  Serial.println("✓ Задачу вентиляції запущено");

  while (1) {
    if (!ventState.calibrationMode && !config.manualVentControl) {
      controlVentilation();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}