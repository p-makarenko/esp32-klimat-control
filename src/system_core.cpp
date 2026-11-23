
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "web_interface.h"
#include "data_storage.h"
#include "data_logger.h"
#include "advanced_climate_logic.h"
#include <time.h>

// Оголошення функцій задач
extern void sensorTask(void *parameter);
extern void heatingTask(void *parameter);
extern void ventilationTask(void *parameter);
extern void webTask(void *parameter);
extern void timeTask(void *parameter);
extern void advancedLogicTask(void *parameter);
extern void plantFanTask(void *parameter);

float tempTrendBuffer[TREND_WINDOW_SIZE];
// Глобальні об'єкти
Preferences preferences;
WebServer server(80);

// Глобальні TaskHandle для можливості призупинення під час OTA
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t heatingTaskHandle = NULL;
TaskHandle_t ventTaskHandle = NULL;
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t timeTaskHandle = NULL;
TaskHandle_t advancedLogicTaskHandle = NULL;
TaskHandle_t dataLoggerTaskHandle = NULL;
TaskHandle_t plantFanTaskHandle = NULL;

// Мʼютекси для синхронізації
SemaphoreHandle_t sensorMutex;
SemaphoreHandle_t configMutex;
SemaphoreHandle_t heatingMutex;
SemaphoreHandle_t historyMutex;
SemaphoreHandle_t timeMutex;
SemaphoreHandle_t syncMutex;
SemaphoreHandle_t ramBufferMutex;

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


// Додано: змінні для аварійного режиму
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

// Додано: геттери/сеттери для аварійного режиму
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

// Очистити неправильні мережеві налаштування
void resetNetworkSettings() {
  preferences.begin("climate", false);
  preferences.putBool("useStaticIP", false);
  preferences.putString("staticIP", "");
  preferences.putString("gateway", "");
  preferences.putString("subnet", "255.255.255.0");
  preferences.putString("dns", "8.8.8.8");
  preferences.end();
  config.useStaticIP = false;
  config.staticIP = "";
  config.gateway = "";
  config.subnet = "255.255.255.0";
  config.dns = "8.8.8.8";
  Serial.println("✅ Мережеві налаштування скинуті на DHCP");
}

void initTime() {
  // Serial.println("Спробуємо синхронізувати час...");  // RUS_REMOVED
  
  // Используем несколько NTP серверов для надежности
  const char* ntpServers[] = {
    "pool.ntp.org",           // Основной пул
    "time.google.com",        // Google (очень точный)
    "time.windows.com",       // Microsoft
    "ntp.ubuntu.com",         // Ubuntu
    "0.ua.pool.ntp.org",      // Украинский сервер 1
    "1.ua.pool.ntp.org",      // Украинский сервер 2
    "2.ua.pool.ntp.org",      // Украинский сервер 3
    "3.ua.pool.ntp.org"       // Украинский сервер 4
  };
  
  // POSIX timezone для Києва — автоматичне переключення літній/зимовий час
  configTime(0, 0, ntpServers[0], ntpServers[1], ntpServers[2]);
  setenv("TZ", TZ_INFO, 1);
  tzset();
  
  // Serial.print("Синхронізація з NTP серверами...");  // RUS_REMOVED
  
  int maxAttempts = 30;  // 30 секунд ожидания
  bool ntpSuccess = false;
  
  for (int i = 0; i < maxAttempts; i++) {
    if (getLocalTime(&timeInfo, 100)) {
      timeSynced = true;
      _lastTimeSync = millis();
      time(&currentTime);
      
      // Serial.println("\n✓ Час синхронізовано через NTP!");  // RUS_REMOVED
      Serial.print("Сервер: ");
      Serial.println(ntpServers[0]);
      Serial.printf("Отримано: %d-%02d-%02d %02d:%02d:%02d\n",
                    timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                    timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
      
      // Проверяем что время адекватное (не в прошлом веке)
      if (timeInfo.tm_year + 1900 >= 2024) {
        // Serial.println("✓ Час валідний (після 2024 року)");  // RUS_REMOVED
        ntpSuccess = true;
        break;
      } else {
        // Serial.println("⚠ NTP повернув старе час, пробуємо інший сервер...");  // RUS_REMOVED
        
        // Пробуем следующий сервер
        static int serverIndex = 1;
        configTime(0, 0,
          ntpServers[serverIndex % 8],
          ntpServers[(serverIndex + 1) % 8],
          ntpServers[(serverIndex + 2) % 8]);
        serverIndex++;
      }
      // Устанавливаем примерное время вручную
    }
    
    delay(1000);
    Serial.print(".");
    
    // Каждые 10 секунд меняем сервер
    if (i > 0 && i % 10 == 0) {
      Serial.printf("\nСпроба %d/30, міняю сервер...\n", i);
    }
  }
  
  if (!ntpSuccess) {
    // Serial.println("\n⚠ Всі NTP сервери не відповідають");  // RUS_REMOVED
    
    // 
    struct tm tm;
    tm.tm_year = 2024 - 1900;   // 2024 год
    tm.tm_mon = 2 - 1;          // Февраль (если сейчас февраль)
    tm.tm_mday = 18;            // 18 число
    tm.tm_hour = 14;            // 14 часов
    tm.tm_min = 0;              // 0 минут
    tm.tm_sec = 0;              // 0 секунд
    tm.tm_isdst = -1;
    
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    
    time(&currentTime);
    localtime_r(&currentTime, &timeInfo);
    timeSynced = true;
    
    Serial.printf("✓ Локальний час встановлено: %s\n", getTimeString().c_str());
    // Serial.println("ℹ Використайте веб-інтерфейс (/settime) для точного часу");  // RUS_REMOVED
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
        timeinfo.tm_isdst = -1; // Автовизначення літнього часу
        time_t adjustedTime = mktime(&timeinfo);
        struct timeval tv = {adjustedTime, 0};
        settimeofday(&tv, nullptr);
        xSemaphoreGive(timeMutex);
      }
      
      // Serial.println("✓ Час оновлено");  // RUS_REMOVED
    }
  }
}

String getFormattedTime() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    // Получаем РЕАЛЬНОЕ текущее время
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
    
    // Получаем РЕАЛЬНОЕ текущее время
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
    
    // Получаем РЕАЛЬНОЕ текущее время
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
  config.heaterFanRelayEnabled = preferences.getBool("htrFanRelay", HEATER_FAN_RELAY_ENABLED_DEFAULT);
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

  // Servo calibration
  config.servoClosedAngle = preferences.getInt("servoClosed", SERVO_CLOSED_ANGLE);
  config.servoOpenAngle = preferences.getInt("servoOpen", SERVO_OPEN_ANGLE);
  config.servoSpeed = preferences.getInt("servoSpeed", 15);
  config.autoStatusEnabled = preferences.getBool("autoStatus", true);
  
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
  config.adaptive_activation_time_min = preferences.getUShort("adpActMin", 30);
  config.adaptive_recovery_time_min = preferences.getUShort("adpRecMin", 60);
  
  // Таймер витяжки
  config.extractorTimer.enabled = preferences.getBool("extTimer", false);
  config.extractorTimer.onMinutes = preferences.getUShort("extOnMin", 30);
  config.extractorTimer.onSeconds = preferences.getUShort("extOnSec", 0);
  config.extractorTimer.offMinutes = preferences.getUShort("extOffMin", 30);
  config.extractorTimer.offSeconds = preferences.getUShort("extOffSec", 0);
  config.extractorTimer.powerPercent = preferences.getUChar("extPower", 70);
  config.extractorTimer.state = false;
  config.extractorTimer.cycleStart = 0;
  config.extractorTimer.lastChange = 0;
  
  config.history_size = preferences.getUShort("historySize", 1440);

  // Сезонне відключення та режим охолодження
  config.seasonalHeatingDisable = preferences.getBool("seasonalDisable", false);
  config.coolingMode = preferences.getBool("coolingMode", false);

  // SCD30 Датчик CO2
  config.scd30MeasurementInterval = preferences.getUShort("scd30Interval", 2);

  // Інтервал запису в SPIFFS
  config.spiffsLogInterval = preferences.getUChar("spiffsLogInt", 5);

  // Поріг логування даних
  config.logTempThreshold = preferences.getFloat("logTempThresh", 0.5f);

  // Інтервал відправки в Google Sheets
  config.sheetsSyncIntervalMin = preferences.getUShort("sheetsIntMin", 15);

  // Пороги для режимів форсаж та аварія
  config.tempCriticalLow = preferences.getFloat("tempCritLow", TEMP_CRITICAL_LOW);
  config.tempEmergencyLow = preferences.getFloat("tempEmergLow", TEMP_EMERGENCY_LOW);

  // Мережеві налаштування
  config.useStaticIP = preferences.getBool("useStaticIP", false);
  config.staticIP = preferences.getString("staticIP", "");
  config.gateway = preferences.getString("gateway", "");
  config.subnet = preferences.getString("subnet", "");
  config.dns = preferences.getString("dns", "");

  // Автентифікація
  config.useAuth = preferences.getBool("useAuth", false);
  config.authLogin = preferences.getString("authLogin", "admin");
  config.authPassword = preferences.getString("authPass", "admin");
  
  preferences.end();
  
  // Ініціалізація буферів
  for (int i = 0; i < TREND_WINDOW_SIZE; i++) {
    tempTrendBuffer[i] = 0.0f;
  }
  
  // Скидання аварійних змінних
  setEmergencyStartTime(0);
  setEmergencyStartTempCarrier(0);
  setEmergencyStartTempRoom(0);
  
  Serial.println("=== Налаштування завантажено ===");
}

void saveConfiguration() {
  Serial.println("\n💾 DEBUG saveConfiguration() - ЗБЕРЕЖЕННЯ НАЛАШТУВАНЬ:");

  preferences.begin("climate", false);

  // ⛔ Повне блокування/вимкнення насосу та реле вентилятора
  preferences.putBool("pumpDisabled", config.pumpDisabled);
  preferences.putBool("htrFanRelay", config.heaterFanRelayEnabled);

  Serial.print("   tempMin: ");
  Serial.println(config.tempMin);
  Serial.print("   tempMax: ");
  Serial.println(config.tempMax);

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
  
  // Servo calibration
  preferences.putInt("servoClosed", config.servoClosedAngle);
  preferences.putInt("servoOpen", config.servoOpenAngle);
  preferences.putInt("servoSpeed", config.servoSpeed);
  
  preferences.putBool("autoStatus", config.autoStatusEnabled);

  // Налаштування вологості
  Serial.print("   humMin: ");
  Serial.println(config.humidityConfig.minHumidity);
  Serial.print("   humMax: ");
  Serial.println(config.humidityConfig.maxHumidity);

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
  preferences.putUShort("adpActMin", config.adaptive_activation_time_min);
  preferences.putUShort("adpRecMin", config.adaptive_recovery_time_min);
  
  // Таймер витяжки
  preferences.putBool("extTimer", config.extractorTimer.enabled);
  preferences.putUShort("extOnMin", config.extractorTimer.onMinutes);
  preferences.putUShort("extOnSec", config.extractorTimer.onSeconds);
  preferences.putUShort("extOffMin", config.extractorTimer.offMinutes);
  preferences.putUShort("extOffSec", config.extractorTimer.offSeconds);
  preferences.putUChar("extPower", config.extractorTimer.powerPercent);
  
  preferences.putUShort("historySize", config.history_size);

  // Сезонне відключення та режим охолодження
  preferences.putBool("seasonalDisable", config.seasonalHeatingDisable);
  preferences.putBool("coolingMode", config.coolingMode);

  // SCD30 Датчик CO2
  preferences.putUShort("scd30Interval", config.scd30MeasurementInterval);

  // Інтервал запису в SPIFFS
  preferences.putUChar("spiffsLogInt", config.spiffsLogInterval);

  // Поріг логування даних
  preferences.putFloat("logTempThresh", config.logTempThreshold);

  // Інтервал відправки в Google Sheets
  preferences.putUShort("sheetsIntMin", config.sheetsSyncIntervalMin);

  // Пороги для режимів форсаж та аварія
  preferences.putFloat("tempCritLow", config.tempCriticalLow);
  preferences.putFloat("tempEmergLow", config.tempEmergencyLow);

  // Мережеві налаштування
  preferences.putBool("useStaticIP", config.useStaticIP);
  preferences.putString("staticIP", config.staticIP);
  preferences.putString("gateway", config.gateway);
  preferences.putString("subnet", config.subnet);
  preferences.putString("dns", config.dns);

  // Автентифікація
  preferences.putBool("useAuth", config.useAuth);
  preferences.putString("authLogin", config.authLogin);
  preferences.putString("authPass", config.authPassword);

  preferences.end();

  Serial.println("   ✅ Налаштування успішно збережені в NVS!\n");
}

void initMutexes() {
  sensorMutex = xSemaphoreCreateMutex();
  configMutex = xSemaphoreCreateMutex();
  heatingMutex = xSemaphoreCreateMutex();
  historyMutex = xSemaphoreCreateMutex();
  timeMutex = xSemaphoreCreateMutex();
  syncMutex = xSemaphoreCreateMutex();
  ramBufferMutex = xSemaphoreCreateMutex();

  if (sensorMutex == NULL || configMutex == NULL || 
      heatingMutex == NULL || historyMutex == NULL || 
      timeMutex == NULL || syncMutex == NULL || ramBufferMutex == NULL) {
    // Serial.println("Помилка створення м'ютексів!");
  } else {
    // Serial.println("М'ютекси створені");
  }
}

void createTasks() {
  // Створення задач FreeRTOS (handles глобальні для OTA)
  xTaskCreatePinnedToCore(
    sensorTask,        // Функція задачі
    "SensorTask",      // Назва
    8192,              // Розмір стеку
    NULL,              // Параметри
    2,                 // Пріоритет
    &sensorTaskHandle, // Handle
    0                  // Ядро
  );
  
  xTaskCreatePinnedToCore(
    heatingTask,        // Функція задачі
    "HeatingTask",      // Назва
    8192,               // Розмір стеку
    NULL,               // Параметри
    3,                  // Пріоритет (вище за SensorTask)
    &heatingTaskHandle, // Handle
    1                   // Ядро
  );
  
  xTaskCreatePinnedToCore(
    ventilationTask,    // Функція задачі
    "VentilationTask",  // Назва
    4096,               // Розмір стеку
    NULL,               // Параметри
    1,                  // Пріоритет
    &ventTaskHandle,    // Handle
    1                   // Ядро
  );
  
  xTaskCreatePinnedToCore(
    webTask,           // Функція задачі
    "WebTask",         // Назва
    8192,              // Розмір стеку
    NULL,              // Параметри
    2,                 // Пріоритет (підвищено для швидкого веб)
    &webTaskHandle,    // Handle
    0                  // Ядро
  );

  xTaskCreatePinnedToCore(
    timeTask,          // Функція задачі
    "TimeTask",        // Назва
    4096,              // Розмір стеку
    NULL,              // Параметри
    1,                 // Пріоритет
    &timeTaskHandle,   // Handle
    0                  // Ядро
  );
  
  xTaskCreatePinnedToCore(
    advancedLogicTask,        // Функція задачі
    "AdvLogicTask",           // Назва
    16384,                    // Розмір стеку
    NULL,                     // Параметри
    3,                        // Пріоритет (високий)
    &advancedLogicTaskHandle, // Handle
    1                         // Ядро
  );

  xTaskCreatePinnedToCore(
    dataLoggerTask,          // Функція задачі
    "DataLoggerTask",        // Назва
    8192,                    // Розмір стеку
    NULL,                    // Параметри
    1,                       // Пріоритет (низький)
    &dataLoggerTaskHandle,   // Handle
    0                        // Ядро 0
  );

  xTaskCreatePinnedToCore(
    plantFanTask,            // Функція задачі
    "PlantFanTask",          // Назва
    4096,                    // Розмір стеку
    NULL,                    // Параметри
    1,                       // Пріоритет
    &plantFanTaskHandle,     // Handle
    1                        // Ядро 1
  );

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
}struct tm* getTimeInfo() {
  updateTimeInfo();
  return &timeInfo;
}

void updateTimeInfo() {
  time(&currentTime);
  localtime_r(&currentTime, &timeInfo);
}

// ============================================================================
// ФУНКЦІЇ ДЛЯ ТРЕНДУ ТЕМПЕРАТУРИ
// ============================================================================


// ============================================================================
// ФУНКЦІЇ ДЛЯ ДОСТУПУ ДО М'ЮТЕКСІВ
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

SemaphoreHandle_t getSyncMutex() {
    return syncMutex;
}

SemaphoreHandle_t getRamBufferMutex() {
    return ramBufferMutex;
}

// ============================================================================
// Р¤РЈРќРљР¦Р†Р‡ Р"Р›РЇ Р"РћРЎРўРЈРџРЈ Р"Рћ Р§РђРЎРЈ
// ============================================================================

time_t getCurrentTime() {
    return currentTime;
}

void setCurrentTime(time_t t) {
    currentTime = t;
}

// Додано: функція для перевірки аварійного режиму
void checkEmergencyTimeout() {
    if (heatingState.emergencyMode && getEmergencyStartTime() > 0) {
        unsigned long now = millis();
        if (now - getEmergencyStartTime() > 120000) { // 2 хвилини = 120000 мс
            float tempCarrierDiff = sensorData.tempCarrier - getEmergencyStartTempCarrier();
            float tempRoomDiff = sensorData.tempRoom - getEmergencyStartTempRoom();
            
            if (tempCarrierDiff < 2.0f && tempRoomDiff < 2.0f) {
                setHeatingPower(0, 0, 0);
                digitalWrite(HUMIDIFIER_PIN, LOW);
                heatingState.emergencyMode = false;
                
                // Serial.println("🚨 АВАРИЙНИЙ РЕЖИМ: вимкнено через відсутність прогріву!");  // RUS_REMOVED
                Serial.printf("  Р—Р±С–Р»СЊС€РµРЅРЅСЏ С‚РµРјРїРµСЂР°С‚СѓСЂРё: С‚РµРїР»РѕРЅРѕСЃС–Р№ +%.1fВC, РєС–РјРЅР°С‚Р° +%.1fВC\n", 
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

// ============================================================================
// ФУНКЦІЇ ДЛЯ OTA - ПРИЗУПИНЕННЯ ЗАДАЧ
// ============================================================================

void suspendAllTasks() {
    Serial.println("⏸️ Призупинення FreeRTOS задач для OTA...");

    // НЕ призупиняємо webTaskHandle - він обробляє OTA!

    if (sensorTaskHandle != NULL) {
        vTaskSuspend(sensorTaskHandle);
        Serial.println("  - SensorTask призупинено");
    }

    if (heatingTaskHandle != NULL) {
        vTaskSuspend(heatingTaskHandle);
        Serial.println("  - HeatingTask призупинено");
    }

    if (ventTaskHandle != NULL) {
        vTaskSuspend(ventTaskHandle);
        Serial.println("  - VentilationTask призупинено");
    }

    if (timeTaskHandle != NULL) {
        vTaskSuspend(timeTaskHandle);
        Serial.println("  - TimeTask призупинено");
    }

    if (advancedLogicTaskHandle != NULL) {
        vTaskSuspend(advancedLogicTaskHandle);
        Serial.println("  - AdvancedLogicTask призупинено");
    }

    if (dataLoggerTaskHandle != NULL) {
        vTaskSuspend(dataLoggerTaskHandle);
        Serial.println("  - DataLoggerTask призупинено");
    }

    if (plantFanTaskHandle != NULL) {
        vTaskSuspend(plantFanTaskHandle);
        Serial.println("  - PlantFanTask призупинено");
    }
    if (dmxTaskHandle != NULL) {
        vTaskSuspend(dmxTaskHandle);
        Serial.println("  - DMXTask призупинено");
    }
    if (lightSchedulerTaskHandle != NULL) {
        vTaskSuspend(lightSchedulerTaskHandle);
        Serial.println("  - LightSchedulerTask призупинено");
    }
    if (loopTaskHandle != NULL) {
        vTaskSuspend(loopTaskHandle);
        Serial.println("  - LoopTask призупинено");
    }

    Serial.println("✅ Всі некритичні задачі призупинено");
}

void resumeAllTasks() {
    Serial.println("▶️ Відновлення FreeRTOS задач...");

    if (sensorTaskHandle != NULL) {
        vTaskResume(sensorTaskHandle);
        Serial.println("  - SensorTask відновлено");
    }

    if (heatingTaskHandle != NULL) {
        vTaskResume(heatingTaskHandle);
        Serial.println("  - HeatingTask відновлено");
    }

    if (ventTaskHandle != NULL) {
        vTaskResume(ventTaskHandle);
        Serial.println("  - VentilationTask відновлено");
    }

    if (timeTaskHandle != NULL) {
        vTaskResume(timeTaskHandle);
        Serial.println("  - TimeTask відновлено");
    }

    if (advancedLogicTaskHandle != NULL) {
        vTaskResume(advancedLogicTaskHandle);
        Serial.println("  - AdvancedLogicTask відновлено");
    }

    if (dataLoggerTaskHandle != NULL) {
        vTaskResume(dataLoggerTaskHandle);
        Serial.println("  - DataLoggerTask відновлено");
    }

    if (plantFanTaskHandle != NULL) {
        vTaskResume(plantFanTaskHandle);
        Serial.println("  - PlantFanTask відновлено");
    }
    if (dmxTaskHandle != NULL) {
        vTaskResume(dmxTaskHandle);
        Serial.println("  - DMXTask відновлено");
    }
    if (lightSchedulerTaskHandle != NULL) {
        vTaskResume(lightSchedulerTaskHandle);
        Serial.println("  - LightSchedulerTask відновлено");
    }
    if (loopTaskHandle != NULL) {
        vTaskResume(loopTaskHandle);
        Serial.println("  - LoopTask відновлено");
    }

    Serial.println("✅ Всі задачі відновлено");
}

