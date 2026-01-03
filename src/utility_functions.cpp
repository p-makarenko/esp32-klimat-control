#include "system_core.h"
#include "actuator_manager.h"
#include "advanced_climate_logic.h"
#include "sensor_manager.h"
#include "data_storage.h"
#include <Arduino.h>

// Оголосити зовнішні змінні
extern int historyIndex;
extern bool historyInitialized;

// ============================================================================
// АВТОМАТИЧНИЙ ВИВІД СТАТУСУ
// ============================================================================

void autoPrintStatus() {
    if (!config.autoStatusEnabled) {
        return;  // Вивід вимкнено
    }
    
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    
    if (now - lastPrint < config.statusPeriod) {
        return;
    }
    
    lastPrint = now;
    
    // Отримуємо дані
    float tempRoom = 0, tempCarrier = 0, humidity = 0, pressure = 0;
    uint8_t pumpPower = 0, fanPower = 0, extractorPower = 0;
    
    if (xSemaphoreTake(getSensorMutex(), pdMS_TO_TICKS(100))) {
        tempRoom = sensorData.tempRoom;
        tempCarrier = sensorData.tempCarrier;
        humidity = sensorData.humidity;
        pressure = sensorData.pressure;
        xSemaphoreGive(getSensorMutex());
    }
    
    if (xSemaphoreTake(getHeatingMutex(), pdMS_TO_TICKS(100))) {
        pumpPower = heatingState.pumpPower;
        fanPower = heatingState.fanPower;
        extractorPower = heatingState.extractorPower;
        xSemaphoreGive(getHeatingMutex());
    }
    
    // Виводимо статус
    Serial.println("\n══════════════════════════════════════════════════════════");
    Serial.println("           📊 СТАТУС СИСТЕМИ");
    Serial.println("══════════════════════════════════════════════════════════");
    Serial.printf("  Час: %s\n", getTimeString().c_str());
    Serial.printf("  Режим: %s\n", getCurrentMode().c_str());
    Serial.println("----------------------------------------------");
    Serial.printf("  Температура: %.1f°C (ціль: %.1f-%.1f°C)\n",
                  tempRoom, config.tempMin, config.tempMax);
    Serial.printf("  Теплоносій: %.1f°C\n", tempCarrier);
    Serial.printf("  Вологість: %.1f%% (ціль: %.1f-%.1f%%)\n",
                  humidity, config.humidityConfig.minHumidity, config.humidityConfig.maxHumidity);
    Serial.printf("  Тиск: %.1f hPa\n", pressure);
    Serial.println("----------------------------------------------");
    Serial.printf("  Насос (A): %d%% (%d)\n", (pumpPower * 100) / 255, pumpPower);
    Serial.printf("  Вентилятор (B): %d%% (%d)\n", (fanPower * 100) / 255, fanPower);
    Serial.printf("  Витяжка (C): %d%% (%d)\n", (extractorPower * 100) / 255, extractorPower);
    Serial.printf("  Зволожувач: %s\n", humidifierState.active ? "ВКЛ" : "ВИМК");
    Serial.printf("  Вентиляція: %s\n", ventState.open ? "ВІДКРИТА" : "ЗАКРИТА");
    Serial.println("----------------------------------------------");
    Serial.printf("  Wi-Fi: %s (%d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
    Serial.printf("  Пам'ять: %lu KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("  Записів в історії: %d\n", historyIndex);
    Serial.println("══════════════════════════════════════════════════════════\n");
}

// ============================================================================
// ФУНКЦІЯ ПОЛУЧЕННЯ ПОТОЧНОГО РЕЖИМУ КЕРУВАННЯ
// ============================================================================

String getCurrentMode() {
    if (heatingState.emergencyMode) return "🆘 АВАРІЯ";
    if (heatingState.forceMode) return "⚡ ФОРСАЖ";
    if (heatingState.manualMode) return "👆 РУЧНИЙ";
    return "🤖 АВТО";
}

// ============================================================================
// ФУНКЦІЯ РОЗРАХУНКУ АДАПТИВНОЇ ВОЛОГОСТІ
// ============================================================================

void calculateAdaptiveHumidity(float tempRoom, float &minHum, float &maxHum) {
    // Базові значення з конфігурації
    minHum = config.humidityConfig.minHumidity;
    maxHum = config.humidityConfig.maxHumidity;

    // Якщо адаптивний режим вимкнено, використовуємо базові значення
    if (!config.humidityConfig.adaptiveMode) {
        return;
    }

    // Коректуємо вологість залежно від температури
    // Чим вище температура, тим нижче повинна бути вологість для комфорту
    float tempAdjustment = (tempRoom - 25.0f) * config.humidityConfig.tempCoefficient;

    minHum -= tempAdjustment;
    maxHum -= tempAdjustment;

    // Обмежуємо значення (не нижче 30% і не вище 80%)
    minHum = constrain(minHum, 30.0f, 75.0f);
    maxHum = constrain(maxHum, minHum + 5.0f, 80.0f);

    // Додаємо гістерезис
    maxHum = minHum + config.humidityConfig.hysteresis;
}

// ============================================================================
// ТЕСТ ВЕНТИЛЯЦІЇ
// ============================================================================

void testVentilation() {
    Serial.println("🔧 Тест вентиляції: відкриття...");
    moveServoSmooth(SERVO_OPEN_ANGLE);
    delay(2000);
    Serial.println("🔧 Тест вентиляції: закриття...");
    moveServoSmooth(SERVO_CLOSED_ANGLE);
    Serial.println("✓ Тест вентиляції завершений");
}