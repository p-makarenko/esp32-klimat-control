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
  float co2Level;         // Рівень CO2 з SCD30 (ppm)
  bool carrierValid;
  bool roomValid;
  bool bmeValid;
  bool co2Valid;          // Валідність даних CO2
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

struct PlantFanTimer {
  bool     enabled;         // Таймер увімкнено
  bool     breezeEnabled;   // Природній вітер під час ON фази
  uint16_t onMinutes;
  uint16_t onSeconds;
  uint16_t offMinutes;
  uint16_t offSeconds;
  uint8_t  powerPercent;    // Потужність (без вітру)
  uint8_t  minStartPercent; // Мінімальна швидкість запуску (апаратне обмеження)
};

struct BreezeConfig {
  uint8_t  baseSpeedMin;  // % мінімальна базова швидкість
  uint8_t  baseSpeedMax;  // % максимальна базова швидкість
  uint8_t  gustBoost;     // % підсилення пориву відносно бази
  uint16_t gustMinSec;    // сек мінімальна тривалість пориву
  uint16_t gustMaxSec;    // сек максимальна тривалість пориву
  uint16_t calmMinSec;    // сек мінімальне затишшя
  uint16_t calmMaxSec;    // сек максимальне затишшя
};

// ============================================================================
// ПЛАВНА РОБОТА ВЕНТИЛЯТОРА ОБІГРІВУ (У ВСІХ РЕЖИМАХ)
// ============================================================================
struct FanSmoothConfig {
  bool     enabled;              // Увімкнути плавний режим (антициклювання + плавний ШІМ)
  float    hysteresis;           // Гістерезис вимкнення/пуску біля порогу (°C, default: 0.5)
  uint8_t  maxStepPerSec;        // Макс. зміна швидкості %/сек (default: 5)
  uint16_t minRunSec;            // Мін. час роботи перед зупинкою (сек, default: 60)
  uint16_t minOffSec;            // Мін. час паузи перед повторним пуском (сек, default: 60)
  bool     keepLowInsteadOfStop; // Тримати мінімум замість стопу до tempMax + hyst
};

// ============================================================================
// РОЗУМНЕ КЕРУВАННЯ НАСОСОМ (ЗА ШВИДКІСТЮ НАГРІВУ КІМНАТИ)
// ============================================================================
struct SmartPumpConfig {
  bool     enabled;          // Увімкнути розумний насос за швидкістю нагріву (default: true)
  uint8_t  workPercent;      // Робоча потужність насоса в штатному режимі (%, default: 33)
  uint16_t evalWindowSec;    // Вікно оцінки швидкості нагріву кімнати (сек, default: 180)
  float    minTempRise;      // Мін. приріст T кімнати за вікно, щоб НЕ вмикати насос (°C, default: 0.15)
  float    carrierStopTemp;  // Вимкнути насос, коли теплоносій прогріється до (°C, default: 36.0; 0=до tempMax)
  float    carrierLowTemp;   // Поріг теплоносія для простого режиму (коли розумний вимк, °C, default: 30.0)
  uint16_t minRunSec;        // Мін. час роботи насоса після пуску (сек, default: 60)
  uint16_t minOffSec;        // Мін. пауза після зупинки насоса (сек, default: 120)
};

// ============================================================================
// ЕНЕРГОЗБЕРЕЖЕННЯ / ПРОФІЛЬ БЛЕКАУТУ (ECO / UPS MODE)
// ============================================================================
enum EcoActivationMode : uint8_t {
  ECO_MODE_AUTO     = 0,  // Автоматично при зникненні 220В (PZEM)
  ECO_MODE_FORCE_ON = 1,  // Примусово увімкнено (ручний ЕКО)
  ECO_MODE_DISABLED = 2   // Вимкнено (ігнорувати відключення 220В)
};

enum EcoDevicePolicy : uint8_t {
  ECO_DEV_OFF       = 0,  // Повністю вимкнути (0%)
  ECO_DEV_LIMITED   = 1,  // ЕКО / обмежений режим
  ECO_DEV_UNCHANGED = 2   // Без змін (штатний режим)
};

struct PowerSaveConfig {
  uint8_t  activationMode;       // EcoActivationMode (0=AUTO, 1=FORCE_ON, 2=DISABLED)
  uint16_t outageConfirmSec;     // Підтвердження зникнення 220В (сек, default: 6)
  uint16_t restoreConfirmSec;    // Стабілізація після повернення 220В (сек, default: 15)

  // Обігрів при блекауті
  uint8_t  heatingPolicy;        // 0=OFF, 1=ECO-обігрів, 2=Штатний (default: 1)
  float    ecoTempMin;           // Цільова мін. температура при блекауті (°C, default: 20.0)
  float    ecoTempHyst;          // Гістерезис обігріву при блекауті (°C, default: 0.8)
  uint8_t  ecoPumpMaxPercent;    // Стелі потужності насоса при блекауті (%, default: 40)
  uint8_t  ecoFanMaxPercent;     // Стелі потужності вентилятора при блекауті (%, default: 30)
  bool     ecoCheckCarrierTemp;  // Не крутити вхолосту, якщо теплоносій холодний
  float    ecoCarrierMinDelta;   // Мін. перевищення теплоносія над кімнатою (°C, default: 3.0)
  bool     ecoAllowHeaterRelay;  // Дозволити реле 220В вентилятора при блекауті (default: false)
  bool     ecoPumpPulseMode;     // Імпульсний режим насоса при блекауті (default: false)
  uint16_t ecoPumpPulseOnSec;    // Імпульс насоса ON (сек, default: 120)
  uint16_t ecoPumpPulseOffSec;   // Імпульс насоса OFF (сек, default: 300)

  // Витяжка при блекауті
  uint8_t  extractorPolicy;      // EcoDevicePolicy: 0=OFF, 1=ЕКО-таймер, 2=Без змін
  uint16_t ecoExtOnMin;          // ЕКО-таймер витяжки: робота (хв, default: 2)
  uint16_t ecoExtOffMin;         // ЕКО-таймер витяжки: пауза (хв, default: 58)
  uint8_t  ecoExtPower;          // ЕКО-таймер витяжки: потужність (%, default: 25)

  // Вітер (вентилятор рослин) при блекауті
  uint8_t  plantFanPolicy;       // EcoDevicePolicy: 0=OFF, 1=ЕКО-мінімум/таймер, 2=Без змін
  uint8_t  ecoPlantFanPower;     // Фіксована потужність вітру в ЕКО без поривів (%, default: 20)
  uint16_t ecoPlantFanOnMin;     // ЕКО-таймер вітру: робота (хв, default: 5)
  uint16_t ecoPlantFanOffMin;    // ЕКО-таймер вітру: пауза (хв, default: 55)

  // Зволожувач при блекауті
  uint8_t  humidifierPolicy;     // 0=OFF, 1=ЕКО-пороги вологості, 2=Без змін (default: 0)
  float    ecoHumMin;            // ЕКО мін. вологість увімкнення (%, default: 45.0)
  float    ecoHumMax;            // ЕКО макс. вологість вимкнення (%, default: 50.0)

  // Освітлення DMX при блекауті
  uint8_t  lightPolicy;          // 0=OFF (0%), 1=Приглушити %, 2=Без змін (default: 0)
  uint8_t  ecoLightScalePercent; // Масштаб яскравості master в ЕКО (%, default: 25)

  // Фонові сервіси
  bool     pauseSheetsSync;      // Пауза Google Sheets під час блекауту (default: true)
  bool     pauseAdaptiveThresh;  // Пауза адаптивних порогів під час блекауту (default: true)
};

struct SystemConfig {
  float tempMin;
  float tempMax;
  float tempVentMin;
  float tempVentMax;
  bool heatingEnabled;
  bool heaterFanRelayEnabled;   // Прапорець дозволу роботи реле 220В вентилятора (Pin 7)
  bool seasonalHeatingDisable;  // Автоматичне відключення обігріву в теплі місяці
  bool coolingMode;              // Режим охолодження (літо: холодна вода в теплоносії)
  bool humidifierEnabled;
  bool pumpDisabled = false; // Повне блокування/вимкнення насосу
  uint32_t statusPeriod;
  uint8_t fanMinPercent;
  uint8_t fanMaxPercent;
  uint8_t pumpMinPercent;
  uint8_t pumpMaxPercent;
  uint8_t pumpStartPercent;      // Пускова потужність насоса при старті з 0% (%, default: 45)
  uint8_t pumpStartDurationSec;  // Тривалість пускового імпульсу насоса (сек, default: 2, 0=вимк)
  uint8_t extractorMinPercent;
  uint8_t extractorMaxPercent;
  bool manualVentControl;
  bool use24hFormat;

  // Налаштування адаптивного обігріву
  uint8_t a_adaptive_min;
  uint8_t a_adaptive_max;
  uint8_t a_normal_range_min;
  uint8_t a_normal_range_max;
  uint8_t b_start_percent;
  uint16_t trend_window_seconds;
  float temp_drop_threshold;
  uint32_t heating_check_interval;
  
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
  
  uint8_t adaptive_temp_step;
  uint8_t adaptive_hum_step;
  uint16_t adaptive_activation_time_min;
  uint16_t adaptive_recovery_time_min;

  ExtractorTimer extractorTimer;
  PlantFanTimer  plantFanTimer;
  BreezeConfig   breezeConfig;
  uint16_t history_size;

  // SCD30 Датчик CO2
  uint16_t scd30MeasurementInterval;  // Інтервал вимірювання в секундах (2-1800)

  // Інтервал запису в SPIFFS (хвилини)
  uint8_t spiffsLogInterval;   // 1-60 хв, default 5

  // Поріг логування даних (для економії пакетів)
  float logTempThreshold;      // Мінімальна зміна температури кімнати для запису (°C, default 0.5)

  // Інтервал відправки в Google Sheets (хвилини, навіть без зміни температури)
  uint16_t sheetsSyncIntervalMin; // 1-1440 хв, default 15

  // Пороги для режимів форсаж та аварія
  float tempCriticalLow;       // Поріг для ФОРСАЖУ - критично низька температура (°C, default 20.0)
  float tempEmergencyLow;      // Поріг для АВАРІЇ - аварійна температура (°C, default 18.0)

  // Плавна робота вентилятора обігріву, розумний насос та профіль енергозбереження
  FanSmoothConfig fanSmooth;
  SmartPumpConfig smartPump;
  PowerSaveConfig powerSave;
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
  unsigned long lastCycle;
  float lastHumidity;
  uint8_t cyclesToday;
};

// Структура для моніторингу відключення зовнішнього живлення (по напрузі PZEM) та ЕКО-режиму
struct PowerOutageState {
  bool detected;                    // Чи підтверджено відсутність 220В
  unsigned long detectionTime;      // Час підтвердження відключення 220В
  bool emergencyHeatingActive;      // Чи активний ЕКО/аварійний обігрів при блекауті
  float lastVoltage;                // Остання зчитана напруга (В)
  bool ecoActive;                   // Чи активний зараз ЕКО-профіль (авто або ручний)
  unsigned long ecoStartTime;       // Час активації ЕКО-профілю
  unsigned long outagePendingSince; // Початок очікування підтвердження зникнення 220В
  unsigned long restorePendingSince;// Початок очікування підтвердження повернення 220В
  bool ecoHeatingOn;                // Поточний стан гістерезису обігріву в ЕКО
};

struct HistoryData {
  unsigned long timestamp;
  float tempCarrier;
  float tempRoom;
  float tempBME;
  float humidity;
  float pressure;
  float co2Level;         // Рівень CO2 від SCD30 (ppm)
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

// Буфери для тренду температури
#ifndef TREND_WINDOW_SIZE
#define TREND_WINDOW_SIZE 300
#endif
extern float tempTrendBuffer[TREND_WINDOW_SIZE];
extern int trendIndex;

// Прототипи функцій
void loadConfiguration();
void saveConfiguration();
void loadPowerSaveConfig();
void savePowerSaveConfig();
bool isEcoModeActive();
void applyEcoPreset(uint8_t presetId);
void initMutexes();
void createTasks();
void resetNetworkSettings();
void initTime();
void printMenu();
void syncTime();
String getTimeString();
String getFormattedTime();
String getDateString();

// Геттери та сеттери для тренду
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
void advancedLogicTask(void *parameter);

// Глобальні TaskHandle для OTA (призупинення задач)
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t heatingTaskHandle;
extern TaskHandle_t ventTaskHandle;
extern TaskHandle_t webTaskHandle;
extern TaskHandle_t timeTaskHandle;
extern TaskHandle_t advancedLogicTaskHandle;
extern TaskHandle_t dataLoggerTaskHandle;
extern TaskHandle_t plantFanTaskHandle;
extern TaskHandle_t dmxTaskHandle;
extern TaskHandle_t lightSchedulerTaskHandle;
extern TaskHandle_t loopTaskHandle;

// Функції призупинення/відновлення задач (для OTA)
void suspendAllTasks();
void resumeAllTasks();

#endif