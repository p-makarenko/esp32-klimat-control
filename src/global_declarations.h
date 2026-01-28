// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "learning_system.h"

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define PROJECT_START_DATE "Dec 7 2025"
#define VERSION "v4.8-D52"
#define VERSION_COMMENT "День 52 розробки"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES 11248
#define FIRMWARE_SIZE_KB 1363

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
