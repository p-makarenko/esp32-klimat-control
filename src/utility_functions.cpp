#include "system_core.h"
#include "actuator_manager.h"
#include "advanced_climate_logic.h"
#include "sensor_manager.h"
#include "data_storage.h"
#include <Arduino.h>

// РћР±СЉСЏРІРёС‚Рµ РІРЅРµС€РЅРёРµ РїРµСЂРµРјРµРЅРЅС‹Рµ
extern int historyIndex;
extern bool historyInitialized;

// ============================================================================
// РђР’РўРћРњРђРўРР§Р•РЎРљРР™ Р’Р«Р’РћР” РЎРўРђРўРЈРЎРђ
// ============================================================================

void autoPrintStatus() {
    static unsigned long lastPrint = 0;
    unsigned long now = millis();
    
    if (now - lastPrint < config.statusPeriod) {
        return;
    }
    
    lastPrint = now;
    
    // РџРѕР»СѓС‡Р°РµРј РґР°РЅРЅС‹Рµ
    float tempRoom = 0, tempCarrier = 0, humidity = 0, pressure = 0;
    uint8_t pumpPower = 0, fanPower = 0, extractorPower = 0;
    
    if (xSemaphoreTake(getSensorMutex(), pdMS_TO_TICKS(100))) {
        tempRoom = sensorData.tempRoom;
        tempCarrier = sensorData.tempCarrier;
        humidity = sensorData.humidity;
        pressure = sensorData.pressure;
        xSemaphoreGive(getSensorMutex());
    }
    
    if (xSemaphoreTake(getHeatingMutex(), pdMS_TO_TICKS(100))) {
        pumpPower = heatingState.pumpPower;
        fanPower = heatingState.fanPower;
        extractorPower = heatingState.extractorPower;
        xSemaphoreGive(getHeatingMutex());
    }
    
    // Р’С‹РІРѕРґРёРј СЃС‚Р°С‚СѓСЃ
    Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.println("           рџ“Љ РЎРўРђРўРЈРЎ РЎРРЎРўР•РњР«");
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.printf("  Р’СЂРµРјСЏ: %s\n", getTimeString().c_str());
    Serial.printf("  Р РµР¶РёРј: %s\n", getCurrentMode().c_str());
    Serial.println("----------------------------------------------");
    Serial.printf("  РўРµРјРїРµСЂР°С‚СѓСЂР°: %.1fВ°C (С†РµР»СЊ: %.1f-%.1fВ°C)\n", 
                 tempRoom, config.tempMin, config.tempMax);
    Serial.printf("  РўРµРїР»РѕРЅРѕСЃРёС‚РµР»СЊ: %.1fВ°C\n", tempCarrier);
    Serial.printf("  Р’Р»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%% (С†РµР»СЊ: %.1f-%.1f%%)\n", 
                 humidity, config.humidityConfig.minHumidity, config.humidityConfig.maxHumidity);
    Serial.printf("  Р”Р°РІР»РµРЅРёРµ: %.1f hPa\n", pressure);
    Serial.println("----------------------------------------------");
    Serial.printf("  РќР°СЃРѕСЃ (A): %d%% (%d)\n", (pumpPower * 100) / 255, pumpPower);
    Serial.printf("  Р’РµРЅС‚РёР»СЏС‚РѕСЂ (B): %d%% (%d)\n", (fanPower * 100) / 255, fanPower);
    Serial.printf("  Р’С‹С‚СЏР¶РєР° (C): %d%% (%d)\n", (extractorPower * 100) / 255, extractorPower);
    Serial.printf("  РЈРІР»Р°Р¶РЅРёС‚РµР»СЊ: %s\n", humidifierState.active ? "Р’РљР›" : "Р’Р«РљР›");
    Serial.printf("  Р’РµРЅС‚РёР»СЏС†РёСЏ: %s\n", ventState.open ? "РћРўРљР Р«РўРђ" : "Р—РђРљР Р«РўРђ");
    Serial.println("----------------------------------------------");
    Serial.printf("  Wi-Fi: %s (%d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
    Serial.printf("  РџР°РјСЏС‚СЊ: %lu KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("  Р—Р°РїРёСЃРµР№ РІ РёСЃС‚РѕСЂРёРё: %d\n", historyIndex);
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
}

// ============================================================================
// Р¤РЈРќРљР¦РРЇ РџРћР›РЈР§Р•РќРРЇ РўР•РљРЈР©Р•Р“Рћ Р Р•Р–РРњРђ
// ============================================================================

String getCurrentMode() {
    if (heatingState.emergencyMode) return "рџљЁ РђР’РђР РРЇ";
    if (heatingState.forceMode) return "рџ”Ґ Р¤РћР РЎРђР–";
    if (heatingState.manualMode) return "рџ‘† Р РЈР§РќРћР™";
    return "рџ¤– РђР’РўРћ";
}

// ============================================================================
// Р¤РЈРќРљР¦РРЇ Р РђРЎР§Р•РўРђ РђР”РђРџРўРР’РќРћР™ Р’Р›РђР–РќРћРЎРўР
// ============================================================================

void calculateAdaptiveHumidity(float tempRoom, float &minHum, float &maxHum) {
    // Р‘Р°Р·РѕРІС‹Рµ Р·РЅР°С‡РµРЅРёСЏ РёР· РєРѕРЅС„РёРіСѓСЂР°С†РёРё
    minHum = config.humidityConfig.minHumidity;
    maxHum = config.humidityConfig.maxHumidity;
    
    // Р•СЃР»Рё Р°РґР°РїС‚РёРІРЅС‹Р№ СЂРµР¶РёРј РІС‹РєР»СЋС‡РµРЅ, РёСЃРїРѕР»СЊР·СѓРµРј Р±Р°Р·РѕРІС‹Рµ Р·РЅР°С‡РµРЅРёСЏ
    if (!config.humidityConfig.adaptiveMode) {
        return;
    }
    
    // РљРѕСЂСЂРµРєС‚РёСЂСѓРµРј РІР»Р°Р¶РЅРѕСЃС‚СЊ РІ Р·Р°РІРёСЃРёРјРѕСЃС‚Рё РѕС‚ С‚РµРјРїРµСЂР°С‚СѓСЂС‹
    // Р§РµРј РІС‹С€Рµ С‚РµРјРїРµСЂР°С‚СѓСЂР°, С‚РµРј РЅРёР¶Рµ РґРѕР»Р¶РЅР° Р±С‹С‚СЊ РІР»Р°Р¶РЅРѕСЃС‚СЊ РґР»СЏ РєРѕРјС„РѕСЂС‚Р°
    float tempAdjustment = (tempRoom - 25.0f) * config.humidityConfig.tempCoefficient;
    
    minHum -= tempAdjustment;
    maxHum -= tempAdjustment;
    
    // РћРіСЂР°РЅРёС‡РёРІР°РµРј Р·РЅР°С‡РµРЅРёСЏ (РЅРµ РЅРёР¶Рµ 30% Рё РЅРµ РІС‹С€Рµ 80%)
    minHum = constrain(minHum, 30.0f, 75.0f);
    maxHum = constrain(maxHum, minHum + 5.0f, 80.0f);
    
    // Р”РѕР±Р°РІР»СЏРµРј РіРёСЃС‚РµСЂРµР·РёСЃ
    maxHum = minHum + config.humidityConfig.hysteresis;
}

// ============================================================================
// РўР•РЎРў Р’Р•РќРўРР›РЇР¦РР
// ============================================================================

void testVentilation() {
    Serial.println("рџ”§ РўРµСЃС‚ РІРµРЅС‚РёР»СЏС†РёРё: РѕС‚РєСЂС‹С‚РёРµ...");
    moveServoSmooth(SERVO_OPEN_ANGLE);
    delay(2000);
    Serial.println("рџ”§ РўРµСЃС‚ РІРµРЅС‚РёР»СЏС†РёРё: Р·Р°РєСЂС‹С‚РёРµ...");
    moveServoSmooth(SERVO_CLOSED_ANGLE);
    Serial.println("вњ… РўРµСЃС‚ РІРµРЅС‚РёР»СЏС†РёРё Р·Р°РІРµСЂС€РµРЅ");
}
