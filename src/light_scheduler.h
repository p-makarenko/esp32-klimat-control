// ============================================================================
// LIGHT_SCHEDULER.H - Емуляція природного освітлення (схід/захід сонця)
// ============================================================================
// Зберігання: Preferences (NVS)
// Логіка: лінійна інтерполяція яскравості RGBW між ключовими точками доби
// ============================================================================

#ifndef LIGHT_SCHEDULER_H
#define LIGHT_SCHEDULER_H

#include <Arduino.h>
#include "dmx_controller.h"

// ============================================================================
// КЛЮЧОВІ ТОЧКИ (keyframes) ДОБОВОГО ЦИКЛУ
// Кожна точка: час + цільовий RGBW
// ============================================================================
#define LIGHT_KEYFRAME_COUNT 8  // макс кількість ключових точок

struct LightKeyframe {
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t r;          // CH2: червоний
    uint8_t g;          // CH3: зелений
    uint8_t b;          // CH4: синій
    uint8_t master;     // CH1: загальна яскравість (0=вимкнено)
};

// ============================================================================
// КОНФІГУРАЦІЯ РОЗКЛАДУ
// ============================================================================
struct LightScheduleConfig {
    bool enabled;                               // вмикач всього розкладу
    uint8_t keyframeCount;
    LightKeyframe keyframes[LIGHT_KEYFRAME_COUNT];
};

// ============================================================================
// ПУБЛІЧНИЙ API
// ============================================================================
void lightSchedulerInit();
void lightSchedulerUpdate();                    // викликати кожну хвилину
void lightSchedulerSave();
void lightSchedulerLoad();

// Обчислити поточний RGBW (без застосування)
RGBWColor lightSchedulerCalculate(uint8_t hour, uint8_t minute);

// Доступ до конфігу (для веб)
LightScheduleConfig& lightSchedulerGetConfig();
void lightSchedulerSetConfig(const LightScheduleConfig& cfg);

// Завантажений за замовчуванням пресет "природне сонце"
void lightSchedulerLoadDefaultPreset();

extern TaskHandle_t lightSchedulerTaskHandle;

#endif // LIGHT_SCHEDULER_H
