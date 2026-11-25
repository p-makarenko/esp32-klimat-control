// ============================================================================
// TC421_CONTROLLER.H - Керування Wi-Fi фітолампою TC421 (4 канали + природний день)
// ============================================================================
// Канали:
//   CH1 (index 0) - Червоний (Red)
//   CH2 (index 1) - Білий (White)
//   CH3 (index 2) - Жовтий (Yellow / Warm)
//   CH4 (index 3) - Інфрачервоний + Ультрафіолет (IR + UV)
//   CH5 (index 4) - Не підключений (завжди 0%)
// ============================================================================

#ifndef TC421_CONTROLLER_H
#define TC421_CONTROLLER_H

#include <Arduino.h>

#define TC421_UDP_PORT      5000
#define TC421_PACKET_SIZE   64
#define TC421_CHANNELS      5
#define TC421_ACTIVE_CH     4
#define TC421_PROFILE_COUNT 4

enum TC421Preset {
    TC421_PRESET_VEG     = 0,  // Вегетація 18/6
    TC421_PRESET_BLOOM   = 1,  // Цвітіння 12/12
    TC421_PRESET_NATURAL = 2,  // Природний 14/10
    TC421_PRESET_CUSTOM  = 3   // Мій профіль (користувацький)
};

struct TC421ScheduleConfig {
    char    name[28];          // Назва профілю
    bool    enabled;           // true = ESP32-автопілот по природній кривій
    uint8_t sunriseHour;       // 0..23
    uint8_t sunriseMin;        // 0..59
    uint8_t sunsetHour;        // 0..23
    uint8_t sunsetMin;         // 0..59
    uint8_t sunriseDurMin;     // Тривалість світанку (5..120 хв)
    uint8_t sunsetDurMin;      // Тривалість заходу (5..120 хв)
    uint8_t dayMax[4];         // Денний максимум (0..100%): [0]=Red, [1]=White, [2]=Yellow, [3]=IR+UV
    uint8_t dawnIrDurMin;      // Імпульс CH4 на світанку (0..45 хв)
    uint8_t dawnIrPct;         // Потужність CH4 на світанку (0..100%)
    uint8_t middayUvHours;     // Полуденне вікно CH4 в центрі дня (0..12 год; 0=вимкнено)
    uint8_t eodIrDurMin;       // EOD Far-Red "сигнал сну" після заходу (0..45 хв)
    uint8_t eodIrPct;          // Потужність EOD Far-Red (0..100%)
    uint8_t ecoMaxPercent;     // Макс. потужність під час ЕКО/блекауту (0=вимкн, напр. 15%)
    uint8_t dayBaseIrPct;      // Фонова денна потужність CH4 між піками (0..50%, напр. 3-5%)
};

struct TC421State {
    char ip[20];
    char mac[13];                     // 12 ASCII hex-символів + '\0'
    uint8_t channels[TC421_CHANNELS]; // 0..100% (фактично відправлені)
    bool liveMode;                    // true = керується з ESP32 (автопілот або ручний)
    bool found;                       // true = контролер відповів на SearchDevice
    bool ecoLimited;                  // true = зараз діє обмеження ЕКО-режиму
    unsigned long lastSeenMs;
};

// Ініціалізація (завантажує IP/MAC + розклад з NVS, запускає задачу автопілота)
void tc421Init();

// Пошук TC421 у локальній мережі (AP 192.168.10.x + STA + збережений IP)
bool tc421Discover(uint16_t timeoutMs = 1500);

// Встановити IP вручну (опціонально MAC) і перевірити зв'язок
bool tc421SetTarget(const String& ipStr, const String& macStr = "");

// Ручне встановлення яскравості 4 каналів (0..100%, 5-й = 0; saveNvs=false для live-повзунків)
bool tc421SetChannels(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5 = 0, bool saveNvs = true);

// Вимкнути всі канали (0%)
bool tc421Off();

// Вийти з керування ESP32 та повернути TC421 на його внутрішній розклад
bool tc421ResumeSchedule();

// Синхронізувати годинник TC421 із системним часом ESP32 (NTP)
bool tc421SyncTime();

// Робота з профілями та конфігурацією "Конструктора природного дня"
TC421ScheduleConfig tc421GetSchedule();
void tc421SetSchedule(const TC421ScheduleConfig& cfg, bool applyNow = true);
uint8_t tc421GetActiveProfile();
String tc421GetProfileName(uint8_t idx);
void tc421SelectProfile(uint8_t idx);
void tc421ResetActiveProfile();
void tc421ApplyPreset(TC421Preset preset);

// Розрахувати значення 4 каналів (0..100%) для заданої хвилини доби (0..1439)
void tc421CalculateAt(const TC421ScheduleConfig& cfg, uint16_t minuteOfDay, uint8_t out[4], bool applyEco = false);

// Примусово оновити поточний стан лампи за розкладом (викликається раз/хв та при зміні ЕКО)
void tc421SchedulerUpdate(bool forceSend = false);

// Отримати копію поточного стану
TC421State tc421GetState();

extern TaskHandle_t tc421TaskHandle;

#endif // TC421_CONTROLLER_H
