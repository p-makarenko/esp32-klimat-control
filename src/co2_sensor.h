#ifndef CO2_SENSOR_H
#define CO2_SENSOR_H

#include <Arduino.h>
#include <SensirionI2cScd30.h>

// Ініціалізація SCD30 датчика
bool initSCD30();

// Читання даних з SCD30 датчика
void readSCD30();

// Отримання рівня CO2
uint16_t getCO2Level();

// Перевірка валідності даних
bool isCO2Valid();

// Налаштування інтервалу вимірювання (2-1800 секунд)
void setSCD30MeasurementInterval(uint16_t intervalSeconds);

#endif
