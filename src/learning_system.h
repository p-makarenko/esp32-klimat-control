#ifndef LEARNING_SYSTEM_H
#define LEARNING_SYSTEM_H

#include <Arduino.h>
#include <Preferences.h>
#include "system_core.h"

// Конфігурація системи навчання
#define MAX_LEARNINGS 100
#define LEARN_TEMP_EPS 0.5f
#define LEARN_MIN_STABLE_TIME 120
#define FAN_MIN_WORK 20
#define PUMP_MIN_WORK 30

// Перелік режимів помпи
enum PumpMode {
    PUMP_OFF = 0,
    PUMP_QUICK_HEAT,
    PUMP_STABLE,
    PUMP_ADAPTIVE_LEARN
};

// Структура запису навчання
struct LearningEntry {
    float roomError;
    uint8_t fanPercent;
    uint8_t pumpPercent;
    uint8_t uses;
    uint32_t lastUsed;
};

// Глобальні змінні (оголошені в .cpp)
extern Preferences prefs;
extern LearningEntry learnings[MAX_LEARNINGS];
extern int learningCount;
extern PumpMode currentPumpMode;
extern bool learningEnabled;

// Змінні стану навчання
extern bool isLearningActive;
extern unsigned long learningStartTime;
extern float learningStartTemp;
extern uint8_t learningCurrentFan;
extern uint8_t learningCurrentPump;

// ============================================================
// ОСНОВНІ ФУНКЦІЇ - ДОДАЙТЕ ВСІ ЦІ ПРОТОТИПИ
// ============================================================
void initLearningSystem();
void saveLearningData();
void loadLearningData();

// Логіка навчання
bool findOptimalSettings(float tempError, uint8_t &fanOut, uint8_t &pumpOut);
void updateLearningSystem(float currentTemp, float targetTemp);
void recordLearning(float tempError, uint8_t fan, uint8_t pump, bool success);

// Управління навчанням
bool getLearningActive();
void setLearningActive(bool active);
unsigned long getLearningStartTime();
float getLearningStartTemp();
uint8_t getLearningCurrentFan();
uint8_t getLearningCurrentPump();
void startLearning(uint8_t fanPower, uint8_t pumpPower);
void stopLearning();

// Утиліти
bool isValidLearning(float err, uint8_t fan, uint8_t pump);
void mergeSimilarLearnings();
void cleanupOldLearnings();
void printLearningStats();

// Серійні команди
void processLearningCommand(const String &cmd);

// ДОДАЙТЕ ЦІ ФУНКЦІЇ ТАКОЖ (якщо їх немає у вашому файлі):
void learningTask(void *parameter);  // якщо є така функція
String getLearningStatus();  // якщо використовується

#endif