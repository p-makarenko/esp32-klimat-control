
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
  ventState.open = (ventState.switchState == HIGH);
  ventState.currentAngle = ventState.open ? config.servoOpenAngle : config.servoClosedAngle;
  ventState.moving = false;
  ventState.servoAttached = false;
  ventState.calibrationMode = false;
  ventState.lastSwitchChange = 0;
  ventState.switchChangeCount = 0;
  ventState.autoCalibrationActive = false;
  ventState.autoCalibrationStep = 0;
  
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
  
  // Ініціалізуємо стан вимикача
  ventState.switchState = digitalRead(VENT_SWITCH_PIN);
  
  Serial.println("✓ GPIO ініціалізовано");
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
    Serial.println("⚠ БЛОКОВАНО: Серво рухається ТІЛЬКИ через механічний вимикач!");
    return;
  }
  
  if (targetAngle == ventState.currentAngle) {
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
    delay(20);
  }
  
  ventServo.write(targetAngle);
  ventState.currentAngle = targetAngle;
  ventState.open = (targetAngle == config.servoOpenAngle);
  
  delay(SERVO_DETACH_DELAY);
  ventServo.detach();
  ventState.servoAttached = false;
  ventState.moving = false;
}

void controlVentilation() {
  if (ventState.moving || ventState.calibrationMode) {
    return;  // Пропускаємо в режимі калібрування або руху
  }
  
  bool currentSwitchState = digitalRead(VENT_SWITCH_PIN);
  
  // Рухаємо серво ТІЛЬКИ якщо змінився стан вимикача
  if (currentSwitchState != ventState.switchState) {
    ventState.switchState = currentSwitchState;
    ventState.moving = true;
    
    if (currentSwitchState) {
      moveServoSmooth(config.servoOpenAngle);
      Serial.println("✓ Вентиляція відкрита (механічний вимикач)");
    } else {
      moveServoSmooth(config.servoClosedAngle);
      Serial.println("✓ Вентиляція закрита (механічний вимикач)");
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
  
  // Перевірка на максимальний час роботи
  if (humidifierState.active && 
      now - humidifierState.startTime > config.humidityConfig.maxRunTime) {
    digitalWrite(HUMIDIFIER_PIN, LOW);
    humidifierState.active = false;
    Serial.println("⚠ Зволожувач: автоматично вимкнено через максимальний час роботи");
    return;
  }
  
  // Перевірка на мінімальний інтервал
  if (!humidifierState.active && 
      now - humidifierState.lastCycle < config.humidityConfig.minInterval) {
    return;
  }
  
  // Логіка вмикання/вимикання
  if (!humidifierState.active && humidity < adaptiveHumMin) {
    digitalWrite(HUMIDIFIER_PIN, HIGH);
    humidifierState.active = true;
    humidifierState.startTime = now;
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
    if (!config.manualVentControl) {
      controlVentilation();
    }
    
    // Перевірка стану механічного вимикача
    bool newSwitchState = digitalRead(VENT_SWITCH_PIN);
    if (newSwitchState != ventState.switchState) {
      unsigned long now = millis();
      
      // Детекція швидких перемикань для автокалібрування
      if (now - ventState.lastSwitchChange < 1000) {
        ventState.switchChangeCount++;
        if (ventState.switchChangeCount >= 3 && !ventState.autoCalibrationActive) {
          startAutoCalibration();
        }
      } else {
        ventState.switchChangeCount = 0;
      }
      ventState.lastSwitchChange = now;
      
      ventState.switchState = newSwitchState;
      if (!config.manualVentControl) {
        controlVentilation();
      }
    }
    
    // Процес автокалібрування
    if (ventState.autoCalibrationActive) {
      processAutoCalibration();
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ============================================================================
// АВТОМАТИЧНЕ КАЛІБРУВАННЯ
// ============================================================================

void startAutoCalibration() {
  Serial.println("\n🔧 АВТОКАЛІБРУВАННЯ: СТАРТ");
  Serial.println("  Швидко перемикайте вимикач для зміни напряму");
  Serial.println("  Заслонка рухається до упору автоматично");
  
  ventState.autoCalibrationActive = true;
  ventState.autoCalibrationStep = 1;
  ventState.calibrationMode = true;
  
  // Рух до першого упору (закрите положення = 0°)
  Serial.println("  → Рух до ЗАКРИТОГО положення...");
  moveServoSmooth(0);
  delay(2000);  // Час для досягнення упору
  
  config.servoClosedAngle = 0;
  Serial.printf("  ✓ ЗАКРИТО = %d°\n", config.servoClosedAngle);
}

void processAutoCalibration() {
  if (!ventState.autoCalibrationActive) return;
  
  bool newSwitchState = digitalRead(VENT_SWITCH_PIN);
  
  if (ventState.autoCalibrationStep == 1) {
    // Чекаємо перемикання для руху до відкритого
    if (newSwitchState != ventState.switchState) {
      ventState.switchState = newSwitchState;
      ventState.autoCalibrationStep = 2;
      
      Serial.println("  → Рух до ВІДКРИТОГО положення...");
      moveServoSmooth(180);
      delay(2000);  // Час для досягнення упору
      
      config.servoOpenAngle = 180;
      Serial.printf("  ✓ ВІДКРИТО = %d°\n", config.servoOpenAngle);
      
      // Зберігаємо конфігурацію
      saveConfiguration();
      
      // Тест - помахати заслонкою
      ventState.autoCalibrationStep = 3;
      Serial.println("  🎉 Тест: махаємо заслонкою...");
      
      for (int i = 0; i < 3; i++) {
        moveServoSmooth(config.servoOpenAngle);
        delay(500);
        moveServoSmooth(config.servoClosedAngle);
        delay(500);
      }
      
      Serial.println("\n✅ АВТОКАЛІБРУВАННЯ ЗАВЕРШЕНО!");
      Serial.printf("  Закрито: %d°, Відкрито: %d°\n", config.servoClosedAngle, config.servoOpenAngle);
      
      // Встановлюємо в положення відповідно до вимикача
      moveServoSmooth(ventState.switchState ? config.servoOpenAngle : config.servoClosedAngle);
      
      ventState.autoCalibrationActive = false;
      ventState.calibrationMode = false;
      ventState.switchChangeCount = 0;
    }
  }
}

