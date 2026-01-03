
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "web_interface.h"
#include "data_storage.h"
#include "advanced_climate_logic.h"
#include <time.h>

// РћРіРѕР»РѕС€РµРЅРЅСЏ С„СѓРЅРєС†С–Р№ Р·Р°РґР°С‡
extern void sensorTask(void *parameter);
extern void heatingTask(void *parameter);
extern void ventilationTask(void *parameter);
extern void webTask(void *parameter);
extern void timeTask(void *parameter);
extern void advancedLogicTask(void *parameter);

// Р“Р»РѕР±Р°Р»СЊРЅС– РѕР±'С”РєС‚Рё
Preferences preferences;
WebServer server(80);

// РњКјСЋС‚РµРєСЃРё РґР»СЏ СЃРёРЅС…СЂРѕРЅС–Р·Р°С†С–С—
SemaphoreHandle_t sensorMutex;
SemaphoreHandle_t configMutex;
SemaphoreHandle_t heatingMutex;
SemaphoreHandle_t historyMutex;
SemaphoreHandle_t timeMutex;

// РЎС‚Р°С‚РёС‡РЅС– Р·РјС–РЅРЅС– (С–РЅРєР°РїСЃСѓР»СЊРѕРІР°РЅС– РІ С†СЊРѕРјСѓ С„Р°Р№Р»С–)
static unsigned long _lastStatusPrint = 0;
static unsigned long _lastSerialCheck = 0;
static unsigned long _lastTrendCheck = 0;
static unsigned long _lastHistorySave = 0;
static unsigned long _lastExtractorCheck = 0;

// Р—РјС–РЅРЅС– РґР»СЏ С‡Р°СЃСѓ
time_t currentTime = 0;
struct tm timeInfo;
static bool timeSynced = false;
static unsigned long _lastTimeSync = 0;

// Р‘СѓС„РµСЂРё РґР»СЏ С‚СЂРµРЅРґСѓ
float tempTrendBuffer[TREND_WINDOW_SIZE];  // Р—РјС–РЅРµРЅРѕ Р· static РЅР° РіР»РѕР±Р°Р»СЊРЅСѓ
int trendIndex = 0;  // Р—РјС–РЅРµРЅРѕ Р· static РЅР° РіР»РѕР±Р°Р»СЊРЅСѓ
static bool trendBufferFilled = false;

// Р”РѕРґР°РЅРѕ: Р·РјС–РЅРЅС– РґР»СЏ Р°РІР°СЂС–Р№РЅРѕРіРѕ СЂРµР¶РёРјСѓ
static unsigned long _emergencyStartTime = 0;
static float _emergencyStartTempCarrier = 0;
static float _emergencyStartTempRoom = 0;
// ============================================================================
// Р“Р•РўРўР•Р Р РўРђ РЎР•РўРўР•Р Р
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

// Р”РѕРґР°РЅРѕ: РіРµС‚С‚РµСЂРё/СЃРµС‚С‚РµСЂРё РґР»СЏ Р°РІР°СЂС–Р№РЅРѕРіРѕ СЂРµР¶РёРјСѓ
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
// Р¤РЈРќРљР¦Р†Р‡ Р”Р›РЇ Р РћР‘РћРўР Р— Р§РђРЎРћРњ
// ============================================================================

void initTime() {
  Serial.println("РЎРїСЂРѕР±СѓС”РјРѕ СЃРёРЅС…СЂРѕРЅС–Р·СѓРІР°С‚Рё С‡Р°СЃ...");
  
  // РСЃРїРѕР»СЊР·СѓРµРј РЅРµСЃРєРѕР»СЊРєРѕ NTP СЃРµСЂРІРµСЂРѕРІ РґР»СЏ РЅР°РґРµР¶РЅРѕСЃС‚Рё
  const char* ntpServers[] = {
    "pool.ntp.org",           // РћСЃРЅРѕРІРЅРѕР№ РїСѓР»
    "time.google.com",        // Google (РѕС‡РµРЅСЊ С‚РѕС‡РЅС‹Р№)
    "time.windows.com",       // Microsoft
    "ntp.ubuntu.com",         // Ubuntu
    "0.ua.pool.ntp.org",      // РЈРєСЂР°РёРЅСЃРєРёР№ СЃРµСЂРІРµСЂ 1
    "1.ua.pool.ntp.org",      // РЈРєСЂР°РёРЅСЃРєРёР№ СЃРµСЂРІРµСЂ 2
    "2.ua.pool.ntp.org",      // РЈРєСЂР°РёРЅСЃРєРёР№ СЃРµСЂРІРµСЂ 3
    "3.ua.pool.ntp.org"       // РЈРєСЂР°РёРЅСЃРєРёР№ СЃРµСЂРІРµСЂ 4
  };
  
  // GMT+2 РґР»СЏ РЈРєСЂР°РёРЅС‹, Р±РµР· Р»РµС‚РЅРµРіРѕ РІСЂРµРјРµРЅРё (Р·РёРјР°)
  // Р•СЃР»Рё СЃРµР№С‡Р°СЃ Р»РµС‚РЅРµРµ РІСЂРµРјСЏ - РёР·РјРµРЅРё РЅР° 3*3600
  configTime(2 * 3600, 0, 
    ntpServers[0], ntpServers[1], ntpServers[2]);
  
  Serial.print("РЎРёРЅС…СЂРѕРЅС–Р·Р°С†С–СЏ Р· NTP СЃРµСЂРІРµСЂР°РјРё...");
  
  int maxAttempts = 30;  // 30 СЃРµРєСѓРЅРґ РѕР¶РёРґР°РЅРёСЏ
  bool ntpSuccess = false;
  
  for (int i = 0; i < maxAttempts; i++) {
    if (getLocalTime(&timeInfo, 100)) {
      timeSynced = true;
      _lastTimeSync = millis();
      time(&currentTime);
      
      Serial.println("\nвњ“ Р§Р°СЃ СЃРёРЅС…СЂРѕРЅС–Р·РѕРІР°РЅРѕ С‡РµСЂРµР· NTP!");
      Serial.printf("РЎРµСЂРІРµСЂ: %s\n", ntpServers[0]);
      Serial.printf("РћС‚СЂРёРјР°РЅРѕ: %d-%02d-%02d %02d:%02d:%02d\n",
                    timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                    timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
      
      // РџСЂРѕРІРµСЂСЏРµРј С‡С‚Рѕ РІСЂРµРјСЏ Р°РґРµРєРІР°С‚РЅРѕРµ (РЅРµ РІ РїСЂРѕС€Р»РѕРј РІРµРєРµ)
      if (timeInfo.tm_year + 1900 >= 2024) {
        Serial.println("вњ“ Р§Р°СЃ РІР°Р»С–РґРЅРёР№ (РїС–СЃР»СЏ 2024 СЂРѕРєСѓ)");
        ntpSuccess = true;
        break;
      } else {
        Serial.println("вљ  NTP РїРѕРІРµСЂРЅСѓРІ СЃС‚Р°СЂРµ С‡Р°СЃ, РїСЂРѕР±СѓС”РјРѕ С–РЅС€РёР№ СЃРµСЂРІРµСЂ...");
        
        // РџСЂРѕР±СѓРµРј СЃР»РµРґСѓСЋС‰РёР№ СЃРµСЂРІРµСЂ
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
    
    // РљР°Р¶РґС‹Рµ 10 СЃРµРєСѓРЅРґ РјРµРЅСЏРµРј СЃРµСЂРІРµСЂ
    if (i > 0 && i % 10 == 0) {
      Serial.printf("\nРЎРїСЂРѕР±Р° %d/30, РјС–РЅСЏСЋ СЃРµСЂРІРµСЂ...\n", i);
    }
  }
  
  if (!ntpSuccess) {
    Serial.println("\nвљ  Р’СЃС– NTP СЃРµСЂРІРµСЂРё РЅРµ РІС–РґРїРѕРІС–РґР°СЋС‚СЊ");
    
    // РЈСЃС‚Р°РЅР°РІР»РёРІР°РµРј РїСЂРёРјРµСЂРЅРѕРµ РІСЂРµРјСЏ РІСЂСѓС‡РЅСѓСЋ
    struct tm tm;
    tm.tm_year = 2024 - 1900;   // 2024 РіРѕРґ
    tm.tm_mon = 2 - 1;          // Р¤РµРІСЂР°Р»СЊ (РµСЃР»Рё СЃРµР№С‡Р°СЃ С„РµРІСЂР°Р»СЊ)
    tm.tm_mday = 18;            // 18 С‡РёСЃР»Рѕ
    tm.tm_hour = 14;            // 14 С‡Р°СЃРѕРІ
    tm.tm_min = 0;              // 0 РјРёРЅСѓС‚
    tm.tm_sec = 0;              // 0 СЃРµРєСѓРЅРґ
    tm.tm_isdst = -1;
    
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    
    time(&currentTime);
    localtime_r(&currentTime, &timeInfo);
    timeSynced = true;
    
    Serial.printf("вњ“ Р›РѕРєР°Р»СЊРЅРёР№ С‡Р°СЃ РІСЃС‚Р°РЅРѕРІР»РµРЅРѕ: %s\n", getTimeString().c_str());
    Serial.println("в„№ Р’РёРєРѕСЂРёСЃС‚Р°Р№С‚Рµ РІРµР±-С–РЅС‚РµСЂС„РµР№СЃ (/settime) РґР»СЏ С‚РѕС‡РЅРѕРіРѕ С‡Р°СЃСѓ");
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
        timeinfo.tm_isdst = -1; // РђРІС‚РѕРІРёР·РЅР°С‡РµРЅРЅСЏ Р»С–С‚РЅСЊРѕРіРѕ С‡Р°СЃСѓ
        time_t adjustedTime = mktime(&timeinfo);
        struct timeval tv = {adjustedTime, 0};
        settimeofday(&tv, nullptr);
        xSemaphoreGive(timeMutex);
      }
      
      Serial.println("вњ“ Р§Р°СЃ РѕРЅРѕРІР»РµРЅРѕ");
    }
  }
}

String getFormattedTime() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    // РџРѕР»СѓС‡Р°РµРј Р Р•РђР›Р¬РќРћР• С‚РµРєСѓС‰РµРµ РІСЂРµРјСЏ
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
  return "РџРѕРјРёР»РєР°";
}

String getDateString() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    // РџРѕР»СѓС‡Р°РµРј Р Р•РђР›Р¬РќРћР• С‚РµРєСѓС‰РµРµ РІСЂРµРјСЏ
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);
    
    xSemaphoreGive(timeMutex);
    return String(dateStr);
  }
  return "РџРѕРјРёР»РєР° РґР°С‚Рё";
}

String getTimeString() {
  if (xSemaphoreTake(timeMutex, pdMS_TO_TICKS(100))) {
    time_t now;
    struct tm timeinfo;
    
    // РџРѕР»СѓС‡Р°РµРј Р Р•РђР›Р¬РќРћР• С‚РµРєСѓС‰РµРµ РІСЂРµРјСЏ
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", &timeinfo);
    
    xSemaphoreGive(timeMutex);
    return String(timeStr);
  }
  return "РџРѕРјРёР»РєР° С‡Р°СЃСѓ";
}
// ============================================================================
// Р¤РЈРќРљР¦Р†Р‡ РљРћРќР¤Р†Р“РЈР РђР¦Р†Р‡
// ============================================================================

void loadConfiguration() {
  preferences.begin("climate", false);
  
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
  
  // Р”РѕРґР°РЅРѕ: РѕР±РјРµР¶РµРЅРЅСЏ РїСЂРёСЃС‚СЂРѕС—РІ
  config.pumpMinPercent = preferences.getUChar("pumpMin", PUMP_MIN_DEFAULT);
  config.pumpMaxPercent = preferences.getUChar("pumpMax", PUMP_MAX_DEFAULT);
  config.fanMaxPercent = preferences.getUChar("fanMax", FAN_MAX_DEFAULT);
  config.extractorMinPercent = preferences.getUChar("extractorMin", EXTRACTOR_MIN_DEFAULT);
  config.extractorMaxPercent = preferences.getUChar("extractorMax", EXTRACTOR_MAX_DEFAULT);
  
  
  // РќР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ РІРѕР»РѕРіРѕСЃС‚С–
  config.humidityConfig.minHumidity = preferences.getFloat("humMin", HUM_MIN_DEFAULT);
  config.humidityConfig.maxHumidity = preferences.getFloat("humMax", HUM_MAX_DEFAULT);
  config.humidityConfig.tempCoefficient = preferences.getFloat("humCoeff", HUM_TEMP_COEFF);
  config.humidityConfig.enabled = preferences.getBool("humCtrl", true);
  config.humidityConfig.minInterval = preferences.getULong("humInt", HUMIDIFIER_MIN_INTERVAL);
  config.humidityConfig.maxRunTime = preferences.getULong("humMaxRun", HUMIDIFIER_MAX_RUN_TIME);
  config.humidityConfig.hysteresis = preferences.getUChar("humHyst", 3);
  config.humidityConfig.adaptiveMode = preferences.getBool("humAdapt", true);
  
  // РќР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ Р°РґР°РїС‚РёРІРЅРѕРіРѕ РѕР±С–РіСЂС–РІСѓ
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
  
  // РўР°Р№РјРµСЂ РІРёС‚СЏР¶РєРё
  config.extractorTimer.enabled = preferences.getBool("extTimer", false);
  config.extractorTimer.onMinutes = preferences.getUShort("extOnMin", 30);
  config.extractorTimer.offMinutes = preferences.getUShort("extOffMin", 30);
  config.extractorTimer.powerPercent = preferences.getUChar("extPower", 70);
  config.extractorTimer.state = false;
  config.extractorTimer.cycleStart = 0;
  config.extractorTimer.lastChange = 0;
  
  config.history_size = preferences.getUShort("historySize", 1440);
  
  preferences.end();
  
  // Р†РЅС–С†С–Р°Р»С–Р·Р°С†С–СЏ Р±СѓС„РµСЂС–РІ
  for (int i = 0; i < TREND_WINDOW_SIZE; i++) {
    tempTrendBuffer[i] = 0.0f;
  }
  
  // РЎРєРёРґР°РЅРЅСЏ Р°РІР°СЂС–Р№РЅРёС… Р·РјС–РЅРЅРёС…
  _emergencyStartTime = 0;
  _emergencyStartTempCarrier = 0;
  _emergencyStartTempRoom = 0;
  
  Serial.println("=== РќР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ Р·Р°РІР°РЅС‚Р°Р¶РµРЅРѕ ===");
}

void saveConfiguration() {
  preferences.begin("climate", false);
  
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
  
  // Р”РѕРґР°РЅРѕ: РѕР±РјРµР¶РµРЅРЅСЏ РїСЂРёСЃС‚СЂРѕС—РІ
  preferences.putUChar("pumpMin", config.pumpMinPercent);
  preferences.putUChar("pumpMax", config.pumpMaxPercent);
  preferences.putUChar("fanMax", config.fanMaxPercent);
  preferences.putUChar("extractorMin", config.extractorMinPercent);
  preferences.putUChar("extractorMax", config.extractorMaxPercent);
  
  // РќР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ РІРѕР»РѕРіРѕСЃС‚С–
  preferences.putFloat("humMin", config.humidityConfig.minHumidity);
  preferences.putFloat("humMax", config.humidityConfig.maxHumidity);
  preferences.putFloat("humCoeff", config.humidityConfig.tempCoefficient);
  preferences.putBool("humCtrl", config.humidityConfig.enabled);
  preferences.putULong("humInt", config.humidityConfig.minInterval);
  preferences.putULong("humMaxRun", config.humidityConfig.maxRunTime);
  preferences.putUChar("humHyst", config.humidityConfig.hysteresis);
  preferences.putBool("humAdapt", config.humidityConfig.adaptiveMode);
  
  // РќР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ Р°РґР°РїС‚РёРІРЅРѕРіРѕ РѕР±С–РіСЂС–РІСѓ
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
  
  // РўР°Р№РјРµСЂ РІРёС‚СЏР¶РєРё
  preferences.putBool("extTimer", config.extractorTimer.enabled);
  preferences.putUShort("extOnMin", config.extractorTimer.onMinutes);
  preferences.putUShort("extOffMin", config.extractorTimer.offMinutes);
  preferences.putUChar("extPower", config.extractorTimer.powerPercent);
  
  preferences.putUShort("historySize", config.history_size);
  
  preferences.end();
  
  Serial.println("вњ“ РќР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ Р·Р±РµСЂРµР¶РµРЅРѕ");
}

// ============================================================================
// Р¤РЈРќРљР¦Р†Р‡ Р†РќР†Р¦Р†РђР›Р†Р—РђР¦Р†Р‡
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
    Serial.println("вљ  РџРѕРјРёР»РєР° СЃС‚РІРѕСЂРµРЅРЅСЏ РјКјСЋС‚РµРєСЃС–РІ!");
  } else {
    Serial.println("вњ“ РњКјСЋС‚РµРєСЃРё СЃС‚РІРѕСЂРµРЅС–");
  }
}

void createTasks() {
  // РЎС‚РІРѕСЂРµРЅРЅСЏ Р·Р°РґР°С‡ FreeRTOS
  TaskHandle_t sensorTaskHandle = NULL;
  TaskHandle_t heatingTaskHandle = NULL;
  TaskHandle_t ventTaskHandle = NULL;
  TaskHandle_t webTaskHandle = NULL;
  TaskHandle_t timeTaskHandle = NULL;
  TaskHandle_t advancedLogicTaskHandle = NULL;
  
  xTaskCreatePinnedToCore(
    sensorTask,        // Р¤СѓРЅРєС†С–СЏ Р·Р°РґР°С‡С–
    "SensorTask",      // РќР°Р·РІР°
    8192,              // Р РѕР·РјС–СЂ СЃС‚РµРєСѓ
    NULL,              // РџР°СЂР°РјРµС‚СЂРё
    2,                 // РџСЂС–РѕСЂРёС‚РµС‚
    &sensorTaskHandle, // Handle
    0                  // РЇРґСЂРѕ
  );
  
  xTaskCreatePinnedToCore(
    heatingTask,        // Р¤СѓРЅРєС†С–СЏ Р·Р°РґР°С‡С–
    "HeatingTask",      // РќР°Р·РІР°
    8192,               // Р РѕР·РјС–СЂ СЃС‚РµРєСѓ
    NULL,               // РџР°СЂР°РјРµС‚СЂРё
    3,                  // РџСЂС–РѕСЂРёС‚РµС‚ (РІРёС‰Рµ Р·Р° SensorTask)
    &heatingTaskHandle, // Handle
    1                   // РЇРґСЂРѕ
  );
  
  xTaskCreatePinnedToCore(
    ventilationTask,    // Р¤СѓРЅРєС†С–СЏ Р·Р°РґР°С‡С–
    "VentilationTask",  // РќР°Р·РІР°
    4096,               // Р РѕР·РјС–СЂ СЃС‚РµРєСѓ
    NULL,               // РџР°СЂР°РјРµС‚СЂРё
    1,                  // РџСЂС–РѕСЂРёС‚РµС‚
    &ventTaskHandle,    // Handle
    1                   // РЇРґСЂРѕ
  );
  
  xTaskCreatePinnedToCore(
    webTask,           // Р¤СѓРЅРєС†С–СЏ Р·Р°РґР°С‡С–
    "WebTask",         // РќР°Р·РІР°
    8192,              // Р РѕР·РјС–СЂ СЃС‚РµРєСѓ
    NULL,              // РџР°СЂР°РјРµС‚СЂРё
    1,                 // РџСЂС–РѕСЂРёС‚РµС‚
    &webTaskHandle,    // Handle
    0                  // РЇРґСЂРѕ
  );
  
  xTaskCreatePinnedToCore(
    timeTask,          // Р¤СѓРЅРєС†С–СЏ Р·Р°РґР°С‡С–
    "TimeTask",        // РќР°Р·РІР°
    4096,              // Р РѕР·РјС–СЂ СЃС‚РµРєСѓ
    NULL,              // РџР°СЂР°РјРµС‚СЂРё
    1,                 // РџСЂС–РѕСЂРёС‚РµС‚
    &timeTaskHandle,   // Handle
    0                  // РЇРґСЂРѕ
  );
  
  xTaskCreatePinnedToCore(
    advancedLogicTask,        // Р¤СѓРЅРєС†С–СЏ Р·Р°РґР°С‡С–
    "AdvLogicTask",           // РќР°Р·РІР°
    16384,                    // Р РѕР·РјС–СЂ СЃС‚РµРєСѓ
    NULL,                     // РџР°СЂР°РјРµС‚СЂРё
    3,                        // РџСЂС–РѕСЂРёС‚РµС‚ (РІРёСЃРѕРєРёР№)
    &advancedLogicTaskHandle, // Handle
    1                         // РЇРґСЂРѕ
  );
  
  Serial.println("вњ“ Р—Р°РґР°С‡С– FreeRTOS СЃС‚РІРѕСЂРµРЅС–");
  
  // Р”Р°С”РјРѕ С‡Р°СЃ Р·Р°РґР°С‡Р°Рј Р·Р°РїСѓСЃС‚РёС‚РёСЃСЏ
  delay(100);
}

// ============================================================================
// Р”РћРџРћРњР†Р–РќР† Р¤РЈРќРљР¦Р†Р‡
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
// Р¤РЈРќРљР¦Р†Р‡ Р”Р›РЇ РўР Р•РќР”РЈ РўР•РњРџР•Р РђРўРЈР Р
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
// Р¤РЈРќРљР¦Р†Р‡ Р”Р›РЇ Р”РћРЎРўРЈРџРЈ Р”Рћ Рњ'Р®РўР•РљРЎР†Р’
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
// Р¤РЈРќРљР¦Р†Р‡ Р”Р›РЇ Р”РћРЎРўРЈРџРЈ Р”Рћ Р§РђРЎРЈ
// ============================================================================

time_t getCurrentTime() {
    return currentTime;
}

void setCurrentTime(time_t t) {
    currentTime = t;
}

// Р”РѕРґР°РЅРѕ: С„СѓРЅРєС†С–СЏ РґР»СЏ РїРµСЂРµРІС–СЂРєРё Р°РІР°СЂС–Р№РЅРѕРіРѕ СЂРµР¶РёРјСѓ
void checkEmergencyTimeout() {
    if (heatingState.emergencyMode && getEmergencyStartTime() > 0) {
        unsigned long now = millis();
        if (now - getEmergencyStartTime() > 120000) { // 2 С…РІРёР»РёРЅРё = 120000 РјСЃ
            float tempCarrierDiff = sensorData.tempCarrier - getEmergencyStartTempCarrier();
            float tempRoomDiff = sensorData.tempRoom - getEmergencyStartTempRoom();
            
            if (tempCarrierDiff < 2.0f && tempRoomDiff < 2.0f) {
                setHeatingPower(0, 0, 0);
                digitalWrite(HUMIDIFIER_PIN, LOW);
                heatingState.emergencyMode = false;
                
                Serial.println("рџљЁ РђР’РђР РР™РќРР™ Р Р•Р–РРњ: РІРёРјРєРЅРµРЅРѕ С‡РµСЂРµР· РІС–РґСЃСѓС‚РЅС–СЃС‚СЊ РїСЂРѕРіСЂС–РІСѓ!");
                Serial.printf("  Р—Р±С–Р»СЊС€РµРЅРЅСЏ С‚РµРјРїРµСЂР°С‚СѓСЂРё: С‚РµРїР»РѕРЅРѕСЃС–Р№ +%.1fВ°C, РєС–РјРЅР°С‚Р° +%.1fВ°C\n", 
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

