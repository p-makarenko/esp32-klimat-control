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
  float bmeOffset;        // Різниця між DS18B20 (кімната) і BME280 (постійна)
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
  uint16_t onSeconds;
  uint16_t offMinutes;
  uint16_t offSeconds;
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
  bool seasonalHeatingDisable;  // Автоматичне відключення обігріву в теплі місяці
  bool coolingMode;              // Режим охолодження (літо: холодна вода в теплоносії)
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
  int servoSpeed;  // Швидкість руху серво (затримка в мс між градусами)
  
  // Serial вивід
  bool autoStatusEnabled;
  
  // Налаштування мережі
  bool useStaticIP;
  String staticIP;
  String gateway;
  String subnet;
  String dns;
  
  // Безпека
  bool useAuth;
  String authLogin;
  String authPassword;
  
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

  // Параметри моніторингу аварій
  float powerOutageTempDropThreshold;   // Поріг падіння температури для виявлення аварії (°C)
  float powerOutageTempRiseThreshold;   // Поріг зростання температури для підтвердження відновлення (°C)
  uint16_t powerOutageCheckInterval;    // Інтервал перевірки тренду (секунди)
  uint16_t powerOutageStage1Time;       // Тривалість етапу 1 діагностики (секунди)
  uint16_t powerOutagePauseTime;        // Тривалість паузи між спробами (секунди)
  uint16_t powerOutageStage2Time;       // Тривалість етапу 2/3 спроб (секунди)
  uint16_t powerOutageAutoExitTime;     // Час оцінювання стабільного зростання для автовиходу (секунди)

  ExtractorTimer extractorTimer;
  uint16_t history_size;

  // Поріг логування даних (для економії пакетів)
  float logTempThreshold;      // Мінімальна зміна температури кімнати для запису (°C, default 0.5)

  // Пороги для режимів форсаж та аварія
  float tempCriticalLow;       // Поріг для ФОРСАЖУ - критично низька температура (°C, default 20.0)
  float tempEmergencyLow;      // Поріг для АВАРІЇ - аварійна температура (°C, default 18.0)
};

struct HeatingState {
  bool active;
  uint8_t pumpPower;
  uint8_t fanPower;
  uint8_t extractorPower;
  bool forceMode;
  bool emergencyMode;
  bool manualMode;
  bool manualModeLocked;        // Блокування ручного режиму (не перемикається на авто)
  unsigned long manualModeStartTime;  // Час входу в ручний режим

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

  // Відстеження роботи вентилятора на максимумі
  unsigned long fan_max_power_start_time;  // Час початку роботи на максимумі
  float fan_max_power_initial_temp;        // Початкова температура при виході на максимум
};

struct VentilationState {
  bool open;
  int currentAngle;
  bool servoAttached;
  unsigned long lastMove;
  bool switchState;
  bool moving;
  bool calibrationMode;  // Режим калібрування - ігнорує вимикач
  
};

struct HumidifierState {
  bool active;
  unsigned long startTime;
  unsigned long lastCycle;
  float lastHumidity;
  uint8_t cyclesToday;
};

// Структура для моніторингу відключення зовнішнього живлення (аварія теплоносія)
struct PowerOutageState {
  bool detected;                    // Чи виявлено аварію
  unsigned long detectionTime;      // Час виявлення аварії
  float tempAtDetection;            // Температура теплоносія при виявленні
  uint8_t recoveryStage;            // Етап відновлення (0=немає, 1=перша спроба, 2=друга спроба, 3=відключення)
  unsigned long stageStartTime;     // Час початку поточного етапу
  float tempBeforeDrop;             // Температура до падіння (5 хв тому)
  unsigned long lastTempSave;       // Час останнього збереження температури
  bool emergencyHeatingActive;      // Чи активний аварійний обігрів
  unsigned long autoExitCheckStart; // Час початку перевірки автовиходу
  float tempAtAutoExitStart;        // Температура при початку перевірки автовиходу
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
extern PowerOutageState powerOutageState;
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
SemaphoreHandle_t getSyncMutex();
SemaphoreHandle_t getRamBufferMutex();

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

// Глобальні TaskHandle для OTA (призупинення задач)
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t heatingTaskHandle;
extern TaskHandle_t ventTaskHandle;
extern TaskHandle_t webTaskHandle;
extern TaskHandle_t timeTaskHandle;
extern TaskHandle_t advancedLogicTaskHandle;
extern TaskHandle_t dataLoggerTaskHandle;

// Функції призупинення/відновлення задач (для OTA)
void suspendAllTasks();
void resumeAllTasks();

#endif