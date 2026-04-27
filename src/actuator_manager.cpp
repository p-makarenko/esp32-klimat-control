
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
  ESP32PWM::allocateTimer(3);  // Виділяємо таймер 3 для серво, щоб не конфліктувати з PWM каналами 0,1,2
  ventServo.setPeriodHertz(50);
  
  ventState.switchState = digitalRead(VENT_SWITCH_PIN);
  // LOW = ВНИЗ = ЗАКРИТО, HIGH = ВГОРУ = ВІДКРИТО
  ventState.open = (ventState.switchState == HIGH);
  ventState.currentAngle = ventState.open ? config.servoOpenAngle : config.servoClosedAngle;
  ventState.moving = false;
  ventState.servoAttached = false;
  ventState.calibrationMode = false;
  
  ventServo.attach(SERVO_PIN);
  ventServo.write(ventState.currentAngle);
  delay(1000);
  ventServo.detach();
  
  Serial.printf("✓ Сервопривід ініціалізовано: %s (%d°)\n", 
                ventState.open ? "ВІДКРИТА" : "ЗАКРИТА", ventState.currentAngle);
}

void initGPIO() {
  pinMode(VENT_SWITCH_PIN, INPUT_PULLUP);
  pinMode(HUMIDIFIER_PIN, OUTPUT);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  
  // Читаємо стан вимикача
  bool switchState = digitalRead(VENT_SWITCH_PIN);
  ventState.switchState = switchState;

  // Встановлюємо серво у відповідне положення при старті
  // LOW = ВНИЗ = ЗАКРИТО, HIGH = ВГОРУ = ВІДКРИТО
  ventState.moving = true;
  int targetAngle = (switchState == LOW) ? config.servoClosedAngle : config.servoOpenAngle;
  
  if (!ventState.servoAttached) {
    ventServo.attach(SERVO_PIN);
    ventState.servoAttached = true;
    delay(50);
  }
  
  ventServo.write(targetAngle);
  ventState.currentAngle = targetAngle;
  ventState.open = (switchState == HIGH);

  delay(500);  // Даємо час серво досягнути позиції
  ventServo.detach();
  ventState.servoAttached = false;
  ventState.moving = false;

  Serial.printf("✓ GPIO ініціалізовано. Серво: %s (%d°)\n",
                ventState.open ? "ВІДКРИТО" : "ЗАКРИТО", targetAngle);
}

void setHeatingPower(uint8_t pumpPower, uint8_t fanPower, uint8_t extractorPower) {
  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    // ВСЕ мінімуми видаляємо з цієї функції - вони вже застосовані у setXPercent()
    heatingState.pumpPower = pumpPower;
    heatingState.fanPower = fanPower;
    heatingState.extractorPower = extractorPower;
    
    ledcWrite(PUMP_CHANNEL, pumpPower);
    ledcWrite(FAN_CHANNEL, fanPower);
    ledcWrite(EXTRACTOR_CHANNEL, extractorPower);
    
    xSemaphoreGive(getHeatingMutex());
  }
}

void setPumpPercent(uint8_t percent) {
  uint8_t originalPercent = percent;

  // Тільки в автоматичному режимі застосовувати мінімум/максимум з налаштувань
  if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
    // В авто режимі: мінімум з налаштувань при НЕНУЛЬОВОМУ значенні
    if (percent > 0 && percent < config.pumpMinPercent) {
      percent = config.pumpMinPercent;
    }
    // Максимум з налаштувань
    if (percent > config.pumpMaxPercent) {
      percent = config.pumpMaxPercent;
    }
  } else {
    // У ручному/форсажному/аварійному режимі: БУДЬ-ЯКЕ значення, включаючи 0%
    percent = constrain(percent, 0, 100);
  }

  uint8_t pwmValue = map(percent, 0, 100, 0, 255);

  // Serial.printf("[DEBUG] setPumpPercent: %d%% -> %d%% -> PWM=%d\n", originalPercent, percent, pwmValue);

  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.pumpPower = pwmValue;
    ledcWrite(PUMP_CHANNEL, pwmValue);
    xSemaphoreGive(getHeatingMutex());
  }
}

void pumpStartupRamp(uint8_t targetPercent) {
  // У ручному режимі - без плавного розгону, просто встановлюємо
  if (heatingState.manualMode) {
    setPumpPercent(targetPercent);
    return;
  }

  // Якщо ціль >= 50%, плавний розгон не потрібен
  if (targetPercent >= 50 || targetPercent == 0) {
    setPumpPercent(targetPercent);
    return;
  }

  // АВТОМАТИЧНИЙ РЕЖИМ: Для цілей < 50% застосовуємо плавний розгон через 50%
  Serial.printf("🔄 Плавний розгон насоса до %d%%...\n", targetPercent);

  // Етап 1: Розгон на 50% за 3 секунди (плавно)
  const uint8_t rampPercent = 50;
  const uint16_t rampDurationMs = 3000;
  const uint16_t stepDurationMs = 50;
  const uint8_t totalSteps = rampDurationMs / stepDurationMs;

  // Отримуємо поточний стан насоса
  uint8_t currentPumpPower = 0;
  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    currentPumpPower = map(heatingState.pumpPower, 0, 255, 0, 100);
    xSemaphoreGive(getHeatingMutex());
  }

  // Розгін з поточного стану до 50%
  for (uint8_t step = 0; step <= totalSteps; step++) {
    uint8_t percent = map(step, 0, totalSteps, currentPumpPower, rampPercent);

    // Прямий запис PWM без виклику setPumpPercent (уникаємо рекурсії мінімумів)
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

  // Етап 2: Робота на 50% протягом 1 секунди
  delay(1000);
  Serial.printf("  ✓ Робота на 50%% протягом 1 сек\n");

  // Етап 3: Плавне зниження до цільового рівня за 1 секунду
  const uint16_t downDurationMs = 1000;
  const uint8_t downSteps = downDurationMs / stepDurationMs;

  for (uint8_t step = 0; step <= downSteps; step++) {
    uint8_t percent = map(step, 0, downSteps, rampPercent, targetPercent);

    // Прямий запис PWM
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
  
  // Тільки в автоматичному режимі застосовувати мінімум/максимум з налаштувань
  if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
    // В авто режимі: мінімум з налаштувань при НЕНУЛЬОВОМУ значенні
    if (percent > 0 && percent < config.fanMinPercent) {
      percent = config.fanMinPercent;
    }
    // Максимум з налаштувань
    if (percent > config.fanMaxPercent) {
      percent = config.fanMaxPercent;
    }
  } else {
    // У ручному/форсажному/аварійному режимі: БУДЬ-ЯКЕ значення, включаючи 0%
    percent = constrain(percent, 0, 100);
  }
  
  uint8_t pwmValue = map(percent, 0, 100, 0, 255);
  
  // Serial.printf("[DEBUG] setFanPercent: %d%% -> %d%% -> PWM=%d\n", originalPercent, percent, pwmValue);
  
  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.fanPower = pwmValue;
    ledcWrite(FAN_CHANNEL, pwmValue);
    xSemaphoreGive(getHeatingMutex());
  }

  // Перевіряємо ефективність вентилятора після зміни потужності
  checkFanMaxPowerEfficiency();
}

// Валідація температури перед активацією насоса
bool isPumpActivationAllowed(float currentTemp) {
  // Насос включається тільки якщо температура вище мінімальної безпечної
  const float MIN_SAFE_TEMP = 5.0;  // 5°C - мінімум для циркуляції

  if (currentTemp < MIN_SAFE_TEMP) {
    Serial.printf("⚠ БЛОКУВАННЯ: Температура занадто низька (%.1f°C < %f°C)\n",
                  currentTemp, MIN_SAFE_TEMP);
    return false;
  }

  // У аварійному режимі (<18°C) дозволяємо включення незалежно від мінімуму
  if (currentTemp < 18.0 && heatingState.emergencyMode) {
    return true;
  }

  return true;
}

void setExtractorPercent(uint8_t percent) {
  uint8_t originalPercent = percent;

  // Тільки в автоматичному режимі застосовувати мінімум/максимум з налаштувань
  if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
    // В авто режимі: мінімум з налаштувань при НЕНУЛЬОВОМУ значенні
    if (percent > 0 && percent < config.extractorMinPercent) {
      percent = config.extractorMinPercent;
    }
    // Максимум з налаштувань
    if (percent > config.extractorMaxPercent) {
      percent = config.extractorMaxPercent;
    }
  } else {
    // У ручному/форсажному/аварійному режимі: БУДЬ-ЯКЕ значення, включаючи 0%
    percent = constrain(percent, 0, 100);
  }

  uint8_t pwmValue = map(percent, 0, 100, 0, 255);

  // Serial.printf("[DEBUG] setExtractorPercent: %d%% -> %d%% -> PWM=%d\n", originalPercent, percent, pwmValue);

  if (xSemaphoreTake(getHeatingMutex(), portMAX_DELAY)) {
    heatingState.extractorPower = pwmValue;
    ledcWrite(EXTRACTOR_CHANNEL, pwmValue);
    xSemaphoreGive(getHeatingMutex());
  }
}

void moveServoSmooth(int targetAngle) {
  // КРИТИЧНИЙ ЗАХИСТ: Серво рухається ТІЛЬКИ через механічний вимикач!
  // Виняток: режим калібрування (для налаштування кутів)
  if (!ventState.moving && !ventState.calibrationMode) {
    Serial.println("⚠ БЛОКОВАНО: Серво рухається ТІЛЬКИ через механічний вимикач або режим калібрування!");
    return;
  }

  // Дозволяємо рух у режимі калібрування БЕЗ механічного вимикача
  // Дозволяємо рух при вмиканні флага moving (через вентиляційний вимикач)
  // Обовʼязково одне з цих умов має бути істинною
  
  if (targetAngle == ventState.currentAngle) {
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
  ventState.servoAttached = false;
  ventState.moving = false;

  // В режимі калібрування НЕ перевіряємо вимикач - користувач керує вручну
  if (ventState.calibrationMode) {
    return;
  }

  // Перевіряємо чи не змінився вимикач під час руху (тільки НЕ в режимі калібрування)
  bool currentSwitchState = digitalRead(VENT_SWITCH_PIN);
  if (currentSwitchState != ventState.switchState) {
    Serial.printf("⚠ УВАГА: Вимикач змінився під час руху серво! Стан: %d\n", currentSwitchState);
    ventState.switchState = currentSwitchState;
    delay(100);
    ventState.moving = true;
    int newTargetAngle = (currentSwitchState == LOW) ? config.servoOpenAngle : config.servoClosedAngle;
    moveServoSmooth(newTargetAngle);
  }
}

void controlVentilation() {
  if (ventState.calibrationMode) {
    return;  // Пропускаємо в режимі калібрування
  }
  
  // Якщо серво вже рухається, чекаємо завершення
  if (ventState.moving) {
    return;
  }
  
  bool currentSwitchState = digitalRead(VENT_SWITCH_PIN);
  
  // Рухаємо серво ТІЛЬКИ якщо змінився стан вимикача
  // LOW = вимикач ВНИЗ = ЗАКРИТО
  // HIGH = вимикач ВГОРУ = ВІДКРИТО
  if (currentSwitchState != ventState.switchState) {
    Serial.printf("Перемикач змінено: %d -> %d\n", ventState.switchState, currentSwitchState);
    ventState.switchState = currentSwitchState;
    ventState.moving = true;

    if (currentSwitchState == LOW) {
      moveServoSmooth(config.servoClosedAngle);
      Serial.println("✓ Вентиляція ЗАКРИТА (вимикач ВНИЗ)");
    } else {
      moveServoSmooth(config.servoOpenAngle);
      Serial.println("✓ Вентиляція ВІДКРИТА (вимикач ВГОРУ)");
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
  
  // Перевірка на мінімальний інтервал
  if (!humidifierState.active && 
      now - humidifierState.lastCycle < config.humidityConfig.minInterval) {
    return;
  }
  
  // Логіка вмикання/вимикання
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
    // В режимі калібрування пропускаємо обробку вимикача
    if (!ventState.calibrationMode && !config.manualVentControl) {
      controlVentilation();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


