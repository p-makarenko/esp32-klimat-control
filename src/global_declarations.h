// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "learning_system.h"

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define PROJECT_START_DATE "Dec 7 2025"
#define VERSION "v4.5-D28"
#define VERSION_COMMENT "День 28 розробки"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES 6710
#define FIRMWARE_SIZE_KB 1045

// Оголошення всіх глобальних змінних
extern Preferences prefs;
extern LearningEntry learnings[MAX_LEARNINGS];
extern int learningCount;
extern PumpMode currentPumpMode;
extern bool learningEnabled;

// Функції для налаштувань
void initLearningPreferences();
void saveLearningPreferences();

#endif
