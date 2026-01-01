#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H

#include <Arduino.h>

// Функции статуса
void autoPrintStatus();
String getCurrentMode();  // Добавьте эту строку

// Функция расчета адаптивной влажности
void calculateAdaptiveHumidity(float tempRoom, float &minHum, float &maxHum);

// Тестирование
void testVentilation();

#endif