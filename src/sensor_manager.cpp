#include "sensor_manager.h"
#include "system_core.h"
#include <Arduino.h>

// Р“Р»РѕР±Р°Р»СЊРЅС– РѕР±'С”РєС‚Рё РґР°С‚С‡РёРєС–РІ
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_BME280 bme;
DeviceAddress tempCarrierAddr, tempRoomAddr;

bool initTemperatureSensors() {
  sensors.begin();

  int deviceCount = sensors.getDeviceCount();
  Serial.printf("Р—РЅР°Р№РґРµРЅРѕ DS18B20 РґР°С‚С‡РёРєС–РІ: %d\n", deviceCount);

  if (deviceCount < 2) {
    Serial.println("вљ  РџРћРџР•Р Р•Р”Р–Р•РќРќРЇ: РџРѕС‚СЂС–Р±РЅРѕ С‰РѕРЅР°Р№РјРµРЅС€Рµ 2 РґР°С‚С‡РёРєРё!");
    return false;
  }

  if (!sensors.getAddress(tempCarrierAddr, 0)) {
    Serial.println("вљ  РџРћРџР•Р Р•Р”Р–Р•РќРќРЇ: РќРµ РІРґР°Р»РѕСЃСЏ Р·РЅР°Р№С‚Рё Р°РґСЂРµСЃСѓ РґР°С‚С‡РёРєР° С‚РµРїР»РѕРЅРѕСЃС–СЏ!");
    return false;
  }
  
  if (!sensors.getAddress(tempRoomAddr, 1)) {
    Serial.println("вљ  РџРћРџР•Р Р•Р”Р–Р•РќРќРЇ: РќРµ РІРґР°Р»РѕСЃСЏ Р·РЅР°Р№С‚Рё Р°РґСЂРµСЃСѓ РґР°С‚С‡РёРєР° РєС–РјРЅР°С‚Рё!");
    return false;
  }

  sensors.setResolution(tempCarrierAddr, 12);
  sensors.setResolution(tempRoomAddr, 12);

  Serial.println("вњ“ DS18B20 РґР°С‚С‡РёРєРё С–РЅС–С†С–Р°Р»С–Р·РѕРІР°РЅРѕ");
  return true;
}

bool initBME280() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  delay(100);

  if (!bme.begin(0x76)) {
    if (!bme.begin(0x77)) {
      Serial.println("вљ  РџРћРџР•Р Р•Р”Р–Р•РќРќРЇ: BME280 РЅРµ Р·РЅР°Р№РґРµРЅРѕ!");
      return false;
    }
  }

  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X2,
                  Adafruit_BME280::SAMPLING_X16,
                  Adafruit_BME280::SAMPLING_X16,
                  Adafruit_BME280::FILTER_X16,
                  Adafruit_BME280::STANDBY_MS_500);

  Serial.println("вњ“ BME280 С–РЅС–С†С–Р°Р»С–Р·РѕРІР°РЅРѕ");
  return true;
}

void readTemperatureSensors() {
  sensors.requestTemperatures();
  
  float carrier = sensors.getTempC(tempCarrierAddr);
  float room = sensors.getTempC(tempRoomAddr);
  
  if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
    sensorData.tempCarrier = carrier;
    sensorData.tempRoom = room;
    sensorData.carrierValid = (carrier > -50.0f && carrier < 125.0f && carrier != -127.0f);
    sensorData.roomValid = (room > -50.0f && room < 125.0f && room != -127.0f);
    sensorData.timestamp = millis();
    
    // РћРЅРѕРІР»РµРЅРЅСЏ Р±СѓС„РµСЂСѓ С‚СЂРµРЅРґСѓ
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
    sensorData.tempBME = temp;
    sensorData.humidity = hum;
    sensorData.pressure = pres;
    sensorData.bmeValid = (!isnan(temp) && !isnan(hum) && !isnan(pres));
    xSemaphoreGive(getSensorMutex());
  }
}

void sensorTask(void *parameter) {
  Serial.println("вњ“ Р—Р°РґР°С‡Сѓ РґР°С‚С‡РёРєС–РІ Р·Р°РїСѓС‰РµРЅРѕ");
  
  // Р§РµРєР°С”РјРѕ РЅР° С–РЅС–С†С–Р°Р»С–Р·Р°С†С–СЋ С–РЅС€РёС… РєРѕРјРїРѕРЅРµРЅС‚С–РІ
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  while (1) {
    // Р—С‡РёС‚СѓРІР°РЅРЅСЏ РґР°С‚С‡РёРєС–РІ
    readTemperatureSensors();
    readBME280();
    
    // Р’РёРІС–Рґ РІС–РґР»Р°РґРєРѕРІРёС… РґР°РЅРёС… (РєРѕР¶РЅС– 30 СЃРµРєСѓРЅРґ)
    static unsigned long lastDebugPrint = 0;
    unsigned long now = millis();
    
    if (now - lastDebugPrint > 30000) {
      lastDebugPrint = now;
      if (sensorData.carrierValid && sensorData.roomValid && sensorData.bmeValid) {
        Serial.printf("[SENSOR] T:%.1f/%.1fВ°C H:%.1f%% P:%.1fhPa\n",
                     sensorData.tempRoom, sensorData.tempCarrier,
                     sensorData.humidity, sensorData.pressure);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL));
  }
}
