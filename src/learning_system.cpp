#include "learning_system.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include "global_declarations.h"

Preferences prefs;
// ============================================================================
// Р“Р›РћР‘РђР›Р¬РќР«Р• РџР•Р Р•РњР•РќРќР«Р•
// ============================================================================

LearningEntry learnings[MAX_LEARNINGS];
int learningCount = 0;
PumpMode currentPumpMode = PUMP_OFF;
bool learningEnabled = true;

bool isLearningActive = false;
unsigned long learningStartTime = 0;
float learningStartTemp = 0;
uint8_t learningCurrentFan = 0;
uint8_t learningCurrentPump = 0;
// ============================================================================
// РРќРР¦РРђР›РР—РђР¦РРЇ Р РЎРћРҐР РђРќР•РќРР•
// ============================================================================

void initLearningSystem() {
    prefs.begin("learning", true);
    
    learningCount = prefs.getUInt("count", 0);
    
    Serial.printf("рџ“љ Р—Р°РІР°РЅС‚Р°Р¶РµРЅРѕ %d Р·Р°РїРёСЃС–РІ РЅР°РІС‡Р°РЅРЅСЏ\n", learningCount);
    
    for (int i = 0; i < learningCount && i < MAX_LEARNINGS; i++) {
        String key = String(i);
        learnings[i].roomError = prefs.getFloat((key + "_err").c_str(), 0);
        learnings[i].fanPercent = prefs.getUChar((key + "_fan").c_str(), 0);
        learnings[i].pumpPercent = prefs.getUChar((key + "_pump").c_str(), 0);
        learnings[i].uses = prefs.getUChar((key + "_uses").c_str(), 0);
        learnings[i].lastUsed = prefs.getULong((key + "_time").c_str(), 0);
    }
    
    prefs.end();
    
    cleanupOldLearnings();
}

void saveLearningData() {
    prefs.begin("learning", false);
    
    prefs.putUInt("count", learningCount);
    
    for (int i = 0; i < learningCount; i++) {
        String key = String(i);
        prefs.putFloat((key + "_err").c_str(), learnings[i].roomError);
        prefs.putUChar((key + "_fan").c_str(), learnings[i].fanPercent);
        prefs.putUChar((key + "_pump").c_str(), learnings[i].pumpPercent);
        prefs.putUChar((key + "_uses").c_str(), learnings[i].uses);
        prefs.putULong((key + "_time").c_str(), learnings[i].lastUsed);
    }
    
    prefs.end();
    
    Serial.printf("рџ’ѕ Р—Р±РµСЂРµР¶РµРЅРѕ %d Р·Р°РїРёСЃС–РІ РЅР°РІС‡Р°РЅРЅСЏ\n", learningCount);
}

void loadLearningData() {
    // РђР»РёР°СЃ РґР»СЏ СЃРѕРІРјРµСЃС‚РёРјРѕСЃС‚Рё
    initLearningSystem();
}

// ============================================================================
// РџРћРРЎРљ РћРџРўРРњРђР›Р¬РќР«РҐ РќРђРЎРўР РћР•Рљ
// ============================================================================

bool findOptimalSettings(float tempError, uint8_t &fanOut, uint8_t &pumpOut) {
    if (learningCount == 0 || !learningEnabled) return false;
    
    int bestIdx = -1;
    float bestDiff = 999.0f;
    
    for (int i = 0; i < learningCount; i++) {
        float diff = fabs(learnings[i].roomError - tempError);
        if (diff < bestDiff && diff <= LEARN_TEMP_EPS) {
            bestDiff = diff;
            bestIdx = i;
        }
    }
    
    if (bestIdx == -1) return false;
    
    fanOut = learnings[bestIdx].fanPercent;
    pumpOut = learnings[bestIdx].pumpPercent;
    
    learnings[bestIdx].uses++;
    learnings[bestIdx].lastUsed = getCurrentTime();
    
    Serial.printf("рџ“љ Р—РЅР°Р№РґРµРЅРѕ РЅР°Р»Р°С€С‚СѓРІР°РЅРЅСЏ: err=%.1fВ°C в†’ fan=%d%%, pump=%d%% (РІРёРєРѕСЂРёСЃС‚Р°РЅРѕ %d СЂР°Р·С–РІ)\n",
                  learnings[bestIdx].roomError, fanOut, pumpOut, learnings[bestIdx].uses);
    
    return true;
}

void updateLearningSystem(float currentTemp, float targetTemp) {
    float tempError = targetTemp - currentTemp;
    
    if (isLearningActive) {
        unsigned long learningTime = (millis() - learningStartTime) / 1000;
        float tempChange = fabs(currentTemp - learningStartTemp);
        
        if (learningTime >= LEARN_MIN_STABLE_TIME) {
            if (tempChange < 0.5f) {
                recordLearning(tempError, learningCurrentFan, learningCurrentPump, true);
                Serial.println("вњ… РќР°РІС‡Р°РЅРЅСЏ СѓСЃРїС–С€РЅРѕ Р·Р°РІРµСЂС€РµРЅРѕ!");
            } else {
                Serial.println("вќЊ РќР°РІС‡Р°РЅРЅСЏ СЃРєР°СЃРѕРІР°РЅРѕ: С‚РµРјРїРµСЂР°С‚СѓСЂР° РЅРµ СЃС‚Р°Р±С–Р»С–Р·СѓРІР°Р»Р°СЃСЏ");
            }
            isLearningActive = false;
        }
    }
}

void recordLearning(float tempError, uint8_t fan, uint8_t pump, bool success) {
    if (!success || !isValidLearning(tempError, fan, pump)) return;
    
    for (int i = 0; i < learningCount; i++) {
        if (fabs(learnings[i].roomError - tempError) < LEARN_TEMP_EPS * 0.3f) {
            learnings[i].fanPercent = (learnings[i].fanPercent + fan) / 2;
            learnings[i].pumpPercent = (learnings[i].pumpPercent + pump) / 2;
            learnings[i].uses++;
            learnings[i].lastUsed = getCurrentTime();
            
            Serial.printf("рџ“љ РћРЅРѕРІР»РµРЅРѕ Р·Р°РїРёСЃ %d: err=%.1fВ°C fan=%d%% pump=%d%%\n",
                          i, tempError, learnings[i].fanPercent, learnings[i].pumpPercent);
            
            saveLearningData();
            mergeSimilarLearnings();
            return;
        }
    }
    
    if (learningCount < MAX_LEARNINGS) {
        learnings[learningCount] = {
    tempError,
    fan,
    pump,
    1,
    (uint32_t)getCurrentTime()  // <-- РґРѕР±Р°РІСЊС‚Рµ РїСЂРёРІРµРґРµРЅРёРµ С‚РёРїР°
};
        learningCount++;
        
        Serial.printf("рџ“љ Р”РѕРґР°РЅРѕ РЅРѕРІРёР№ Р·Р°РїРёСЃ: err=%.1fВ°C fan=%d%% pump=%d%%\n",
                      tempError, fan, pump);
        
        saveLearningData();
        mergeSimilarLearnings();
    } else {
        Serial.println("вљ  Р‘Р°Р·Р° РЅР°РІС‡Р°РЅРЅСЏ РїРµСЂРµРїРѕРІРЅРµРЅР°!");
    }
}

// ============================================================================
// Р¤РЈРќРљР¦РР Р”РћРЎРўРЈРџРђ
// ============================================================================

bool getLearningActive() { return isLearningActive; }
void setLearningActive(bool active) { isLearningActive = active; }
unsigned long getLearningStartTime() { return learningStartTime; }
float getLearningStartTemp() { return learningStartTemp; }
uint8_t getLearningCurrentFan() { return learningCurrentFan; }
uint8_t getLearningCurrentPump() { return learningCurrentPump; }

void startLearning(uint8_t fanPower, uint8_t pumpPower) {
    isLearningActive = true;
    learningStartTime = millis();
    learningStartTemp = sensorData.tempRoom;
    learningCurrentFan = fanPower;
    learningCurrentPump = pumpPower;
}

void stopLearning() {
    isLearningActive = false;
}

// ============================================================================
// РЈРўРР›РРўР«
// ============================================================================

bool isValidLearning(float err, uint8_t fan, uint8_t pump) {
    if (fan < FAN_MIN_WORK || fan > 100) return false;
    if (pump < PUMP_MIN_WORK || pump > 100) return false;
    if (fabs(err) > 10.0f) return false;
    
    static float lastStableTemp = NAN;
    static unsigned long lastCheck = 0;
    
    if (!isnan(lastStableTemp) && millis() - lastCheck < 60000) {
        float tempChange = sensorData.tempRoom - lastStableTemp;
        if (tempChange < -0.5f) return false;
    }
    
    lastStableTemp = sensorData.tempRoom;
    lastCheck = millis();
    
    return true;
}

void mergeSimilarLearnings() {
    bool merged = false;
    
    for (int i = 0; i < learningCount; i++) {
        for (int j = i + 1; j < learningCount; j++) {
            if (fabs(learnings[i].roomError - learnings[j].roomError) < LEARN_TEMP_EPS * 0.5f) {
                learnings[i].fanPercent = (learnings[i].fanPercent + learnings[j].fanPercent) / 2;
                learnings[i].pumpPercent = (learnings[i].pumpPercent + learnings[j].pumpPercent) / 2;
                learnings[i].uses += learnings[j].uses;
                
                for (int k = j; k < learningCount - 1; k++) {
                    learnings[k] = learnings[k + 1];
                }
                learningCount--;
                j--;
                merged = true;
            }
        }
    }
    
    if (merged) {
        saveLearningData();
        Serial.println("рџ”„ РћР±'С”РґРЅР°РЅРѕ СЃС…РѕР¶С– Р·Р°РїРёСЃРё РЅР°РІС‡Р°РЅРЅСЏ");
    }
}

void cleanupOldLearnings() {
    time_t nowTime = getCurrentTime();
    int removed = 0;
    
    for (int i = 0; i < learningCount; i++) {
        if ((nowTime - learnings[i].lastUsed > 7776000) && learnings[i].uses < 3) {
            for (int j = i; j < learningCount - 1; j++) {
                learnings[j] = learnings[j + 1];
            }
            learningCount--;
            i--;
            removed++;
        }
    }
    
    if (removed > 0) {
        saveLearningData();
        Serial.printf("рџ§№ РћС‡РёС‰РµРЅРѕ %d Р·Р°СЃС‚Р°СЂС–Р»РёС… Р·Р°РїРёСЃС–РІ РЅР°РІС‡Р°РЅРЅСЏ\n", removed);
    }
}

void printLearningStats() {
    Serial.println("\nв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.println("           рџ“Љ РЎРўРђРўРРЎРўРРљРђ РќРђР’Р§РђРќРќРЇ");
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ");
    Serial.printf("Р—Р°РїРёСЃС–РІ Сѓ Р±Р°Р·С–: %d/%d\n", learningCount, MAX_LEARNINGS);
    Serial.println("----------------------------------------------");
    
    for (int i = 0; i < learningCount; i++) {
        char timeStr[20];
        time_t lastUsedTime = learnings[i].lastUsed;
        struct tm *tm_info = localtime(&lastUsedTime);
        strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M", tm_info);
        
        Serial.printf(" %2d: err=%+5.1fВ°C в†’ fan=%3d%%, pump=%3d%% | uses=%3d | %s\n",
                      i, learnings[i].roomError, learnings[i].fanPercent,
                      learnings[i].pumpPercent, learnings[i].uses, timeStr);
    }
    
    if (learningCount == 0) {
        Serial.println(" Р‘Р°Р·Р° РґР°РЅРёС… РїРѕСЂРѕР¶РЅСЏ. РЈРІС–РјРєРЅС–С‚СЊ СЂСѓС‡РЅРёР№ СЂРµР¶РёРј РґР»СЏ РЅР°РІС‡Р°РЅРЅСЏ.");
    }
    
    Serial.println("в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ\n");
}

// ============================================================================
// РЎР•Р РР™РќР«Р• РљРћРњРђРќР”Р«
// ============================================================================

void processLearningCommand(const String &cmd) {
    if (cmd == "learn stats") {
        printLearningStats();
    }
    else if (cmd == "learn export") {
        Serial.println("\nрџ“Љ Р•РљРЎРџРћР Рў Р”РђРќРРҐ РќРђР’Р§РђРќРќРЇ (CSV):");
        Serial.println("roomError,fanPercent,pumpPercent,uses,lastUsed");
        for (int i = 0; i < learningCount; i++) {
            Serial.printf("%.2f,%d,%d,%d,%lu\n",
                          learnings[i].roomError,
                          learnings[i].fanPercent,
                          learnings[i].pumpPercent,
                          learnings[i].uses,
                          learnings[i].lastUsed);
        }
        Serial.println("\nвњ… Р”Р°РЅС– РµРєСЃРїРѕСЂС‚РѕРІР°РЅРѕ Сѓ С„РѕСЂРјР°С‚С– CSV");
    }
    else if (cmd == "learn clear") {
        learningCount = 0;
        saveLearningData();
        Serial.println("вњ… Р‘Р°Р·Сѓ РЅР°РІС‡Р°РЅРЅСЏ РѕС‡РёС‰РµРЅРѕ");
    }
    else if (cmd == "learn enable") {
        learningEnabled = true;
        Serial.println("вњ… РќР°РІС‡Р°РЅРЅСЏ СѓРІС–РјРєРЅРµРЅРѕ");
    }
    else if (cmd == "learn disable") {
        learningEnabled = false;
        Serial.println("вЏёпёЏ РќР°РІС‡Р°РЅРЅСЏ РІРёРјРєРЅРµРЅРѕ");
    }
    else if (cmd.startsWith("learn add")) {
        float err = 0;
        uint8_t fan = 0, pump = 0;
        
        int errPos = cmd.indexOf("err=");
        int fanPos = cmd.indexOf("fan=");
        int pumpPos = cmd.indexOf("pump=");
        
        if (errPos != -1) err = cmd.substring(errPos + 4, fanPos).toFloat();
        if (fanPos != -1) fan = cmd.substring(fanPos + 4, pumpPos).toInt();
        if (pumpPos != -1) pump = cmd.substring(pumpPos + 5).toInt();
        
        if (isValidLearning(err, fan, pump)) {
            recordLearning(err, fan, pump, true);
            Serial.printf("вњ… Р”РѕРґР°РЅРѕ Р·Р°РїРёСЃ: err=%.1fВ°C fan=%d%% pump=%d%%\n", err, fan, pump);
        } else {
            Serial.println("вќЊ РќРµРІС–СЂРЅС– РїР°СЂР°РјРµС‚СЂРё РґР»СЏ РЅР°РІС‡Р°РЅРЅСЏ");
        }
    }
    else if (cmd == "learn start") {
        if (!isLearningActive) {
            isLearningActive = true;
            learningStartTime = millis();
            learningStartTemp = sensorData.tempRoom;
            learningCurrentFan = (heatingState.fanPower * 100) / 255;
            learningCurrentPump = (heatingState.pumpPower * 100) / 255;
            Serial.println("рџ“љ Р РѕР·РїРѕС‡Р°С‚Рѕ РїСЂРѕС†РµСЃ РЅР°РІС‡Р°РЅРЅСЏ (2 С…РІРёР»РёРЅРё)");
        }
    }
    else if (cmd == "learn stop") {
        isLearningActive = false;
        Serial.println("вЏ№пёЏ РџСЂРѕС†РµСЃ РЅР°РІС‡Р°РЅРЅСЏ Р·СѓРїРёРЅРµРЅРѕ");
    }
    else {
        Serial.println("вќЊ РќРµРІС–РґРѕРјР° РєРѕРјР°РЅРґР° РЅР°РІС‡Р°РЅРЅСЏ");
    }
}

