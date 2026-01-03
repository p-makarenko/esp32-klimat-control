#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H

#include <Arduino.h>

// Функції статусу
void autoPrintStatus();
String getCurrentMode();  // Додайте цей рядок

// Функція розрахунку адаптивної вологості
void calculateAdaptiveHumidity(float tempRoom, float &minHum, float &maxHum);

// Тестування
void testVentilation();

#endif