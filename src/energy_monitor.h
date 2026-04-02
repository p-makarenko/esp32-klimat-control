#ifndef ENERGY_MONITOR_H
#define ENERGY_MONITOR_H

#include <Arduino.h>

// ============================================================================
// ЕНЕРГОКОНТРОЛЕР - МОНІТОРИНГ ЕНЕРГІЇ
// ============================================================================

struct EnergyMeasurements {
    float voltage = 0.0;      // V
    float current = 0.0;      // A
    float power = 0.0;        // W
    float energy = 0.0;       // kWh
    float frequency = 0.0;    // Hz
    float powerFactor = 0.0;  // PF
    bool error = true;
};

// RAM буфер для графіка потужності (зберігається на ESP32, не в браузері)
#define ENERGY_POWER_BUFFER_SIZE 720   // 720 точок = 24 години при інтервалі 2 хв
struct PowerRecord {
    uint32_t timestamp;  // Unix time
    float power;         // W
    float voltage;       // V
    float current;       // A
};

extern PowerRecord powerBuffer[];
extern uint16_t powerBufferIndex;
extern uint16_t powerBufferCount;

// Ініціалізація енергоконтролера
void initEnergyMonitor();

// Оновлення даних з лічильника PZEM004Tv30
void updateEnergyData();

// Отримати поточні вимірювання
EnergyMeasurements getEnergyMeasurements();

// Запис історії енергозатрат
void appendEnergyHistory(float energy);

// Скидання лічильника енергії
void resetEnergyCounter();

// Веб-обробники для енергоконтролера
void handleEnergyPage();
void handleEnergyAPI();
void handleEnergyHistory();
void handleEnergyHistoryStats();
void handleEnergyReset();
void handleEnergyPowerData();

#endif
