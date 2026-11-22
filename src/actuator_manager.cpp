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

void setHeatingPower(uint8_t pumpPercent, uint8_t fanPercent, uint8_t extractorPercent) {
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

    // 4. Керування реле 220В вентилятора обігріву (Pin 7)
    if (fanPercent > 0 && config.heaterFanRelayEnabled) {
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
}void setPumpPercent(uint8_t percent) {
  // ⛔ Якщо насос повністю вимкнено у налаштуваннях — завжди 0%
  if (config.pumpDisabled) {
    if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
      heatingState.pumpPower = 0;
      ledcWrite(PUMP_CHANNEL, 0);
      xSemaphoreGive(getHeatingMutex());
    }
    return;
  }

  uint8_t originalPercent = percent;

  // Тільки в автоматичному режимі застосовувати мінімум/максимум з налаштувань
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

  uint8_t pwmValue = map(percent, 0, 100, 0, 255);

  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.pumpPower = pwmValue;
    ledcWrite(PUMP_CHANNEL, pwmValue);
    xSemaphoreGive(getHeatingMutex());
  }
}

void pumpStartupRamp(uint8_t targetPercent) {
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

    // 2. Реле 220В (Pin 7) — слухається ТІЛЬКИ своєї галочки
    if (percent > 0 && config.heaterFanRelayEnabled) {
      digitalWrite(HEATER_FAN_RELAY_PIN, LOW);   // Увімкнути 220В (LOW)
    } else {
      digitalWrite(HEATER_FAN_RELAY_PIN, HIGH);  // Вимкнути 220В (HIGH)
    }

    xSemaphoreGive(getHeatingMutex());
  }

  checkFanMaxPowerEfficiency();
}

void setExtractorPercent(uint8_t percent) {
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
    int newTargetAngle = (currentSwitchState == LOW) ? config.servoClosedAngle : config.servoOpenAngle;
    moveServoSmooth(newTargetAngle);
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
  if (!config.humidityConfig.enabled || !config.humidifierEnabled) {
    digitalWrite(HUMIDIFIER_PIN, LOW);
    humidifierState.active = false;
    return;
  }
  
  float adaptiveHumMin, adaptiveHumMax;
  calculateAdaptiveHumidity(tempRoom, adaptiveHumMin, adaptiveHumMax);
  
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