#ifndef SYSTEM_CORE_H
#define SYSTEM_CORE_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "config.h"

// Структури даних
struct SensorData {
  float tempCarrier;
  float tempRoom;
  float tempBME;
  float humidity;
  float pressure;
  bool carrierValid;
  bool roomValid;
  bool bmeValid;
  unsigned long timestamp;
};

struct HumidityConfig {
  float minHumidity;
  float maxHumidity;
  float tempCoefficient;
  bool enabled;
  uint32_t minInterval;
  uint32_t maxRunTime;
  uint8_t hysteresis;
  bool adaptiveMode;
};

struct ExtractorTimer {
  bool enabled;
  uint16_t onMinutes;
  uint16_t offMinutes;
  bool state;
  unsigned long cycleStart;
  unsigned long lastChange;
  uint8_t powerPercent;
};

struct SystemConfig {
  float tempMin;
  float tempMax;
  float tempVentMin;
  float tempVentMax;
  bool heatingEnabled;
  bool humidifierEnabled;
  uint32_t statusPeriod;
  uint8_t fanMinPercent;
  uint8_t fanMaxPercent;
  uint8_t pumpMinPercent;
  uint8_t pumpMaxPercent;
  uint8_t extractorMinPercent;
  uint8_t extractorMaxPercent;
  bool manualVentControl;
  bool use24hFormat;
  
  // Калібрування серво
  int servoClosedAngle;
  int servoOpenAngle;
  
  // Serial вивід
  bool autoStatusEnabled;
  
  HumidityConfig humidityConfig;
  
  uint8_t a_adaptive_min;
  uint8_t a_adaptive_max;
  uint8_t a_normal_range_min;
  uint8_t a_normal_range_max;
  uint8_t b_start_percent;
  uint16_t trend_window_seconds;
  float temp_drop_threshold;
  uint16_t heating_check_interval;
  uint8_t adaptive_temp_step;
  uint8_t adaptive_hum_step;
  
  ExtractorTimer extractorTimer;
  uint16_t history_size;
};

struct HeatingState {
  bool active;
  uint8_t pumpPower;
  uint8_t fanPower;
  uint8_t extractorPower;
  bool forceMode;
  bool emergencyMode;
  bool manualMode;
  
  float tempHistory[TEMP_HISTORY_SIZE];
  float carrierHistory[TEMP_HISTORY_SIZE];
  int historyIndex;
  unsigned long lastCheck;
  
  bool adaptive_heating_active;
  float original_temp_target;
  float original_hum_target_min;
  float original_hum_target_max;
  float adaptive_temp_target;
  float adaptive_hum_target_min;
  float adaptive_hum_target_max;
  uint8_t adaptive_stage;
  unsigned long adaptive_start_time;
  unsigned long stage_start_time;
  float initial_carrier_temp;
  float initial_room_temp;
  bool temp_recovered;
  uint8_t optimal_b_speed;
  unsigned long last_efficiency_check;
  float last_room_temp_check;
  float last_carrier_temp_check;
};

struct VentilationState {
  bool open;
  int currentAngle;
  bool servoAttached;
  unsigned long lastMove;
  bool switchState;
  bool moving;
  bool calibrationMode;  // Режим калібрування - ігнорує вимикач
  
  // Автокалібрування
  unsigned long lastSwitchChange;
  uint8_t switchChangeCount;
  bool autoCalibrationActive;
  uint8_t autoCalibrationStep;  // 0=idle, 1=move to first stop, 2=move to second stop, 3=test
};

struct HumidifierState {
  bool active;
  unsigned long startTime;
  unsigned long lastCycle;
  float lastHumidity;
  uint8_t cyclesToday;
};

struct HistoryData {
  unsigned long timestamp;
  float tempCarrier;
  float tempRoom;
  float tempBME;
  float humidity;
  float pressure;
  uint8_t pumpPower;
  uint8_t fanPower;
  uint8_t extractorPower;
  String mode;
};

// Глобальні об'єкти
extern Preferences preferences;
extern WebServer server;

// Глобальні змінні
extern SystemConfig config;
extern SensorData sensorData;
extern HeatingState heatingState;
extern VentilationState ventState;
extern HumidifierState humidifierState;
extern time_t currentTime;
extern struct tm timeInfo;
extern bool enableStatusOutput;

// Прототипи функцій
void loadConfiguration();
void saveConfiguration();
void initMutexes();
void createTasks();
void initTime();
void printMenu();
void syncTime();
String getTimeString();
String getFormattedTime();
String getDateString();

// Функції для доступу до тренду температури
float* getTempTrendBufferPtr();
int getTrendIndexValue();
void setTrendIndex(int index);
bool isTrendBufferFilled();
void setTrendBufferFilled(bool filled);

// Геттери та сеттери для часових змінних
unsigned long getLastStatusPrint();
void setLastStatusPrint(unsigned long time);
unsigned long getLastSerialCheck();
void setLastSerialCheck(unsigned long time);
unsigned long getLastTrendCheck();
void setLastTrendCheck(unsigned long time);
unsigned long getLastHistorySave();
void setLastHistorySave(unsigned long time);
unsigned long getLastExtractorCheck();
void setLastExtractorCheck(unsigned long time);
unsigned long getLastTimeSync();
void setLastTimeSync(unsigned long time);

// Геттери/сеттери для аварійного режиму
unsigned long getEmergencyStartTime();
void setEmergencyStartTime(unsigned long time);
float getEmergencyStartTempCarrier();
void setEmergencyStartTempCarrier(float temp);
float getEmergencyStartTempRoom();
void setEmergencyStartTempRoom(float temp);
void checkEmergencyTimeout();

// Функції для доступу до м'ютексів
SemaphoreHandle_t getSensorMutex();
SemaphoreHandle_t getConfigMutex();
SemaphoreHandle_t getHeatingMutex();
SemaphoreHandle_t getHistoryMutex();
SemaphoreHandle_t getTimeMutex();

// Функції для роботи з часом
bool isTimeSynced();
void setTimeSynced(bool synced);
struct tm* getTimeInfo();
void updateTimeInfo();
time_t getCurrentTime();
void setCurrentTime(time_t t);

// Прототипи задач FreeRTOS
void timeTask(void *parameter);
extern bool learningEnabled;
extern int learningCount;

// Прототип задачі розширеної логіки
void advancedLogicTask(void *parameter);

#endif