#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <Arduino.h>

// Определения пинов (вставьте свои правильные значения)
#define PUMP_PWM_PIN      21    // Насос (PWM)
#define FAN_PWM_PIN       38    // Вентилятор обогрева (PWM)
#define EXTRACTOR_PIN     12    // Вытяжка (PWM)
#define SERVO_PIN         14    // Сервопривод вентиляции
#define VENT_SWITCH_PIN   13    // Механический выключатель вентиляции
#define HUMIDIFIER_PIN    6     // Увлажнитель

#define PUMP_RELAY_PIN    4     // Реле насоса
#define FAN_RELAY_PIN     5     // Реле вентилятора

// PWM функции
void initPWM();
void setPWM(uint8_t dutyCycle);

// Основные функции управления
void setPumpPercent(uint8_t percent);
void setFanPercent(uint8_t percent);
void setExtractorPercent(uint8_t percent);
void setHeatingPower(uint8_t pumpPercent, uint8_t fanPercent, uint8_t extractorPercent);

// Servo функции
void initServo();
void moveServoSmooth(int targetAngle);

// GPIO функции
void initGPIO();
void setupActuators();

// Управление реле
void controlPump(bool state);
void controlFan(bool state);
void controlHumidifier(bool state);
bool isVentSwitchOpen();

// Task функции (для FreeRTOS)
void heatingTask(void* parameter);
void ventilationTask(void* parameter);

// Getter функции
uint8_t getPumpPercent();
uint8_t getFanPercent();
uint8_t getExtractorPercent();
uint8_t getServoPosition();

#endif // ACTUATOR_MANAGER_H
