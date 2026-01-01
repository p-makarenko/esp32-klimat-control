#include "advanced_climate_logic.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "system_core.h"
#include "data_storage.h"
#include <Arduino.h>
#include "utility_functions.h"

// ============================================================================
// Р“Р›РћР‘РђР›Р¬РќР«Р• РџР•Р Р•РњР•РќРќР«Р• Р”Р›РЇ РЈРџР РђР’Р›Р•РќРРЇ Р Р•Р–РРњРђРњР
// ============================================================================

bool compactMode = true;
unsigned long lastModeSwitch = 0;
unsigned long lastPowerUpdate = 0;
const unsigned long POWER_UPDATE_INTERVAL = 1000;

// ============================================================================
// Р”Р’Рђ Р Р•Р–РРњРђ Р РђР‘РћРўР« SERIAL MONITOR
// ============================================================================

void printCompactMode() {
    Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.println("        рџљЂ Р Р•Р–РРњ Р‘Р«РЎРўР РћР“Рћ РЈРџР РђР’Р›Р•РќРРЇ");
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.println("Р‘Р«РЎРўР Р«Р• РљРћРњРђРќР”Р« (Р±СѓРєРІР° + С‡РёСЃР»Рѕ):");
    Serial.println("  aXX - РЅР°СЃРѕСЃ XX%        (РїСЂРёРјРµСЂ: a50)");
    Serial.println("  bXX - РІРµРЅС‚РёР»СЏС‚РѕСЂ XX%   (РїСЂРёРјРµСЂ: b40)");
    Serial.println("  cXX - РІС‹С‚СЏР¶РєР° XX%      (РїСЂРёРјРµСЂ: c30, c0)");
    Serial.println("\nРќРђРЎРўР РћР™РљР РџР Р•Р”Р•Р›РћР’:");
    Serial.println("  tmin XX - РјРёРЅ. С‚РµРјРї.   (РїСЂРёРјРµСЂ: tmin 25)");
    Serial.println("  tmax XX - РјР°РєСЃ. С‚РµРјРї.  (РїСЂРёРјРµСЂ: tmax 26)");
    Serial.println("  hmin XX - РјРёРЅ. РІР»Р°Р¶.   (РїСЂРёРјРµСЂ: hmin 65)");
    Serial.println("  hmax XX - РјР°РєСЃ. РІР»Р°Р¶.  (РїСЂРёРјРµСЂ: hmax 70)");
    Serial.println("\nРўРђР™РњР•Р  Р’Р«РўРЇР–РљР:");
    Serial.println("  ton XX  - РІРєР» РЅР° XX РјРёРЅ (РїСЂРёРјРµСЂ: ton 30)");
    Serial.println("  toff    - РІС‹РєР»СЋС‡РёС‚СЊ С‚Р°Р№РјРµСЂ");
    Serial.println("  tcycle X Y - С†РёРєР» X РјРёРЅ РІРєР», Y РјРёРЅ РІС‹РєР»");
    Serial.println("\nРЎРРЎРўР•РњРќР«Р•:");
    Serial.println("  s      - СЃС‚Р°С‚СѓСЃ СЃРёСЃС‚РµРјС‹");
    Serial.println("  m      - РїРѕР»РЅРѕРµ РјРµРЅСЋ");
    Serial.println("  mode   - СЃРјРµРЅРёС‚СЊ СЂРµР¶РёРј (РєРѕРјРїР°РєС‚РЅС‹Р№/РїРѕР»РЅС‹Р№)");
    Serial.println("  web    - РёРЅС„РѕСЂРјР°С†РёСЏ Рѕ РІРµР±-РёРЅС‚РµСЂС„РµР№СЃРµ");
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
}

void printExtendedMode() {
    Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.println("        вљ™пёЏ РџРћР›РќРћР• РЎРРЎРўР•РњРќРћР• РњР•РќР® v4.0");
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.println("РћРЎРќРћР’РќР«Р• РљРћРњРђРќР”Р«:");
    Serial.println("  status      - РЎС‚Р°С‚СѓСЃ СЃРёСЃС‚РµРјС‹");
    Serial.println("  menu        - РџРѕРєР°Р·Р°С‚СЊ РјРµРЅСЋ");
    Serial.println("  compact     - РџРµСЂРµР№С‚Рё РІ РєРѕРјРїР°РєС‚РЅС‹Р№ СЂРµР¶РёРј");
    Serial.println("  web         - РРЅС„РѕСЂРјР°С†РёСЏ Рѕ РІРµР±-РёРЅС‚РµСЂС„РµР№СЃРµ");
    Serial.println("  save        - РЎРѕС…СЂР°РЅРёС‚СЊ РЅР°СЃС‚СЂРѕР№РєРё");
    Serial.println("  reboot      - РџРµСЂРµР·Р°РіСЂСѓР·РєР°");
    
    Serial.println("\nРЈРџР РђР’Р›Р•РќРР• РњРћР©РќРћРЎРўР¬Р®:");
    Serial.println("  pump XX     - РќР°СЃРѕСЃ (0-100%)");
    Serial.println("  fan XX      - Р’РµРЅС‚РёР»СЏС‚РѕСЂ (0-100%)");
    Serial.println("  extractor XX- Р’С‹С‚СЏР¶РєР° (0-100%)");
    Serial.println("  auto        - РђРІС‚РѕРјР°С‚РёС‡РµСЃРєРёР№ СЂРµР¶РёРј");
    Serial.println("  manual      - Р СѓС‡РЅРѕР№ СЂРµР¶РёРј");
    Serial.println("  force       - Р¤РѕСЂСЃР°Р¶РЅС‹Р№ СЂРµР¶РёРј");
    
    Serial.println("\nРќРђРЎРўР РћР™РљР РўР•РњРџР•Р РђРўРЈР Р«:");
    Serial.println("  tmin XX     - РњРёРЅРёРјР°Р»СЊРЅР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР°");
    Serial.println("  tmax XX     - РњР°РєСЃРёРјР°Р»СЊРЅР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР°");
    Serial.println("  temp XX     - РЈСЃС‚Р°РЅРѕРІРёС‚СЊ РѕР±Рµ РіСЂР°РЅРёС†С‹ (РјРёРЅ=РјР°РєСЃ)");
    
    Serial.println("\nРќРђРЎРўР РћР™РљР Р’Р›РђР–РќРћРЎРўР:");
    Serial.println("  hmin XX     - РњРёРЅРёРјР°Р»СЊРЅР°СЏ РІР»Р°Р¶РЅРѕСЃС‚СЊ");
    Serial.println("  hmax XX     - РњР°РєСЃРёРјР°Р»СЊРЅР°СЏ РІР»Р°Р¶РЅРѕСЃС‚СЊ");
    Serial.println("  hum XX      - РЈСЃС‚Р°РЅРѕРІРёС‚СЊ РѕР±Рµ РіСЂР°РЅРёС†С‹ (РјРёРЅ=РјР°РєСЃ)");
    
    Serial.println("\nРўРђР™РњР•Р  Р’Р«РўРЇР–РљР:");
    Serial.println("  timer on XX - Р’РєР»СЋС‡РёС‚СЊ С‚Р°Р№РјРµСЂ РЅР° XX РјРёРЅСѓС‚");
    Serial.println("  timer off   - Р’С‹РєР»СЋС‡РёС‚СЊ С‚Р°Р№РјРµСЂ");
    Serial.println("  timer set XX YY - РЈСЃС‚Р°РЅРѕРІРёС‚СЊ С‚Р°Р№РјРµСЂ (РІРєР»/РІС‹РєР»)");
    Serial.println("  timer power XX - РњРѕС‰РЅРѕСЃС‚СЊ С‚Р°Р№РјРµСЂР° (10-100%)");
    
    Serial.println("\nР Р•Р–РРњР« РЈРџР РђР’Р›Р•РќРРЇ:");
    Serial.println("  mode auto   - РђРІС‚РѕРјР°С‚РёС‡РµСЃРєРёР№ СЂРµР¶РёРј");
    Serial.println("  mode manual - Р СѓС‡РЅРѕР№ СЂРµР¶РёРј");
    Serial.println("  mode compact- РљРѕРјРїР°РєС‚РЅС‹Р№ СЂРµР¶РёРј Serial");
    Serial.println("  mode full   - РџРѕР»РЅС‹Р№ СЂРµР¶РёРј Serial");
    
    Serial.println("\nРўР•РЎРўР«:");
    Serial.println("  test vent   - РўРµСЃС‚ РІРµРЅС‚РёР»СЏС†РёРё");
    Serial.println("  test pump   - РўРµСЃС‚ РЅР°СЃРѕСЃР° (10 СЃРµРє)");
    Serial.println("  test fan    - РўРµСЃС‚ РІРµРЅС‚РёР»СЏС‚РѕСЂР° (10 СЃРµРє)");
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
}

void toggleMode() {
    compactMode = !compactMode;
    if (compactMode) {
        Serial.println("\nвњ… РџРµСЂРµРєР»СЋС‡РµРЅРѕ РІ РљРћРњРџРђРљРўРќР«Р™ СЂРµР¶РёРј (Р±С‹СЃС‚СЂРѕРµ СѓРїСЂР°РІР»РµРЅРёРµ)");
        printCompactMode();
    } else {
        Serial.println("\nвњ… РџРµСЂРµРєР»СЋС‡РµРЅРѕ РІ Р РђРЎРЁРР Р•РќРќР«Р™ СЂРµР¶РёРј (РїРѕР»РЅС‹Рµ РЅР°СЃС‚СЂРѕР№РєРё)");
        printExtendedMode();
    }
    lastModeSwitch = millis();
}

// ============================================================================
// РћР‘Р РђР‘РћРўРљРђ РљРћРњРђРќР” Р’ РљРћРњРџРђРљРўРќРћРњ Р Р•Р–РРњР•
// ============================================================================

void processCompactCommand(String command) {
    command.toLowerCase();
    command.trim();
    
    if (command.length() < 1) return;
    
    // РљРѕРјР°РЅРґС‹ С‚РёРїР° "a50", "b40", "c30"
    if (command.length() >= 2 && command.length() <= 4) {
        char device = command[0];
        String valueStr = command.substring(1);
        int value = valueStr.toInt();
        
        if (value < 0 || value > 100) {
            Serial.println("вќЊ РћС€РёР±РєР°: Р·РЅР°С‡РµРЅРёРµ РґРѕР»Р¶РЅРѕ Р±С‹С‚СЊ 0-100");
            return;
        }
        
        switch(device) {
            case 'a':
                setPumpPercent(value);
                Serial.printf("вњ… РќР°СЃРѕСЃ: %d%%\n", value);
                break;
            case 'b':
                setFanPercent(value);
                Serial.printf("вњ… Р’РµРЅС‚РёР»СЏС‚РѕСЂ: %d%%\n", value);
                break;
            case 'c':
                setExtractorPercent(value);
                Serial.printf("вњ… Р’С‹С‚СЏР¶РєР°: %d%%\n", value);
                break;
            default:
                Serial.println("вќЊ РќРµРёР·РІРµСЃС‚РЅРѕРµ СѓСЃС‚СЂРѕР№СЃС‚РІРѕ. РСЃРїРѕР»СЊР·СѓР№С‚Рµ a, b РёР»Рё c");
        }
        return;
    }
    
    // РљРѕРјР°РЅРґС‹ РЅР°СЃС‚СЂРѕРµРє
    if (command.startsWith("tmin ")) {
        float temp = command.substring(5).toFloat();
        config.tempMin = temp;
        Serial.printf("вњ… РњРёРЅ. С‚РµРјРїРµСЂР°С‚СѓСЂР°: %.1fВ°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("tmax ")) {
        float temp = command.substring(5).toFloat();
        config.tempMax = temp;
        Serial.printf("вњ… РњР°РєСЃ. С‚РµРјРїРµСЂР°С‚СѓСЂР°: %.1fВ°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("hmin ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.minHumidity = hum;
        Serial.printf("вњ… РњРёРЅ. РІР»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("hmax ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.maxHumidity = hum;
        Serial.printf("вњ… РњР°РєСЃ. РІР»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("ton ")) {
        int minutes = command.substring(4).toInt();
        minutes = constrain(minutes, 1, 240);
        config.extractorTimer.onMinutes = minutes;
        config.extractorTimer.enabled = true;
        config.extractorTimer.cycleStart = millis();
        Serial.printf("вњ… РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р’РљР› РЅР° %d РјРёРЅ\n", minutes);
        saveConfiguration();
    }
    else if (command == "toff") {
        config.extractorTimer.enabled = false;
        setExtractorPercent(0);
        Serial.println("вњ… РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р’Р«РљР›");
        saveConfiguration();
    }
    else if (command.startsWith("tcycle ")) {
        String params = command.substring(7);
        int spaceIndex = params.indexOf(' ');
        if (spaceIndex > 0) {
            int onTime = params.substring(0, spaceIndex).toInt();
            int offTime = params.substring(spaceIndex + 1).toInt();
            
            onTime = constrain(onTime, 1, 120);
            offTime = constrain(offTime, 1, 120);
            
            config.extractorTimer.onMinutes = onTime;
            config.extractorTimer.offMinutes = offTime;
            config.extractorTimer.enabled = true;
            config.extractorTimer.cycleStart = millis();
            
            Serial.printf("вњ… РўР°Р№РјРµСЂ: %d РјРёРЅ Р’РљР› / %d РјРёРЅ Р’Р«РљР›\n", onTime, offTime);
            saveConfiguration();
        }
    }
    else if (command == "s") {
        autoPrintStatus();
    }
    else if (command == "m") {
        toggleMode();
    }
    else if (command == "mode") {
        toggleMode();
    }
    else if (command == "web") {
        Serial.println("\nрџЊђ Р’Р•Р‘-РРќРўР•Р Р¤Р•Р™РЎ:");
        Serial.printf("  РђРґСЂРµСЃ: http://%s\n", WiFi.localIP().toString().c_str());
        Serial.println("  РљРѕРјР°РЅРґРЅР°СЏ СЃС‚СЂРѕРєР° РґРѕСЃС‚СѓРїРЅР° РЅР° РіР»Р°РІРЅРѕР№ СЃС‚СЂР°РЅРёС†Рµ");
        Serial.println("  РќР°СЃС‚СЂРѕР№РєРё -> /settings");
        Serial.println("  РЈРїСЂР°РІР»РµРЅРёРµ -> /control");
    }
    else {
        Serial.println("вќЊ РќРµРёР·РІРµСЃС‚РЅР°СЏ РєРѕРјР°РЅРґР°. Р’РІРµРґРёС‚Рµ 'm' РґР»СЏ РјРµРЅСЋ");
    }
}

// ============================================================================
// РћР‘Р РђР‘РћРўРљРђ РљРћРњРђРќР” Р’ Р РђРЎРЁРР Р•РќРќРћРњ Р Р•Р–РРњР•
// ============================================================================

void processExtendedCommand(String command) {
    command.toLowerCase();
    command.trim();
    
    if (command == "status" || command == "s") {
        autoPrintStatus();
    }
    else if (command == "menu" || command == "m") {
        printExtendedMode();
    }
    else if (command == "compact") {
        compactMode = true;
        Serial.println("\nвњ… РџРµСЂРµРєР»СЋС‡РµРЅРѕ РІ РєРѕРјРїР°РєС‚РЅС‹Р№ СЂРµР¶РёРј");
        printCompactMode();
    }
    else if (command == "web") {
        Serial.println("\nрџЊђ Р’Р•Р‘-РРќРўР•Р Р¤Р•Р™РЎ:");
        Serial.printf("  РђРґСЂРµСЃ: http://%s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  РЎС‚Р°С‚СѓСЃ: /status\n");
        Serial.printf("  РЈРїСЂР°РІР»РµРЅРёРµ: /control\n");
        Serial.printf("  РќР°СЃС‚СЂРѕР№РєРё: /settings\n");
        Serial.printf("  РСЃС‚РѕСЂРёСЏ: /history\n");
    }
    else if (command == "save") {
        saveConfiguration();
        Serial.println("вњ… РќР°СЃС‚СЂРѕР№РєРё СЃРѕС…СЂР°РЅРµРЅС‹");
    }
    else if (command == "reboot") {
        Serial.println("рџ”„ РџРµСЂРµР·Р°РіСЂСѓР·РєР° СЃРёСЃС‚РµРјС‹...");
        delay(1000);
        ESP.restart();
    }
    else if (command.startsWith("pump ")) {
        int percent = command.substring(5).toInt();
        percent = constrain(percent, 0, 100);
        setPumpPercent(percent);
        Serial.printf("вњ… РќР°СЃРѕСЃ: %d%%\n", percent);
    }
    else if (command.startsWith("fan ")) {
        int percent = command.substring(4).toInt();
        percent = constrain(percent, 0, 100);
        setFanPercent(percent);
        Serial.printf("вњ… Р’РµРЅС‚РёР»СЏС‚РѕСЂ: %d%%\n", percent);
    }
    else if (command.startsWith("extractor ")) {
        int percent = command.substring(10).toInt();
        percent = constrain(percent, 0, 100);
        setExtractorPercent(percent);
        Serial.printf("вњ… Р’С‹С‚СЏР¶РєР°: %d%%\n", percent);
    }
    else if (command == "auto") {
        heatingState.manualMode = false;
        heatingState.forceMode = false;
        heatingState.emergencyMode = false;
        Serial.println("вњ… Р РµР¶РёРј: РђР’РўРћРњРђРўРР§Р•РЎРљРР™");
    }
    else if (command == "manual") {
        heatingState.manualMode = true;
        heatingState.forceMode = false;
        Serial.println("вњ… Р РµР¶РёРј: Р РЈР§РќРћР™");
    }
    else if (command == "force") {
        heatingState.forceMode = true;
        heatingState.manualMode = false;
        Serial.println("рџ”Ґ Р РµР¶РёРј: Р¤РћР РЎРђР–");
    }
    else if (command.startsWith("tmin ")) {
        float temp = command.substring(5).toFloat();
        config.tempMin = temp;
        Serial.printf("вњ… РњРёРЅ. С‚РµРјРїРµСЂР°С‚СѓСЂР°: %.1fВ°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("tmax ")) {
        float temp = command.substring(5).toFloat();
        config.tempMax = temp;
        Serial.printf("вњ… РњР°РєСЃ. С‚РµРјРїРµСЂР°С‚СѓСЂР°: %.1fВ°C\n", temp);
        saveConfiguration();
    }
    else if (command.startsWith("temp ")) {
        float temp = command.substring(5).toFloat();
        config.tempMin = temp;
        config.tempMax = temp + 1.0f;
        Serial.printf("вњ… РўРµРјРїРµСЂР°С‚СѓСЂР°: %.1f-%.1fВ°C\n", temp, temp + 1.0f);
        saveConfiguration();
    }
    else if (command.startsWith("hmin ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.minHumidity = hum;
        Serial.printf("вњ… РњРёРЅ. РІР»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("hmax ")) {
        float hum = command.substring(5).toFloat();
        config.humidityConfig.maxHumidity = hum;
        Serial.printf("вњ… РњР°РєСЃ. РІР»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%%\n", hum);
        saveConfiguration();
    }
    else if (command.startsWith("hum ")) {
        float hum = command.substring(4).toFloat();
        config.humidityConfig.minHumidity = hum;
        config.humidityConfig.maxHumidity = hum + 5.0f;
        Serial.printf("вњ… Р’Р»Р°Р¶РЅРѕСЃС‚СЊ: %.1f-%.1f%%\n", hum, hum + 5.0f);
        saveConfiguration();
    }
    else if (command.startsWith("timer on ")) {
        int minutes = command.substring(9).toInt();
        minutes = constrain(minutes, 1, 240);
        config.extractorTimer.enabled = true;
        config.extractorTimer.onMinutes = minutes;
        config.extractorTimer.offMinutes = 0;
        config.extractorTimer.cycleStart = millis();
        Serial.printf("вњ… РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р’РљР› РЅР° %d РјРёРЅ\n", minutes);
        saveConfiguration();
    }
    else if (command == "timer off") {
        config.extractorTimer.enabled = false;
        setExtractorPercent(0);
        Serial.println("вњ… РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р’Р«РљР›");
        saveConfiguration();
    }
    else if (command.startsWith("timer set ")) {
        String params = command.substring(10);
        int spaceIndex = params.indexOf(' ');
        if (spaceIndex > 0) {
            int onTime = params.substring(0, spaceIndex).toInt();
            int offTime = params.substring(spaceIndex + 1).toInt();
            
            onTime = constrain(onTime, 1, 120);
            offTime = constrain(offTime, 1, 120);
            
            config.extractorTimer.onMinutes = onTime;
            config.extractorTimer.offMinutes = offTime;
            config.extractorTimer.enabled = true;
            config.extractorTimer.cycleStart = millis();
            
            Serial.printf("вњ… РўР°Р№РјРµСЂ: %d РјРёРЅ Р’РљР› / %d РјРёРЅ Р’Р«РљР›\n", onTime, offTime);
            saveConfiguration();
        }
    }
    else if (command.startsWith("timer power ")) {
        int power = command.substring(12).toInt();
        power = constrain(power, 10, 100);
        config.extractorTimer.powerPercent = power;
        Serial.printf("вњ… РњРѕС‰РЅРѕСЃС‚СЊ С‚Р°Р№РјРµСЂР°: %d%%\n", power);
        saveConfiguration();
    }
    else if (command.startsWith("mode ")) {
        String mode = command.substring(5);
        if (mode == "auto") {
            heatingState.manualMode = false;
            heatingState.forceMode = false;
            Serial.println("вњ… Р РµР¶РёРј: РђР’РўРћРњРђРўРР§Р•РЎРљРР™");
        }
        else if (mode == "manual") {
            heatingState.manualMode = true;
            heatingState.forceMode = false;
            Serial.println("вњ… Р РµР¶РёРј: Р РЈР§РќРћР™");
        }
        else if (mode == "compact") {
            compactMode = true;
            Serial.println("вњ… Р РµР¶РёРј Serial: РљРћРњРџРђРљРўРќР«Р™");
            printCompactMode();
        }
        else if (mode == "full") {
            compactMode = false;
            Serial.println("вњ… Р РµР¶РёРј Serial: РџРћР›РќР«Р™");
            printExtendedMode();
        }
    }
    else if (command == "test vent") {
        testVentilation();
    }
    else if (command == "test pump") {
        Serial.println("рџ”§ РўРµСЃС‚ РЅР°СЃРѕСЃР°: 10 СЃРµРєСѓРЅРґ РЅР° 50%");
        setPumpPercent(50);
        delay(10000);
        setPumpPercent(0);
        Serial.println("вњ… РўРµСЃС‚ Р·Р°РІРµСЂС€РµРЅ");
    }
    else if (command == "test fan") {
        Serial.println("рџ”§ РўРµСЃС‚ РІРµРЅС‚РёР»СЏС‚РѕСЂР°: 10 СЃРµРєСѓРЅРґ РЅР° 50%");
        setFanPercent(50);
        delay(10000);
        setFanPercent(0);
        Serial.println("вњ… РўРµСЃС‚ Р·Р°РІРµСЂС€РµРЅ");
    }
    else {
        Serial.println("вќЊ РќРµРёР·РІРµСЃС‚РЅР°СЏ РєРѕРјР°РЅРґР°. Р’РІРµРґРёС‚Рµ 'menu' РґР»СЏ СЃРїРёСЃРєР° РєРѕРјР°РЅРґ");
    }
}

// ============================================================================
// РЈР›РЈР§РЁР•РќРќРђРЇ РћР‘Р РђР‘РћРўРљРђ РЎР•Р РР™РќР«РҐ РљРћРњРђРќР”
// ============================================================================

void processAdvancedSerialCommand() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.length() == 0) return;
    
    Serial.println("> " + command);
    
    if (compactMode) {
        processCompactCommand(command);
    } else {
        processExtendedCommand(command);
    }
}

// ============================================================================
// Р РђРЎРЁРР Р•РќРќРђРЇ Р›РћР“РРљРђ РЈРџР РђР’Р›Р•РќРРЇ РћР‘РћР“Р Р•Р’РћРњ
// ============================================================================

void smartHeatingControl() {
    if (!config.heatingEnabled) return;
    
    unsigned long now = millis();
    
    float tempRoom = 0, tempCarrier = 0, humidity = 0;
    
    if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
        tempRoom = sensorData.tempRoom;
        tempCarrier = sensorData.tempCarrier;
        humidity = sensorData.humidity;
        xSemaphoreGive(getSensorMutex());
    }
    
    // РџСЂРѕРІРµСЂСЏРµРј Р°РІР°СЂРёР№РЅС‹Рµ СЂРµР¶РёРјС‹
    if (tempRoom <= TEMP_EMERGENCY_LOW) {
        if (!heatingState.emergencyMode) {
            setEmergencyStartTime(millis());
            setEmergencyStartTempCarrier(tempCarrier);
            setEmergencyStartTempRoom(tempRoom);
        }
        
        heatingState.emergencyMode = true;
        setPumpPercent(100);
        setFanPercent(100);
        Serial.println("рџљЁ РђР’РђР РР™РќР«Р™ Р Р•Р–РРњ: РљСЂРёС‚РёС‡РµСЃРєРё РЅРёР·РєР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР°!");
        return;
    }
    
    if (tempRoom <= TEMP_CRITICAL_LOW) {
        heatingState.forceMode = true;
        setPumpPercent(80);
        setFanPercent(80);
        Serial.println("вљ  РљР РРўРР§Р•РЎРљРР™ Р Р•Р–РРњ: РќРёР·РєР°СЏ С‚РµРјРїРµСЂР°С‚СѓСЂР°!");
        return;
    }
    
    // РђРґР°РїС‚РёРІРЅРѕРµ СѓРїСЂР°РІР»РµРЅРёРµ РѕР±РѕРіСЂРµРІРѕРј
    if (tempRoom < config.tempMin) {
        float tempDiff = config.tempMin - tempRoom;
        int pumpPower = map(constrain(tempDiff * 10, 0, 20), 0, 20, config.pumpMinPercent, config.pumpMaxPercent);
        int fanPower = map(constrain(tempDiff * 10, 0, 20), 0, 20, config.fanMinPercent, config.fanMaxPercent);
        
        setPumpPercent(pumpPower);
        setFanPercent(fanPower);
        
        if (now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
            Serial.printf("рџ”Ґ РћР±РѕРіСЂРµРІ: T=%.1fВ°C, РќР°СЃРѕСЃ=%d%%, Р’РµРЅС‚РёР»СЏС‚РѕСЂ=%d%%\n", 
                         tempRoom, pumpPower, fanPower);
            lastPowerUpdate = now;
        }
    }
    else if (tempRoom > config.tempMax) {
        setPumpPercent(0);
        setFanPercent(config.fanMinPercent);
        
        if (now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
            Serial.printf("вќ„ РћС…Р»Р°Р¶РґРµРЅРёРµ: T=%.1fВ°C, РќР°СЃРѕСЃ=0%%, Р’РµРЅС‚РёР»СЏС‚РѕСЂ=%d%%\n", 
                         tempRoom, config.fanMinPercent);
            lastPowerUpdate = now;
        }
    }
    else {
        float tempRange = config.tempMax - config.tempMin;
        float tempPosition = (tempRoom - config.tempMin) / tempRange;
        
        int pumpPower = map(tempPosition * 100, 0, 100, config.pumpMinPercent, config.pumpMinPercent / 2);
        int fanPower = map(tempPosition * 100, 0, 100, 40, config.fanMinPercent);
        
        pumpPower = constrain(pumpPower, config.pumpMinPercent / 2, config.pumpMinPercent);
        fanPower = constrain(fanPower, config.fanMinPercent, 40);
        
        setPumpPercent(pumpPower);
        setFanPercent(fanPower);
        
        if (now - lastPowerUpdate > POWER_UPDATE_INTERVAL) {
            Serial.printf("вљ– РџРѕРґРґРµСЂР¶Р°РЅРёРµ: T=%.1fВ°C, РќР°СЃРѕСЃ=%d%%, Р’РµРЅС‚РёР»СЏС‚РѕСЂ=%d%%\n", 
                         tempRoom, pumpPower, fanPower);
            lastPowerUpdate = now;
        }
    }
}

// ============================================================================
// РЈР›РЈР§РЁР•РќРќР«Р™ РљРћРќРўР РћР›Р¬ Р’Р›РђР–РќРћРЎРўР
// ============================================================================

void advancedHumidityControl(float humidity, float tempRoom) {
    if (!config.humidifierEnabled || !config.humidityConfig.enabled) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifierState.active = false;
        return;
    }
    
    unsigned long now = millis();
    
    float adaptiveHumMin, adaptiveHumMax;
    calculateAdaptiveHumidity(tempRoom, adaptiveHumMin, adaptiveHumMax);
    
    if (humidifierState.active && 
        now - humidifierState.startTime > config.humidityConfig.maxRunTime) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifierState.active = false;
        Serial.println("вљ  РЈРІР»Р°Р¶РЅРёС‚РµР»СЊ: Р°РІС‚РѕРјР°С‚РёС‡РµСЃРєРё РІС‹РєР»СЋС‡РµРЅ С‡РµСЂРµР· РјР°РєСЃРёРјР°Р»СЊРЅРѕРµ РІСЂРµРјСЏ СЂР°Р±РѕС‚С‹");
        return;
    }
    
    if (!humidifierState.active && 
        now - humidifierState.lastCycle < config.humidityConfig.minInterval) {
        return;
    }
    
    if (!humidifierState.active && humidity < adaptiveHumMin) {
        digitalWrite(HUMIDIFIER_PIN, HIGH);
        humidifierState.active = true;
        humidifierState.startTime = now;
        humidifierState.cyclesToday++;
        Serial.printf("вњ… РЈРІР»Р°Р¶РЅРёС‚РµР»СЊ: Р’РљР› (Р’Р»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%%, Р¦РµР»СЊ: %.1f%%)\n", 
                     humidity, adaptiveHumMin);
    } 
    else if (humidifierState.active && humidity > adaptiveHumMax) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifierState.active = false;
        humidifierState.lastCycle = now;
        Serial.printf("вњ… РЈРІР»Р°Р¶РЅРёС‚РµР»СЊ: Р’Р«РљР› (Р’Р»Р°Р¶РЅРѕСЃС‚СЊ: %.1f%%, Р¦РµР»СЊ: %.1f%%)\n", 
                     humidity, adaptiveHumMax);
    }
}

// ============================================================================
// РЈР›РЈР§РЁР•РќРќР«Р™ РўРђР™РњР•Р  Р’Р«РўРЇР–РљР
// ============================================================================

void advancedUpdateExtractorTimer() {
    if (!config.extractorTimer.enabled) {
        return;
    }
    
    unsigned long now = millis();
    
    if (config.extractorTimer.cycleStart == 0) {
        config.extractorTimer.cycleStart = now;
        config.extractorTimer.state = true;
        config.extractorTimer.lastChange = now;
        
        setExtractorPercent(config.extractorTimer.powerPercent);
        Serial.println("вЏ° РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р—Р°РїСѓС‰РµРЅ, РІС‹С‚СЏР¶РєР° РІРєР»СЋС‡РµРЅР°");
    }
    
    unsigned long cycleTime = now - config.extractorTimer.cycleStart;
    unsigned long cycleDuration = (config.extractorTimer.onMinutes + 
                                  config.extractorTimer.offMinutes) * 60000;
    
    bool shouldBeOn = true;
    
    if (cycleDuration == 0) {
        if (cycleTime > config.extractorTimer.onMinutes * 60000) {
            config.extractorTimer.cycleStart = now;
            cycleTime = 0;
        }
        shouldBeOn = true;
    } else {
        if (cycleTime >= cycleDuration) {
            config.extractorTimer.cycleStart = now;
            cycleTime = 0;
        }
        
        shouldBeOn = (cycleTime < (config.extractorTimer.onMinutes * 60000));
    }
    
    if (shouldBeOn != config.extractorTimer.state) {
        config.extractorTimer.state = shouldBeOn;
        config.extractorTimer.lastChange = now;
        
        if (shouldBeOn) {
            setExtractorPercent(config.extractorTimer.powerPercent);
            Serial.println("вЏ° РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р’С‹С‚СЏР¶РєР° РІРєР»СЋС‡РµРЅР°");
        } else {
            setExtractorPercent(0);
            Serial.println("вЏ° РўР°Р№РјРµСЂ РІС‹С‚СЏР¶РєРё: Р’С‹С‚СЏР¶РєР° РІС‹РєР»СЋС‡РµРЅР°");
        }
    }
}

// ============================================================================
// РњРћРќРРўРћР РРќР“ Р Р”РРђР“РќРћРЎРўРРљРђ
// ============================================================================

void monitorSystemHealth() {
    static unsigned long lastHealthCheck = 0;
    unsigned long now = millis();
    
    if (now - lastHealthCheck < 60000) return;
    lastHealthCheck = now;
    
    if (!sensorData.carrierValid || !sensorData.roomValid) {
        Serial.println("вљ  Р’РќРРњРђРќРР•: РџСЂРѕР±Р»РµРјР° СЃ РґР°С‚С‡РёРєР°РјРё С‚РµРјРїРµСЂР°С‚СѓСЂС‹!");
    }
    
    if (!sensorData.bmeValid) {
        Serial.println("вљ  Р’РќРРњРђРќРР•: РџСЂРѕР±Р»РµРјР° СЃ РґР°С‚С‡РёРєРѕРј BME280!");
    }
    
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {
        Serial.printf("вљ  Р’РќРРњРђРќРР•: РњР°Р»Рѕ СЃРІРѕР±РѕРґРЅРѕР№ РїР°РјСЏС‚Рё: %d Р±Р°Р№С‚\n", freeHeap);
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("вљ  Р’РќРРњРђРќРР•: РџРѕС‚РµСЂСЏРЅРѕ СЃРѕРµРґРёРЅРµРЅРёРµ Wi-Fi!");
    }
    
    extern int historyIndex;
    Serial.printf("рџ“Љ РЎС‚Р°С‚РёСЃС‚РёРєР°: РџР°РјСЏС‚СЊ=%dKB, Р—Р°РїРёСЃРµР№=%d, Р’СЂРµРјСЏ=%luСЃРµРє\n",
                 freeHeap / 1024, historyIndex, now / 1000);
}

// ============================================================================
// РРќРР¦РРђР›РР—РђР¦РРЇ Р РђРЎРЁРР Р•РќРќРћР™ Р›РћР“РРљР
// ============================================================================

void initAdvancedLogic() {
    Serial.println("вњ“ Р Р°СЃС€РёСЂРµРЅРЅР°СЏ Р»РѕРіРёРєР° РёРЅРёС†РёР°Р»РёР·РёСЂРѕРІР°РЅР°");
    
    compactMode = true;
    lastModeSwitch = millis();
    lastPowerUpdate = millis();
    
    config.extractorTimer.cycleStart = 0;
    config.extractorTimer.state = false;
    config.extractorTimer.lastChange = 0;
    
    Serial.println("вњ… Р”РІР° СЂРµР¶РёРјР° Serial Monitor РіРѕС‚РѕРІС‹ Рє СЂР°Р±РѕС‚Рµ");
    printCompactMode();
}

// ============================================================================
// РћРЎРќРћР’РќРђРЇ Р—РђР”РђР§Рђ Р РђРЎРЁРР Р•РќРќРћР™ Р›РћР“РРљР
// ============================================================================

void advancedLogicTask(void *parameter) {
    Serial.println("вњ“ Р—Р°РґР°С‡Р° СЂР°СЃС€РёСЂРµРЅРЅРѕР№ Р»РѕРіРёРєРё Р·Р°РїСѓС‰РµРЅР°");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    while (1) {
        float tempRoom = 0, tempCarrier = 0, humidity = 0;
        
        if (xSemaphoreTake(getSensorMutex(), portMAX_DELAY)) {
            tempRoom = sensorData.tempRoom;
            tempCarrier = sensorData.tempCarrier;
            humidity = sensorData.humidity;
            xSemaphoreGive(getSensorMutex());
        }
        
        if (!heatingState.manualMode && !heatingState.forceMode && !heatingState.emergencyMode) {
            smartHeatingControl();
        }
        
        advancedHumidityControl(humidity, tempRoom);
        
        advancedUpdateExtractorTimer();
        
        monitorSystemHealth();
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
