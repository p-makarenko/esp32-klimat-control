#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "system_core.h"

// УБЕРИТЕ эти extern (они уже объявлены в .cpp):
// extern OneWire oneWire;
// extern DallasTemperature sensors;
// extern Adafruit_BME280 bme;
// extern DeviceAddress tempCarrierAddr, tempRoomAddr;

// Оставьте только прототипы функций:
bool initTemperatureSensors();
bool initBME280();
void readTemperatureSensors();
void readBME280();
void sensorTask(void *parameter);

#endif