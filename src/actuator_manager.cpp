#include "actuator_manager.h"
#include <Arduino.h>
#include <ESP32Servo.h>

// лобальные переменные
Servo ventServo;
uint8_t currentPumpPercent = 0;
uint8_t currentFanPercent = 0;
uint8_t currentExtractorPercent = 0;
uint8_t currentServoPosition = 90;
bool servoAttached = false;

// ==================== PWM  ====================
void initPWM() {
    ledcSetup(0, 5000, 8);  // анал 0: асос
    ledcSetup(1, 5000, 8);  // анал 1: ентилятор
    ledcSetup(2, 5000, 8);  // анал 2: ытяжка
    
    ledcAttachPin(PUMP_PWM_PIN, 0);
    ledcAttachPin(FAN_PWM_PIN, 1);
    ledcAttachPin(EXTRACTOR_PIN, 2);
}

// ==================== СЫ  ====================
void setPumpPercent(uint8_t percent) {
    percent = constrain(percent, 0, 100);
    currentPumpPercent = percent;
    uint8_t pwmValue = map(percent, 0, 100, 0, 255);
    ledcWrite(0, pwmValue);
    Serial.print("асос: ");
    Serial.print(percent);
    Serial.println("%");
}

void setFanPercent(uint8_t percent) {
    percent = constrain(percent, 0, 100);
    currentFanPercent = percent;
    uint8_t pwmValue = map(percent, 0, 100, 0, 255);
    ledcWrite(1, pwmValue);
    Serial.print("ентилятор: ");
    Serial.print(percent);
    Serial.println("%");
}

void setExtractorPercent(uint8_t percent) {
    percent = constrain(percent, 0, 100);
    currentExtractorPercent = percent;
    uint8_t pwmValue = map(percent, 0, 100, 0, 255);
    ledcWrite(2, pwmValue);
    Serial.print("ытяжка: ");
    Serial.print(percent);
    Serial.println("%");
}

void setHeatingPower(uint8_t pumpPercent, uint8_t fanPercent, uint8_t extractorPercent) {
    setPumpPercent(pumpPercent);
    setFanPercent(fanPercent);
    setExtractorPercent(extractorPercent);
}

// ==================== SERVO  ====================
void initServo() {
    //  присоединяем серво при инициализации
    // ventServo.detach();
    servoAttached = false;
    Serial.println("Серво инициализировано (отключено)");
}

void moveServoSmooth(int targetAngle) {
    targetAngle = constrain(targetAngle, 0, 180);
    
    // вигаем только если положение изменилось
    if (targetAngle != currentServoPosition) {
        // ременно включаем серво
        if (!servoAttached) {
            ventServo.attach(SERVO_PIN);
            servoAttached = true;
        }
        
        ventServo.write(targetAngle);
        delay(300); // аём время на движение
        
        // тключаем серво после движения
        ventServo.detach();
        servoAttached = false;
        
        currentServoPosition = targetAngle;
        Serial.print("Серво двигается в: ");
        Serial.print(targetAngle);
        Serial.println(" градусов");
    }
}

// ==================== GPIO  ====================
void initGPIO() {
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    pinMode(FAN_RELAY_PIN, OUTPUT);
    pinMode(HUMIDIFIER_PIN, OUTPUT);
    pinMode(VENT_SWITCH_PIN, INPUT_PULLUP);
    
    digitalWrite(PUMP_RELAY_PIN, LOW);
    digitalWrite(FAN_RELAY_PIN, LOW);
    digitalWrite(HUMIDIFIER_PIN, LOW);
    
    // значально С вентиляторы ЫЫ
    setPumpPercent(0);
    setFanPercent(0);
    setExtractorPercent(0);
    
    Serial.println("GPIO инициализировано. се вентиляторы Ы.");
}

void setupActuators() {
    initGPIO();
    initPWM();
    initServo();
}

// ====================  ====================
void controlPump(bool state) {
    digitalWrite(PUMP_RELAY_PIN, state ? HIGH : LOW);
    Serial.print("еле насоса: ");
    Serial.println(state ? "" : "Ы");
}

void controlFan(bool state) {
    digitalWrite(FAN_RELAY_PIN, state ? HIGH : LOW);
    Serial.print("еле вентилятора: ");
    Serial.println(state ? "" : "Ы");
}

void controlHumidifier(bool state) {
    digitalWrite(HUMIDIFIER_PIN, state ? HIGH : LOW);
    Serial.print("влажнитель: ");
    Serial.println(state ? "" : "Ы");
}

bool isVentSwitchOpen() {
    return digitalRead(VENT_SWITCH_PIN) == HIGH;
}

// ==================== TASK  ====================
void heatingTask(void* parameter) {
    while (true) {
        // ростая заглушка
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void ventilationTask(void* parameter) {
    while (true) {
        // ростая заглушка
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// ==================== GETTERS ====================
uint8_t getPumpPercent() { return currentPumpPercent; }
uint8_t getFanPercent() { return currentFanPercent; }
uint8_t getExtractorPercent() { return currentExtractorPercent; }
uint8_t getServoPosition() { return currentServoPosition; }
