#ifndef ENERGY_MONITOR_H
#define ENERGY_MONITOR_H

#include <Arduino.h>

// ============================================================================
// ЕНЕРГОКОНТРОЛЕР - МОНІТОРИНГ ЕНЕРГІЇ
// ============================================================================
// На основі indacator2.txt - для подальшої інтеграції лічильника PZEM004Tv30

struct EnergyMeasurements {
    float voltage = 0.0;      // V
    float current = 0.0;      // A
    float power = 0.0;        // W
    float energy = 0.0;       // kWh
    float frequency = 0.0;    // Hz
    float powerFactor = 0.0;  // PF
    bool error = true;
};

// Ініціалізація енергоконтролера
void initEnergyMonitor();

// Оновлення даних з лічильника PZEM004Tv30
void updateEnergyData();

// Отримати поточні вимірювання
EnergyMeasurements getEnergyMeasurements();

// Запис історії енергозатрат
void appendEnergyHistory(float energy);

// Веб-обробники для энергоконтролера
void handleEnergyPage();
void handleEnergyAPI();
void handleEnergyHistory();
void handleEnergyHistoryStats();

#endif
