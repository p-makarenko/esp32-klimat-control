#ifndef ADVANCED_CLIMATE_LOGIC_H
#define ADVANCED_CLIMATE_LOGIC_H

#include <Arduino.h>
#include "system_core.h"
#include "utility_functions.h"  // Додайте цей інклуд

// Два режими роботи Serial Monitor
extern bool compactMode;

// Функції керування режимами
void printCompactMode();
void printExtendedMode();
void toggleMode();
void processCompactCommand(String command);
void processExtendedCommand(String command);
void processAdvancedSerialCommand();

// Розширена логіка керування
void smartHeatingControl();
void advancedHumidityControl(float humidity, float tempRoom);
void advancedUpdateExtractorTimer();  // Перейменували, щоб уникнути конфлікту
void monitorSystemHealth();

// Ініціалізація та завдання
void initAdvancedLogic();
void advancedLogicTask(void *parameter);

#endif