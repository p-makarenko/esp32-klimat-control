// ============================================================================
// PLANT_FAN.H - Керування вентилятором обдуву рослин
// ============================================================================
// Режими: таймер (ON/OFF цикл) та "природній вітер" (імітація поривів)
// Зберігання: Preferences namespace "plant_fan"
// ============================================================================

#ifndef PLANT_FAN_H
#define PLANT_FAN_H

#include <Arduino.h>


// ============================================================================
// ФАЗИ РЕЖИМУ ПРИРОДНЬОГО ВІТРУ
// ============================================================================
enum BreezePhase {
    BREEZE_CALM  = 0,  // Спокійно (базова швидкість + хаос)
    BREEZE_RISE  = 1,  // Наростання пориву
    BREEZE_HOLD  = 2,  // Пік пориву
    BREEZE_DECAY = 3   // Затихання пориву
};

// ============================================================================
// ПУБЛІЧНИЙ API
// ============================================================================
void plantFanInit();
void plantFanTask(void* parameter);
void plantFanSave();
void plantFanLoad();

void          plantFanResetBreeze();   // Скинути фазу вітру (при зміні конфігу)
uint8_t       plantFanGetCurrentPower();
BreezePhase   plantFanGetBreezePhase();
bool          plantFanGetTimerState();
unsigned long plantFanGetTimeToNextChange();

// Визначення в system_core.cpp (разом з усіма іншими TaskHandle)
extern TaskHandle_t plantFanTaskHandle;

#endif // PLANT_FAN_H
