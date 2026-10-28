#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "web_interface.h"
#include "data_storage.h"
#include "advanced_climate_logic.h"
#include <time.h>

// Оголошення функцій задач
extern void sensorTask(void *parameter);
extern void heatingTask(void *parameter);
extern void ventilationTask(void *parameter);
extern void webTask(void *parameter);
extern void timeTask(void *parameter);
extern void advancedLogicTask(void *parameter);

// Глобальні об'єкти
Preferences preferences;
WebServer server(80);

// М'ютекси для синхронізації
SemaphoreHandle_t sensorMutex;
SemaphoreHandle_t configMutex;
SemaphoreHandle_t heatingMutex;
SemaphoreHandle_t historyMutex;
SemaphoreHandle_t timeMutex;

// Статичні змінні (інкапсульовані в цьому файлі)
static unsigned long _lastStatusPrint = 0;
static unsigned long _lastSerialCheck = 0;
static unsigned long _lastTrendCheck = 0;
static unsigned long _lastHistorySave = 0;
static unsigned long _lastExtractorCheck = 0;

// Змінні для часу
time_t currentTime = 0;
struct tm timeInfo;
static bool timeSynced = false;
static unsigned long _lastTimeSync = 0;

// Буфери для тренду
float tempTrendBuffer[TREND_WINDOW_SIZE];
int trendIndex = 0;
static bool trendBufferFilled = false;

// Змінні для аварійного режиму
static unsigned long _emergencyStartTime = 0;
static float _emergencyStartTempCarrier = 0;
static float _emergencyStartTempRoom = 0;

// ============================================================================
// ГЕТТЕРИ ТА СЕТТЕРИ
// ============================================================================

unsigned long getLastStatusPrint() {
    return _lastStatusPrint;
}

void setLastStatusPrint(unsigned long time) {
    _lastStatusPrint = time;
}

unsigned long getLastSerialCheck() {
    return _lastSerialCheck;
}

void setLastSerialCheck(unsigned long time) {
    _lastSerialCheck = time;
}

unsigned long getLastTrendCheck() {
    return _lastTrendCheck;
}

void setLastTrendCheck(unsigned long time) {
    _lastTrendCheck = time;
}

unsigned long getLastHistorySave() {
    return _lastHistorySave;
}

void setLastHistorySave(unsigned long time) {
    _lastHistorySave = time;
}

unsigned long getLastExtractorCheck() {
    return _lastExtractorCheck;
}

void setLastExtractorCheck(unsigned long time) {
    _lastExtractorCheck = time;
}

unsigned long getLastTimeSync() {
    return _lastTimeSync;
}

void setLastTimeSync(unsigned long time) {
    _lastTimeSync = time;
}

unsigned long getEmergencyStartTime() {
    return _emergencyStartTime;
}

void setEmergencyStartTime(unsigned long time) {
    _emergencyStartTime = time;
}

float getEmergencyStartTempCarrier() {
    return _emergencyStartTempCarrier;
}

void setEmergencyStartTempCarrier(float temp) {
    _emergencyStartTempCarrier = temp;
}

float getEmergencyStartTempRoom() {
    return _emergencyStartTempRoom;
}

void setEmergencyStartTempRoom(float temp) {
    _emergencyStartTempRoom = temp;
}

// ============================================================================
// ФУНКЦІЇ ДЛЯ РОБОТИ З ЧАСОМ
// ============================================================================

void initTime() {
  Serial.println("Спробуємо синхронізувати час...");
  
  const char* ntpServers[] = {
    "pool.ntp.org",
    "time.google.com",
    "time.windows.com",
    "ntp.ubuntu.com",
    "0.ua.pool.ntp.org",
    "1.ua.pool.ntp.org",
    "2.ua.pool.ntp.org",
    "3.ua.pool.ntp.org"
  };
  
  configTime(2 * 3600, 0, ntpServers[0], ntpServers[1], ntpServers[2]);
  
  Serial.print("Синхронізація з NTP серверами...");
  
  int maxAttempts = 30;
  bool ntpSuccess = false;
  
  for (int i = 0; i < maxAttempts; i++) {
    if (getLocalTime(&timeInfo, 100)) {
      timeSynced = true;
      _lastTimeSync = millis();
      time(&currentTime);
      
      Serial.println("\n✓ Час синхронізовано через NTP!");
      Serial.printf("Сервер: %s\n", ntpServers[0]);
      Serial.printf("Отримано: %d-%02d-%02d %02d:%02d:%02d\n",
                    timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                    timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
      
      if (timeInfo.tm_year + 1900 >= 2024) {
        Serial.println("✓ Час валідний (після 2024 року)");
        ntpSuccess = true;
        break;
      } else {
        Serial.println("⚠️ NTP повернув старе значення часу, пробуємо інший сервер...");
        
        static int serverIndex = 1;
        configTime(2 * 3600, 0, 
          ntpServers[serverIndex % 8], 
          ntpServers[(serverIndex + 1) % 8], 
          ntpServers[(serverIndex + 2) % 8]);
        serverIndex++;
      }
    }
    
    delay(1000);
    Serial.print(".");
    
    if (i > 0 && i % 10 == 0) {
      Serial.printf("\nСпроба %d/30, міняю сервер...\n", i);
    }
  }
  
  if (!ntpSuccess) {
    Serial.println("\n⚠️ Всі NTP сервери не відповідають");
    
    struct tm tm;
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 2 - 1;
    tm.tm_mday = 18;
    tm.tm_hour = 14;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    
    time(&currentTime);
    localtime_r(&currentTime, &timeInfo);
    timeSynced = true;
    
    Serial.printf("✓ Локальний час встановлено: %s\n", getTimeString().c_str());
    Serial.println("ℹ️ Використайте веб-інтерфейс (/settime) для точного часу");
  }
}

void syncTime() {
  unsigned long now = millis();
  if (now - _lastTimeSync > NTP_UPDATE_INTERVAL) {
    if (getLocalTime(&timeInfo, 100)) {
      timeSynced = true;
      _lastTimeSync = now;
      time(&currentTime);
      
      if (xSemaphoreTake(timeMutex, portMAX_DELAY)) {
        struct tm timeinfo;
        localtime_r(&currentTime, &timeinfo);
        timeinfo.tm_isdst = -1;
        time_t adjustedTime = mktime(&timeinfo);
        struct timeval tv = {adjustedTime, 0};
        settimeofday(&tv, nullptr);
        xSemaphoreGive(timeMutex);
      }
      
      Serial.println("✓ Час оновлено");
    }
  }
}

String getFormattedTime() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char timeStr[20];
    if (config.use24hFormat) {
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    } else {
      strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", &timeinfo);
    }
    
    xSemaphoreGive(timeMutex);
    return String(timeStr);
  }
  return "Помилка";
}

String getDateString() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);
    
    xSemaphoreGive(timeMutex);
    return String(dateStr);
  }
  return "Помилка дати";
}

String getTimeString() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", &timeinfo);
    
    xSemaphoreGive(timeMutex);
    return String(timeStr);
  }
  return "Помилка часу";
}

// ============================================================================
// ФУНКЦІЇ КОНФІГУРАЦІЇ
// ============================================================================

void loadConfiguration() {
  preferences.begin("climate", false);
  
  config.pumpDisabled = preferences.getBool("pumpDisabled", false);
  config.tempMin = preferences.getFloat("tempMin", TEMP_MIN_THRESHOLD);
  config.tempMax = preferences.getFloat("tempMax", TEMP_MAX_THRESHOLD);
  config.tempVentMin = preferences.getFloat("tempVentMin", TEMP_VENT_MIN);
  config.tempVentMax = preferences.getFloat("tempVentMax", TEMP_VENT_MAX);
  config.heatingEnabled = preferences.getBool("heating", true);
  config.humidifierEnabled = preferences.getBool("humidifier", true);
  config.statusPeriod = preferences.getULong("statusPeriod", STATUS_PRINT_INTERVAL);
  config.fanMinPercent = preferences.getUChar("fanMin", 10);
  config.manualVentControl = preferences.getBool("manualVent", false);
  config.use24hFormat = preferences.getBool("use24h", true);
  
  // Обмеження пристроїв
  config.pumpMinPercent = preferences.getUChar("pumpMin", PUMP_MIN_DEFAULT);
  config.pumpMaxPercent = preferences.getUChar("pumpMax", PUMP_MAX_DEFAULT);
  config.fanMaxPercent = preferences.getUChar("fanMax", FAN_MAX_DEFAULT);
  config.extractorMinPercent = preferences.getUChar("extractorMin", EXTRACTOR_MIN_DEFAULT);
  config.extractorMaxPercent = preferences.getUChar("extractorMax", EXTRACTOR_MAX_DEFAULT);
  
  // Налаштування вологості
  config.humidityConfig.minHumidity = preferences.getFloat("humMin", HUM_MIN_DEFAULT);
  config.humidityConfig.maxHumidity = preferences.getFloat("humMax", HUM_MAX_DEFAULT);
  config.humidityConfig.tempCoefficient = preferences.getFloat("humCoeff", HUM_TEMP_COEFF);
  config.humidityConfig.enabled = preferences.getBool("humCtrl", true);
  config.humidityConfig.minInterval = preferences.getULong("humInt", HUMIDIFIER_MIN_INTERVAL);
  config.humidityConfig.maxRunTime = preferences.getULong("humMaxRun", HUMIDIFIER_MAX_RUN_TIME);
  config.humidityConfig.hysteresis = preferences.getUChar("humHyst", 3);
  config.humidityConfig.adaptiveMode = preferences.getBool("humAdapt", true);
  
  // Налаштування адаптивного обігріву
  config.a_adaptive_min = preferences.getUChar("aAdaptMin", 30);
  config.a_adaptive_max = preferences.getUChar("aAdaptMax", 45);
  config.a_normal_range_min = preferences.getUChar("aNormMin", 30);
  config.a_normal_range_max = preferences.getUChar("aNormMax", 35);
  config.b_start_percent = preferences.getUChar("bStart", 32);
  config.trend_window_seconds = preferences.getUShort("trendWindow", 300);
  config.temp_drop_threshold = preferences.getFloat("dropThresh", 2.0f);
  config.heating_check_interval = preferences.getULong("heatCheck", 300000);
  config.adaptive_temp_step = preferences.getUChar("tempStep", 1);
  config.adaptive_hum_step = preferences.getUChar("humStep", 1);
  
  // Таймер витяжки
  config.extractorTimer.enabled = preferences.getBool("extTimer", false);
  config.extractorTimer.onMinutes = preferences.getUShort("extOnMin", 30);
  config.extractorTimer.offMinutes = preferences.getUShort("extOffMin", 30);
  config.extractorTimer.powerPercent = preferences.getUChar("extPower", 70);
  config.extractorTimer.state = false;
  config.extractorTimer.cycleStart = 0;
  config.extractorTimer.lastChange = 0;
  
  config.history_size = preferences.getUShort("historySize", 1440);
  
  preferences.end();
  
  // Ініціалізація буферів
  for (int i = 0; i < TREND_WINDOW_SIZE; i++) {
    tempTrendBuffer[i] = 0.0f;
  }
  
  // Скидання аварійних змінних
  _emergencyStartTime = 0;
  _emergencyStartTempCarrier = 0;
  _emergencyStartTempRoom = 0;
  
  Serial.println("=== Налаштування завантажено ===");
}

void saveConfiguration() {
  preferences.begin("climate", false);
  
  preferences.putBool("pumpDisabled", config.pumpDisabled);
  preferences.putFloat("tempMin", config.tempMin);
  preferences.putFloat("tempMax", config.tempMax);
  preferences.putFloat("tempVentMin", config.tempVentMin);
  preferences.putFloat("tempVentMax", config.tempVentMax);
  preferences.putBool("heating", config.heatingEnabled);
  preferences.putBool("humidifier", config.humidifierEnabled);
  preferences.putULong("statusPeriod", config.statusPeriod);
  preferences.putUChar("fanMin", config.fanMinPercent);
  preferences.putBool("manualVent", config.manualVentControl);
  preferences.putBool("use24h", config.use24hFormat);
  
  // Обмеження пристроїв
  preferences.putUChar("pumpMin", config.pumpMinPercent);
  preferences.putUChar("pumpMax", config.pumpMaxPercent);
  preferences.putUChar("fanMax", config.fanMaxPercent);
  preferences.putUChar("extractorMin", config.extractorMinPercent);
  preferences.putUChar("extractorMax", config.extractorMaxPercent);
  
  // Налаштування вологості
  preferences.putFloat("humMin", config.humidityConfig.minHumidity);
  preferences.putFloat("humMax", config.humidityConfig.maxHumidity);
  preferences.putFloat("humCoeff", config.humidityConfig.tempCoefficient);
  preferences.putBool("humCtrl", config.humidityConfig.enabled);
  preferences.putULong("humInt", config.humidityConfig.minInterval);
  preferences.putULong("humMaxRun", config.humidityConfig.maxRunTime);
  preferences.putUChar("humHyst", config.humidityConfig.hysteresis);
  preferences.putBool("humAdapt", config.humidityConfig.adaptiveMode);
  
  // Налаштування адаптивного обігріву
  preferences.putUChar("aAdaptMin", config.a_adaptive_min);
  preferences.putUChar("aAdaptMax", config.a_adaptive_max);
  preferences.putUChar("aNormMin", config.a_normal_range_min);
  preferences.putUChar("aNormMax", config.a_normal_range_max);
  preferences.putUChar("bStart", config.b_start_percent);
  preferences.putUShort("trendWindow", config.trend_window_seconds);
  preferences.putFloat("dropThresh", config.temp_drop_threshold);
  preferences.putULong("heatCheck", config.heating_check_interval);
  preferences.putUChar("tempStep", config.adaptive_temp_step);
  preferences.putUChar("humStep", config.adaptive_hum_step);
  
  // Таймер витяжки
  preferences.putBool("extTimer", config.extractorTimer.enabled);
  preferences.putUShort("extOnMin", config.extractorTimer.onMinutes);
  preferences.putUShort("extOffMin", config.extractorTimer.offMinutes);
  preferences.putUChar("extPower", config.extractorTimer.powerPercent);
  
  preferences.putUShort("historySize", config.history_size);
  
  preferences.end();
  
  Serial.println("✓ Налаштування збережено");
}

// ============================================================================
// ФУНКЦІЇ ІНІЦІАЛІЗАЦІЇ
// ============================================================================

void initMutexes() {
  sensorMutex = xSemaphoreCreateMutex();
  configMutex = xSemaphoreCreateMutex();
  heatingMutex = xSemaphoreCreateMutex();
  historyMutex = xSemaphoreCreateMutex();
  timeMutex = xSemaphoreCreateMutex();
  
  if (sensorMutex == NULL || configMutex == NULL || 
      heatingMutex == NULL || historyMutex == NULL || 
      timeMutex == NULL) {
    Serial.println("⚠️ Помилка створення м'ютексів!");
  } else {
    Serial.println("✓ М'ютекси створені");
  }
}

void createTasks() {
  TaskHandle_t sensorTaskHandle = NULL;
  TaskHandle_t heatingTaskHandle = NULL;
  TaskHandle_t ventTaskHandle = NULL;
  TaskHandle_t webTaskHandle = NULL;
  TaskHandle_t timeTaskHandle = NULL;
  TaskHandle_t advancedLogicTaskHandle = NULL;
  
  xTaskCreatePinnedToCore(
    sensorTask,
    "SensorTask",
    8192,
    NULL,
    2,
    &sensorTaskHandle,
    0
  );
  
  xTaskCreatePinnedToCore(
    heatingTask,
    "HeatingTask",
    8192,
    NULL,
    3,
    &heatingTaskHandle,
    1
  );
  
  xTaskCreatePinnedToCore(
    ventilationTask,
    "VentilationTask",
    4096,
    NULL,
    1,
    &ventTaskHandle,
    1
  );
  
  xTaskCreatePinnedToCore(
    webTask,
    "WebTask",
    8192,
    NULL,
    1,
    &webTaskHandle,
    0
  );
  
  xTaskCreatePinnedToCore(
    timeTask,
    "TimeTask",
    4096,
    NULL,
    1,
    &timeTaskHandle,
    0
  );
  
  xTaskCreatePinnedToCore(
    advancedLogicTask,
    "AdvLogicTask",
    16384,
    NULL,
    3,
    &advancedLogicTaskHandle,
    1
  );
  
  Serial.println("✓ Задачі FreeRTOS створені");
  delay(100);
}

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================================

bool isTimeSynced() {
  return timeSynced;
}

void setTimeSynced(bool synced) {
  timeSynced = synced;
}

struct tm* getTimeInfo() {
  return &timeInfo;
}

void updateTimeInfo() {
  time(&currentTime);
  localtime_r(&currentTime, &timeInfo);
}

// ============================================================================
// ФУНКЦІЇ ДЛЯ ТРЕНДУ ТЕМПЕРАТУРИ
// ============================================================================

float* getTempTrendBufferPtr() {
    return tempTrendBuffer;
}

int getTrendIndexValue() {
    return trendIndex;
}

void setTrendIndex(int index) {
    trendIndex = index;
}

bool isTrendBufferFilled() {
    return trendBufferFilled;
}

void setTrendBufferFilled(bool filled) {
    trendBufferFilled = filled;
}

// ============================================================================
// ФУНКЦІЇ ДЛЯ ДОСТУПУ ДO М'ЮТЕКСІВ
// ============================================================================

SemaphoreHandle_t getSensorMutex() {
    return sensorMutex;
}

SemaphoreHandle_t getConfigMutex() {
    return configMutex;
}

SemaphoreHandle_t getHeatingMutex() {
    return heatingMutex;
}

SemaphoreHandle_t getHistoryMutex() {
    return historyMutex;
}

SemaphoreHandle_t getTimeMutex() {
    return timeMutex;
}

// ============================================================================
// ФУНКЦІЇ ДЛЯ ДОСТУПУ ДО ЧАСУ
// ============================================================================

time_t getCurrentTime() {
    return currentTime;
}

void setCurrentTime(time_t t) {
    currentTime = t;
}

void checkEmergencyTimeout() {
    if (heatingState.emergencyMode && getEmergencyStartTime() > 0) {
        unsigned long now = millis();
        if (now - getEmergencyStartTime() > 120000) { // 2 хвилини
            float tempCarrierDiff = sensorData.tempCarrier - getEmergencyStartTempCarrier();
            float tempRoomDiff = sensorData.tempRoom - getEmergencyStartTempRoom();
            
            if (tempCarrierDiff < 2.0f && tempRoomDiff < 2.0f) {
                setHeatingPower(0, 0, 0);
                digitalWrite(HUMIDIFIER_PIN, LOW);
                heatingState.emergencyMode = false;
                
                Serial.println("🚨 АВАРІЙНИЙ РЕЖИМ: вимкнено через відсутність прогріву!");
                Serial.printf("  Збільшення температури: теплоносій +%.1f°C, кімната +%.1f°C\n", 
                              tempCarrierDiff, tempRoomDiff);
                
                setEmergencyStartTime(0);
                setEmergencyStartTempCarrier(0);
                setEmergencyStartTempRoom(0);
            } else {
                setEmergencyStartTime(now);
                setEmergencyStartTempCarrier(sensorData.tempCarrier);
                setEmergencyStartTempRoom(sensorData.tempRoom);
            }
        }
    }
}