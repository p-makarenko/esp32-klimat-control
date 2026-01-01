#ifndef ADVANCED_CLIMATE_LOGIC_H
#define ADVANCED_CLIMATE_LOGIC_H

#include <Arduino.h>
#include "system_core.h"
#include "utility_functions.h"  // Добавьте этот инклюд

// Два режима работы Serial Monitor
extern bool compactMode;

// Функции управления режимами
void printCompactMode();
void printExtendedMode();
void toggleMode();
void processCompactCommand(String command);
void processExtendedCommand(String command);
void processAdvancedSerialCommand();

// Расширенная логика управления
void smartHeatingControl();
void advancedHumidityControl(float humidity, float tempRoom);
void advancedUpdateExtractorTimer();  // Переименовали, чтобы избежать конфликта
void monitorSystemHealth();

// Инициализация и задачи
void initAdvancedLogic();
void advancedLogicTask(void *parameter);

#endif