#include <Arduino.h>

#include "config.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "web_interface.h"
#include "data_storage.h"
#include "advanced_climate_logic.h"
#include "utility_functions.h"
#include <Arduino.h>

// Р“Р›РћР‘РђР›Р¬РќР«Р• РџР•Р Р•РњР•РќРќР«Р•
SystemConfig config;
SensorData sensorData;
HeatingState heatingState;
VentilationState ventState;
HumidifierState humidifierState;

extern int historyIndex;
extern bool historyInitialized;
extern HistoryData history[HISTORY_BUFFER_SIZE];

extern bool compactMode;

// РќРђРЎРўР РћР™РљРђ РЎРРЎРўР•РњР«
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n==========================================");
  Serial.println("       РЎРРЎРўР•РњРђ РљР›РРњРђРў-РљРћРќРўР РћР›РЇ v4.0");
  Serial.println("==========================================");
  
  initMutexes();
  loadConfiguration();
  
  Serial.println("\n=== РРќРР¦РРђР›РР—РђР¦РРЇ WI-FI ===");
  initWiFi();
  
  Serial.println("\n=== РРќРР¦РРђР›РР—РђР¦РРЇ Р’Р Р•РњР•РќР ===");
  initTime();
  
  Serial.println("\n=== РРќРР¦РРђР›РР—РђР¦РРЇ Р”РђРўР§РРљРћР’ ===");
  if (!initTemperatureSensors()) {
    Serial.println("вљ  Р’РќРРњРђРќРР•: РџСЂРѕР±Р»РµРјР° СЃ РґР°С‚С‡РёРєР°РјРё С‚РµРјРїРµСЂР°С‚СѓСЂС‹!");
  } else {
    Serial.println("вњ“ Р”Р°С‚С‡РёРєРё С‚РµРјРїРµСЂР°С‚СѓСЂС‹ РіРѕС‚РѕРІС‹");
  }
  
  if (!initBME280()) {
    Serial.println("вљ  Р’РќРРњРђРќРР•: РџСЂРѕР±Р»РµРјР° СЃ РґР°С‚С‡РёРєРѕРј BME280!");
  } else {
    Serial.println("вњ“ Р”Р°С‚С‡РёРє BME280 РіРѕС‚РѕРІ");
  }
  
  Serial.println("\n=== РРќРР¦РРђР›РР—РђР¦РРЇ РРЎРџРћР›РќРРўР•Р›Р¬РќР«РҐ РЈРЎРўР РћР™РЎРўР’ ===");
  initGPIO();
  initPWM();
  initServo();
  Serial.println("вњ“ РСЃРїРѕР»РЅРёС‚РµР»СЊРЅС‹Рµ СѓСЃС‚СЂРѕР№СЃС‚РІР° РіРѕС‚РѕРІС‹");
  
  Serial.println("\n=== РРќРР¦РРђР›РР—РђР¦РРЇ Р›РћР“РРљР ===");
  initAdvancedLogic();
  
  Serial.println("\n=== РЎРћР—Р”РђРќРР• Р—РђР”РђР§ ===");
  createTasks();
  
  Serial.println("\n=== РРќРР¦РРђР›РР—РђР¦РРЇ РЎРћРЎРўРћРЇРќРРЇ ===");
  heatingState.active = false;
  heatingState.pumpPower = 0;
  heatingState.fanPower = 0;
  heatingState.extractorPower = 0;
  heatingState.forceMode = false;
  heatingState.emergencyMode = false;
  heatingState.manualMode = false;
  heatingState.adaptive_heating_active = false;
  
  humidifierState.active = false;
  humidifierState.startTime = 0;
  humidifierState.lastCycle = 0;
  humidifierState.cyclesToday = 0;
  
  if (config.pumpMinPercent == 0) config.pumpMinPercent = PUMP_MIN_DEFAULT;
  if (config.pumpMaxPercent == 0) config.pumpMaxPercent = PUMP_MAX_DEFAULT;
  if (config.fanMaxPercent == 0) config.fanMaxPercent = FAN_MAX_DEFAULT;
  if (config.extractorMinPercent == 0) config.extractorMinPercent = EXTRACTOR_MIN_DEFAULT;
  if (config.extractorMaxPercent == 0) config.extractorMaxPercent = EXTRACTOR_MAX_DEFAULT;
  
  config.extractorTimer.cycleStart = 0;
  config.extractorTimer.state = false;
  config.extractorTimer.lastChange = 0;
  
  Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("вњ… РЎРРЎРўР•РњРђ Р“РћРўРћР’Рђ Рљ Р РђР‘РћРўР•");
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("РСЃРїРѕР»СЊР·СѓР№С‚Рµ:");
  Serial.println("  'm' - РјРµРЅСЋ СѓРїСЂР°РІР»РµРЅРёСЏ");
  Serial.println("  's' - СЃС‚Р°С‚СѓСЃ СЃРёСЃС‚РµРјС‹");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("  Р’РµР±-РёРЅС‚РµСЂС„РµР№СЃ: http://" + WiFi.localIP().toString());
  } else {
    Serial.println("  Р’РµР±-РёРЅС‚РµСЂС„РµР№СЃ: http://192.168.4.1 (СЂРµР¶РёРј С‚РѕС‡РєРё РґРѕСЃС‚СѓРїР°)");
  }
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
  
  setHeatingPower(0, 0, 0);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  
  Serial.println("вњ“ РќР°С‡Р°Р»СЊРЅРѕРµ СЃРѕСЃС‚РѕСЏРЅРёРµ СѓСЃС‚Р°РЅРѕРІР»РµРЅРѕ");
  Serial.println("вњ“ Р’СЃРµ СЃРёСЃС‚РµРјС‹ РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅС‹");
}

// Р“Р›РђР’РќР«Р™ Р¦РРљР›
void loop() {
  processAdvancedSerialCommand();
  autoPrintStatus();
  addToHistory();
  checkEmergencyTimeout();
  delay(100);
}

// Р’РЎРџРћРњРћР“РђРўР•Р›Р¬РќР«Р• Р¤РЈРќРљР¦РР
void printSystemInfo() {
  Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("рџ“Љ РРќР¤РћР РњРђР¦РРЇ Рћ РЎРРЎРўР•РњР•");
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.printf("  IP Р°РґСЂРµСЃ: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("  РЎРІРѕР±РѕРґРЅР°СЏ РїР°РјСЏС‚СЊ: %lu KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("  Р’СЂРµРјСЏ СЂР°Р±РѕС‚С‹: %lu СЃРµРє\n", millis() / 1000);
  Serial.printf("  Р—Р°РїРёСЃРµР№ РІ РёСЃС‚РѕСЂРёРё: %d\n", historyIndex);
  Serial.printf("  Р РµР¶РёРј Serial: %s\n", compactMode ? "РљРѕРјРїР°РєС‚РЅС‹Р№" : "Р Р°СЃС€РёСЂРµРЅРЅС‹Р№");
  Serial.printf("  РќР°СЃРѕСЃ (A): %d%% (min:%d%%, max:%d%%)\n", 
                (heatingState.pumpPower * 100) / 255, 
                config.pumpMinPercent, config.pumpMaxPercent);
  Serial.printf("  Р’РµРЅС‚РёР»СЏС‚РѕСЂ (B): %d%% (min:%d%%, max:%d%%)\n", 
                (heatingState.fanPower * 100) / 255,
                config.fanMinPercent, config.fanMaxPercent);
  Serial.printf("  Р’С‹С‚СЏР¶РєР° (C): %d%% (min:%d%%, max:%d%%)\n", 
                (heatingState.extractorPower * 100) / 255,
                config.extractorMinPercent, config.extractorMaxPercent);
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
}

void emergencyStop() {
  Serial.println("\nрџљЁрџљЁрџљЁ Р­РљРЎРўР Р•РќРќРћР• РћРўРљР›Р®Р§Р•РќРР• РЎРРЎРўР•РњР« рџљЁрџљЁрџљЁ");
  
  setHeatingPower(0, 0, 0);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  moveServoSmooth(SERVO_CLOSED_ANGLE);
  
  heatingState.emergencyMode = true;
  heatingState.manualMode = false;
  heatingState.forceMode = false;
  
  Serial.println("вњ… Р’СЃРµ СЃРёСЃС‚РµРјС‹ РѕС‚РєР»СЋС‡РµРЅС‹");
  Serial.println("вљ  РЎРёСЃС‚РµРјР° РІ Р°РІР°СЂРёР№РЅРѕРј СЂРµР¶РёРјРµ");
}

void runSystemTest() {
  Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("рџ”§ РџРћР›РќР«Р™ РўР•РЎРў РЎРРЎРўР•РњР«");
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  
  Serial.println("1. РўРµСЃС‚ РЅР°СЃРѕСЃР°...");
  setPumpPercent(50);
  delay(2000);
  setPumpPercent(0);
  Serial.println("   вњ… РќР°СЃРѕСЃ: РћРљ");
  
  Serial.println("2. РўРµСЃС‚ РІРµРЅС‚РёР»СЏС‚РѕСЂР°...");
  setFanPercent(50);
  delay(2000);
  setFanPercent(0);
  Serial.println("   вњ… Р’РµРЅС‚РёР»СЏС‚РѕСЂ: РћРљ");
  
  Serial.println("3. РўРµСЃС‚ РІС‹С‚СЏР¶РєРё...");
  setExtractorPercent(50);
  delay(2000);
  setExtractorPercent(0);
  Serial.println("   вњ… Р’С‹С‚СЏР¶РєР°: РћРљ");
  
  Serial.println("4. РўРµСЃС‚ РІРµРЅС‚РёР»СЏС†РёРё...");
  testVentilation();
  Serial.println("   вњ… Р’РµРЅС‚РёР»СЏС†РёСЏ: РћРљ");
  
  Serial.println("5. РўРµСЃС‚ СѓРІР»Р°Р¶РЅРёС‚РµР»СЏ...");
  digitalWrite(HUMIDIFIER_PIN, HIGH);
  delay(1000);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  Serial.println("   вњ… РЈРІР»Р°Р¶РЅРёС‚РµР»СЊ: РћРљ");
  
  Serial.println("6. РџСЂРѕРІРµСЂРєР° РґР°С‚С‡РёРєРѕРІ...");
  if (sensorData.carrierValid && sensorData.roomValid && sensorData.bmeValid) {
    Serial.printf("   вњ… Р”Р°С‚С‡РёРєРё: T=%.1fВ°C, H=%.1f%%, P=%.1fhPa\n",
                 sensorData.tempRoom, sensorData.humidity, sensorData.pressure);
  } else {
    Serial.println("   вљ  РџСЂРѕР±Р»РµРјР° СЃ РґР°С‚С‡РёРєР°РјРё!");
  }
  
  Serial.println("7. РџСЂРѕРІРµСЂРєР° Wi-Fi...");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   вњ… Wi-Fi: %s (%d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
  } else {
    Serial.println("   вљ  Wi-Fi РЅРµ РїРѕРґРєР»СЋС‡РµРЅ!");
  }
  
  Serial.println("8. РџСЂРѕРІРµСЂРєР° РІСЂРµРјРµРЅРё...");
  String timeStr = getTimeString();
  if (timeStr != "РџРѕРјРёР»РєР° С‡Р°СЃСѓ") {
    Serial.println("   вњ… Р’СЂРµРјСЏ: " + timeStr);
  } else {
    Serial.println("   вљ  РџСЂРѕР±Р»РµРјР° СЃРѕ РІСЂРµРјРµРЅРµРј!");
  }
  
  Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("рџЋ‰ РўР•РЎРў Р—РђР’Р•Р РЁР•Рќ РЈРЎРџР•РЁРќРћ!");
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
  
  setHeatingPower(0, 0, 0);
  digitalWrite(HUMIDIFIER_PIN, LOW);
}

void debugCommands() {
  Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("рџђћ РћРўР›РђР”РћР§РќР«Р• РљРћРњРђРќР”Р«");
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
  Serial.println("  test     - РџРѕР»РЅС‹Р№ С‚РµСЃС‚ СЃРёСЃС‚РµРјС‹");
  Serial.println("  info     - РРЅС„РѕСЂРјР°С†РёСЏ Рѕ СЃРёСЃС‚РµРјРµ");
  Serial.println("  stop     - Р­РєСЃС‚СЂРµРЅРЅРѕРµ РѕС‚РєР»СЋС‡РµРЅРёРµ");
  Serial.println("  start    - Р—Р°РїСѓСЃРє Р°РІС‚РѕРјР°С‚РёС‡РµСЃРєРѕРіРѕ СЂРµР¶РёРјР°");
  Serial.println("  reset    - РЎР±СЂРѕСЃ РЅР°СЃС‚СЂРѕРµРє Рє Р·Р°РІРѕРґСЃРєРёРј");
  Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
}

void resetToFactoryDefaults() {
  Serial.println("\nвљ вљ вљ  РЎР‘Р РћРЎ РќРђРЎРўР РћР•Рљ Рљ Р—РђР’РћР”РЎРљРРњ вљ вљ вљ ");
  Serial.println("Р’СЃРµ РЅР°СЃС‚СЂРѕР№РєРё Р±СѓРґСѓС‚ РїРѕС‚РµСЂСЏРЅС‹!");
  Serial.println("Р”Р»СЏ РѕС‚РјРµРЅС‹ РїРµСЂРµР·Р°РіСЂСѓР·РёС‚Рµ СЃРёСЃС‚РµРјСѓ РІ С‚РµС‡РµРЅРёРµ 5 СЃРµРєСѓРЅРґ...");
  
  for (int i = 5; i > 0; i--) {
    Serial.printf("РћСЃС‚Р°Р»РѕСЃСЊ %d СЃРµРєСѓРЅРґ...\n", i);
    delay(1000);
  }
  
  preferences.begin("climate", false);
  preferences.clear();
  preferences.end();
  
  Serial.println("вњ… РќР°СЃС‚СЂРѕР№РєРё СЃР±СЂРѕС€РµРЅС‹ Рє Р·Р°РІРѕРґСЃРєРёРј");
  Serial.println("рџ”„ РџРµСЂРµР·Р°РіСЂСѓР·РєР° СЃРёСЃС‚РµРјС‹...");
  delay(2000);
  ESP.restart();
}

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input == "test") {
      runSystemTest();
    }
    else if (input == "info") {
      printSystemInfo();
    }
    else if (input == "stop") {
      emergencyStop();
    }
    else if (input == "start") {
      heatingState.manualMode = false;
      heatingState.emergencyMode = false;
      Serial.println("вњ… РђРІС‚РѕРјР°С‚РёС‡РµСЃРєРёР№ СЂРµР¶РёРј Р°РєС‚РёРІРёСЂРѕРІР°РЅ");
    }
    else if (input == "reset") {
      resetToFactoryDefaults();
    }
    else if (input == "debug") {
      debugCommands();
    }
    else {
      processAdvancedSerialCommand();
    }
  }
}

