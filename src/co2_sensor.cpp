#include "co2_sensor.h"
#include "config.h"
#include <Wire.h>

// Константи SCD30
#define SCD30_I2C_ADDR 0x61

// Глобальні змінні
static TwoWire scd30Wire(1);  // Другий I2C шина (I2C1)
static SensirionI2cScd30 scd30;
static uint16_t co2_level = 0;
static float temperature = 0.0f;
static float humidity = 0.0f;
static bool co2_valid = false;
static bool scd30_initialized = false;

// Ініціалізація SCD30
bool initSCD30() {
  if (scd30_initialized) {
    return true;
  }

  // Ініціалізуємо окремий I2C для SCD30 (GPIO4=SDA, GPIO5=SCL)
  scd30Wire.begin(SCD30_I2C_SDA, SCD30_I2C_SCL);
  scd30Wire.setClock(100000);  // 100 kHz для SCD30

  // Ініціалізуємо сам датчик
  scd30.begin(scd30Wire, SCD30_I2C_ADDR);

  // Зупиняємо поточне вимірювання перед конфігуруванням
  uint16_t err = scd30.stopPeriodicMeasurement();
  if (err != 0) {
    Serial.println("[SCD30] Помилка: не вдалося зупинити вимірювання");
    return false;
  }
  delay(500);

  // Встановлюємо інтервал вимірювання: 2 секунди (оптимально для моніторингу)
  err = scd30.setMeasurementInterval(2);
  if (err != 0) {
    Serial.println("[SCD30] Помилка: не вдалося встановити інтервал вимірювання");
    return false;
  }
  delay(100);

  // Запускаємо періодичне вимірювання
  err = scd30.startPeriodicMeasurement(0);  // 0 = без калібрування тиску
  if (err != 0) {
    Serial.println("[SCD30] Помилка: не вдалося запустити вимірювання");
    return false;
  }

  scd30_initialized = true;
  Serial.println("[SCD30] ✓ Датчик ініціалізовано");
  return true;
}

// Читання даних з SCD30
void readSCD30() {
  if (!scd30_initialized) {
    co2_valid = false;
    return;
  }

  // Перевіряємо, чи готові дані
  uint16_t data_ready = 0;
  uint16_t err = scd30.getDataReady(data_ready);

  if (err != 0) {
    co2_valid = false;
    return;
  }

  if (!data_ready) {
    // Дані ще не готові
    return;
  }

  // Читаємо дані
  float co2;
  float temp;
  float humidity_raw;

  err = scd30.readMeasurementData(co2, temp, humidity_raw);

  if (err != 0) {
    co2_valid = false;
    Serial.println("[SCD30] Помилка читання даних");
    return;
  }

  // Оновлюємо глобальні змінні
  co2_level = (uint16_t)co2;
  temperature = temp;
  humidity = humidity_raw;

  // Валідація даних
  if (co2_level > 0 && co2_level < 5000 && temperature > -40 && temperature < 85) {
    co2_valid = true;
  } else {
    co2_valid = false;
  }
}

// Отримання рівня CO2
float getCO2Level() {
  return co2_valid ? (float)co2_level : 0.0f;
}

// Отримання температури від SCD30
float getSCD30Temperature() {
  return co2_valid ? temperature : 0.0f;
}

// Отримання вологості від SCD30
float getSCD30Humidity() {
  return co2_valid ? humidity : 0.0f;
}

// Перевірка валідності даних
bool isCO2Valid() {
  return co2_valid;
}

// Налаштування інтервалу вимірювання (2-1800 секунд)
void setSCD30MeasurementInterval(uint16_t intervalSeconds) {
  if (!scd30_initialized) {
    return;
  }

  // Обмежуємо діапазон
  if (intervalSeconds < 2) intervalSeconds = 2;
  if (intervalSeconds > 1800) intervalSeconds = 1800;

  // Зупиняємо вимірювання
  scd30.stopPeriodicMeasurement();
  delay(500);

  // Встановлюємо новий інтервал
  uint16_t err = scd30.setMeasurementInterval(intervalSeconds);
  if (err == 0) {
    // Запускаємо знову
    scd30.startPeriodicMeasurement(0);
    Serial.printf("[SCD30] Інтервал встановлено: %d сек\n", intervalSeconds);
  } else {
    Serial.printf("[SCD30] Помилка встановлення інтервалу: %d\n", err);
  }
}
