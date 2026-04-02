// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define PROJECT_START_DATE "Dec 7 2025"
#define VERSION "v5.0-D56"
#define VERSION_COMMENT "v5.0 Release: OTA + Backup (День 56)"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES 13756
#define FIRMWARE_SIZE_KB 1475

// Версіонування конфігурації (для міграції)
#define CONFIG_VERSION_MAJOR 5
#define CONFIG_VERSION_MINOR 0

// Оголошення всіх глобальних змінних
extern Preferences prefs;

#endif
