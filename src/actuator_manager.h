#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <Arduino.h>

// Визначення пінів (вставте свої правильні значення)
#define PUMP_PWM_PIN      21    // Насос (PWM)
#define FAN_PWM_PIN       38    // Вентилятор обігріву (PWM)
#define EXTRACTOR_PIN     12    // Витяжка (PWM)
#define SERVO_PIN         14    // Сервопривод вентиляції
#define VENT_SWITCH_PIN   13    // Механічний вимикач вентиляції
#define HUMIDIFIER_PIN    6     // Зволожувач

#define PUMP_RELAY_PIN    4     // Реле насоса
#define FAN_RELAY_PIN     5     // Реле вентилятора

// PWM функції
void initPWM();
void setPWM(uint8_t dutyCycle);

// Основні функції управління
void setPumpPercent(uint8_t percent);
void setFanPercent(uint8_t percent);
void setExtractorPercent(uint8_t percent);
void setHeatingPower(uint8_t pumpPercent, uint8_t fanPercent, uint8_t extractorPercent);

// Servo функції
void initServo();
void moveServoSmooth(int targetAngle);

// GPIO функції
void initGPIO();
void setupActuators();

// Управління реле
void controlPump(bool state);
void controlFan(bool state);
void controlHumidifier(bool state);
bool isVentSwitchOpen();

// Auto calibration
void startAutoCalibration();
void processAutoCalibration();

// Task функції (для FreeRTOS)
void heatingTask(void* parameter);
void ventilationTask(void* parameter);

// Getter функції
uint8_t getPumpPercent();
uint8_t getFanPercent();
uint8_t getExtractorPercent();
uint8_t getServoPosition();

#endif // ACTUATOR_MANAGER_H