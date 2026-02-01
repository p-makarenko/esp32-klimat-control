// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "learning_system.h"

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define PROJECT_START_DATE "Dec 7 2025"
#define VERSION "v5.0-D55"
#define VERSION_COMMENT "v5.0 Release: OTA + Backup (День 55)"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES 13572
#define FIRMWARE_SIZE_KB 1467

// Версіонування конфігурації (для міграції)
#define CONFIG_VERSION_MAJOR 5
#define CONFIG_VERSION_MINOR 0

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
