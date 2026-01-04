#include "sensor_manager.h"
#include "system_core.h"
#include <Arduino.h>

// Глобальні об'єкти датчиків
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_BME280 bme;
DeviceAddress tempCarrierAddr, tempRoomAddr;

bool initTemperatureSensors() {
  sensors.begin();

  int deviceCount = sensors.getDeviceCount();
  Serial.printf("Знайдено DS18B20 датчиків: %d\n", deviceCount);

  if (deviceCount < 2) {
    Serial.println("⚠ Проблема: Потрібно щонайменше 2 датчики!");
    return false;
  }

  if (!sensors.getAddress(tempCarrierAddr, 0)) {
    Serial.println("⚠ Проблема: Не вдалося знайти адресу датчика теплоносія!");
    return false;
  }
  
  if (!sensors.getAddress(tempRoomAddr, 1)) {
    Serial.println("⚠ Проблема: Не вдалося знайти адресу датчика кімнати!");
    return false;
  }

  sensors.setResolution(tempCarrierAddr, 12);
  sensors.setResolution(tempRoomAddr, 12);

  Serial.println("✓ DS18B20 датчики ініціалізовано");
  return true;
}

bool initBME280() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  delay(100);

  if (!bme.begin(0x76)) {
    if (!bme.begin(0x77)) {
      Serial.println("⚠ Проблема: BME280 не знайдено!");
      return false;
    }
  }

  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X2,
                  Adafruit_BME280::SAMPLING_X16,
                  Adafruit_BME280::SAMPLING_X16,
                  Adafruit_BME280::FILTER_X16,
                  Adafruit_BME280::STANDBY_MS_500);

  Serial.println("✓ BME280 ініціалізовано");
  return true;
}

void readTemperatureSensors() {
  sensors.requestTemperatures();
  
  float carrier = sensors.getTempC(tempCarrierAddr);
  float room = sensors.getTempC(tempRoomAddr);
  
  if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
    // Перевірка на помилкові значення DS18B20: -127, 85, та поза допустимим діапазоном
    bool carrierOk = (carrier > -50.0f && carrier < 125.0f && carrier != -127.0f && carrier != 85.0f);
    bool roomOk = (room > -50.0f && room < 125.0f && room != -127.0f && room != 85.0f);
    
    // Лічильники помилок
    static uint8_t carrierErrorCount = 0;
    static uint8_t roomErrorCount = 0;
    const uint8_t MAX_ERRORS = 10; // 10 помилок підряд = аварія
    
    // Оновлюємо лічильники
    if (!carrierOk) {
      carrierErrorCount++;
      if (carrierErrorCount == 1) {
        Serial.printf("⚠ Датчик теплоносія: помилка %.1f°C\n", carrier);
      } else if (carrierErrorCount >= MAX_ERRORS) {
        Serial.println("🚨 АВАРІЯ: Датчик теплоносія не відповідає!");
      }
    } else {
      if (carrierErrorCount > 0) {
        Serial.println("✓ Датчик теплоносія відновлено");
      }
      carrierErrorCount = 0;
      sensorData.tempCarrier = carrier;
    }
    
    if (!roomOk) {
      roomErrorCount++;
      if (roomErrorCount == 1) {
        Serial.printf("⚠ Датчик кімнати: помилка %.1f°C\n", room);
      } else if (roomErrorCount >= MAX_ERRORS) {
        Serial.println("🚨 АВАРІЯ: Датчик кімнати не відповідає!");
      }
    } else {
      if (roomErrorCount > 0) {
        Serial.println("✓ Датчик кімнати відновлено");
      }
      roomErrorCount = 0;
      sensorData.tempRoom = room;
    }
    
    // Встановлюємо статус: валідний тільки якщо помилок менше порогу
    sensorData.carrierValid = (carrierErrorCount < MAX_ERRORS);
    sensorData.roomValid = (roomErrorCount < MAX_ERRORS);
    sensorData.timestamp = millis();
    
    // Оновлення буфера тренду
    if (sensorData.carrierValid) {
      float* trendBuffer = getTempTrendBufferPtr();
      int currentTrendIndex = getTrendIndexValue();
      
      trendBuffer[currentTrendIndex] = carrier;
      setTrendIndex((currentTrendIndex + 1) % TREND_WINDOW_SIZE);
      if (currentTrendIndex == TREND_WINDOW_SIZE - 1) {
        setTrendBufferFilled(true);
      }
    }
    
    xSemaphoreGive(getSensorMutex());
  }
}

void readBME280() {
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0f;
  
  if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
    // Перевірка на коректність даних
    bool tempOk = (!isnan(temp) && temp > -40.0f && temp < 85.0f);
    bool humOk = (!isnan(hum) && hum >= 0.0f && hum <= 100.0f);
    bool presOk = (!isnan(pres) && pres > 300.0f && pres < 1100.0f);
    bool allOk = (tempOk && humOk && presOk);
    
    // Лічильник помилок
    static uint8_t bmeErrorCount = 0;
    const uint8_t MAX_ERRORS = 10;
    
    if (!allOk) {
      bmeErrorCount++;
      if (bmeErrorCount == 1) {
        Serial.printf("⚠ BME280 помилка: T=%.1f H=%.1f P=%.1f\n", temp, hum, pres);
      } else if (bmeErrorCount >= MAX_ERRORS) {
        Serial.println("🚨 АВАРІЯ: BME280 не відповідає!");
      }
    } else {
      if (bmeErrorCount > 0) {
        Serial.println("✓ BME280 відновлено");
      }
      bmeErrorCount = 0;
      // Оновлюємо тільки валідні значення
      if (tempOk) sensorData.tempBME = temp;
      if (humOk) sensorData.humidity = hum;
      if (presOk) sensorData.pressure = pres;
    }
    
    sensorData.bmeValid = (bmeErrorCount < MAX_ERRORS);
    
    xSemaphoreGive(getSensorMutex());
  }
}

void sensorTask(void *parameter) {
  Serial.println("✓ Задача датчиків запущена");
  
  // Чекаємо на ініціалізацію інших компонентів
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  while (1) {
    // Читання датчиків
    readTemperatureSensors();
    readBME280();
    
    // Вивід відладкових даних (кожні 30 секунд)
    static unsigned long lastDebugPrint = 0;
    unsigned long now = millis();
    
    if (now - lastDebugPrint > 30000) {
      lastDebugPrint = now;
      if (sensorData.carrierValid && sensorData.roomValid && sensorData.bmeValid) {
        Serial.printf("[SENSOR] T:%.1f/%.1f°C H:%.1f%% P:%.1fhPa\n",
                     sensorData.tempRoom, sensorData.tempCarrier,
                     sensorData.humidity, sensorData.pressure);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL));
  }
}