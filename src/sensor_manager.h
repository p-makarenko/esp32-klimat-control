#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "system_core.h"
#include "co2_sensor.h"

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
void readCO2Sensor();  // Читання датчика SCD30
float getAdjustedBmeTemperature();  // Отримати скориговану температуру BME280
void sensorTask(void *parameter);

#endif