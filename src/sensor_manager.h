#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "system_core.h"

// ВИДАЛІТЬ ці extern (вони вже оголошені в .cpp):
// extern OneWire oneWire;
// extern DallasTemperature sensors;
// extern Adafruit_BME280 bme;
// extern DeviceAddress tempCarrierAddr, tempRoomAddr;

// Залиште тільки прототипи функцій:
bool initTemperatureSensors();
bool initBME280();
void readTemperatureSensors();
void readBME280();
void sensorTask(void *parameter);

#endif