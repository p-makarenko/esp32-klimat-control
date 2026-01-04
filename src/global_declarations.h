// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "learning_system.h"

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define VERSION __DATE__ " " __TIME__
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES 5846
#define SESSION_TOKENS 59557

// Оголошення всіх глобальних змінних
extern Preferences prefs;
extern LearningEntry learnings[MAX_LEARNINGS];
extern int learningCount;
extern PumpMode currentPumpMode;
extern bool learningEnabled;
extern bool isLearningActive;
extern unsigned long learningStartTime;
extern float learningStartTemp;
extern uint8_t learningCurrentFan;
extern uint8_t learningCurrentPump;

#endif