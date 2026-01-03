#include "learning_system.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include "global_declarations.h"

Preferences prefs;

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ
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
// ІНІЦІАЛІЗАЦІЯ І ЗБЕРЕЖЕННЯ
// ============================================================================

void initLearningSystem() {
    prefs.begin("learning", true);
    
    learningCount = prefs.getUInt("count", 0);
    
    Serial.printf("📚 Завантажено %d записів навчання\n", learningCount);
    
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
    
    Serial.printf("💾 Збережено %d записів навчання\n", learningCount);
}

void loadLearningData() {
    // Аліас для сумісності
    initLearningSystem();
}

// ============================================================================
// ОСНОВНА ЛОГІКА НАВЧАННЯ
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
    
    Serial.printf("🔍 Знайдено оптимальні налаштування: err=%.1f°C → fan=%d%%, pump=%d%% (використано %d разів)\n",
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
                Serial.println("✓ Навчання успішно завершено!");
            } else {
                Serial.println("✗ Навчання скасовано: температура не стабілізувалася");
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
            
            Serial.printf("📝 Оновлено запис %d: err=%.1f°C fan=%d%% pump=%d%%\n",
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
            (uint32_t)getCurrentTime()
        };
        learningCount++;
        
        Serial.printf("📝 Додано новий запис: err=%.1f°C fan=%d%% pump=%d%%\n",
                      tempError, fan, pump);
        
        saveLearningData();
        mergeSimilarLearnings();
    } else {
        Serial.println("⚠ База навчання переповнена!");
    }
}

// ============================================================================
// ФУНКЦІЇ КЕРУВАННЯ
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
// УТИЛІТИ
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
        Serial.println("🔄 Об'єднано схожі записи навчання");
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
        Serial.printf("🧹 Очищено %d застарілих записів навчання\n", removed);
    }
}

void printLearningStats() {
    Serial.println("\n══════════════════════════════════════════════════════════");
    Serial.println("           📊 СТАТИСТИКА НАВЧАННЯ");
    Serial.println("══════════════════════════════════════════════════════════");
    Serial.printf("Записів у базі: %d/%d\n", learningCount, MAX_LEARNINGS);
    Serial.println("----------------------------------------------");
    
    for (int i = 0; i < learningCount; i++) {
        char timeStr[20];
        time_t lastUsedTime = learnings[i].lastUsed;
        struct tm *tm_info = localtime(&lastUsedTime);
        strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M", tm_info);
        
        Serial.printf(" %2d: err=%+5.1f°C → fan=%3d%%, pump=%3d%% | uses=%3d | %s\n",
                      i, learnings[i].roomError, learnings[i].fanPercent,
                      learnings[i].pumpPercent, learnings[i].uses, timeStr);
    }
    
    if (learningCount == 0) {
        Serial.println(" База даних порожня. Використовуйте ручний режим для навчання.");
    }
    
    Serial.println("══════════════════════════════════════════════════════════\n");
}

// ============================================================================
// СЕРІЙНІ КОМАНДИ
// ============================================================================

void processLearningCommand(const String &cmd) {
    if (cmd == "learn stats") {
        printLearningStats();
    }
    else if (cmd == "learn export") {
        Serial.println("\n📊 ЕКСПОРТ ДАНИХ НАВЧАННЯ (CSV):");
        Serial.println("roomError,fanPercent,pumpPercent,uses,lastUsed");
        for (int i = 0; i < learningCount; i++) {
            Serial.printf("%.2f,%d,%d,%d,%lu\n",
                          learnings[i].roomError,
                          learnings[i].fanPercent,
                          learnings[i].pumpPercent,
                          learnings[i].uses,
                          learnings[i].lastUsed);
        }
        Serial.println("\n✓ Дані експортовані у форматі CSV");
    }
    else if (cmd == "learn clear") {
        learningCount = 0;
        saveLearningData();
        Serial.println("✓ База навчання очищена");
    }
    else if (cmd == "learn enable") {
        learningEnabled = true;
        Serial.println("✓ Навчання увімкнено");
    }
    else if (cmd == "learn disable") {
        learningEnabled = false;
        Serial.println("🔇 Навчання вимкнено");
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
            Serial.printf("✓ Додано запис: err=%.1f°C fan=%d%% pump=%d%%\n", err, fan, pump);
        } else {
            Serial.println("✗ Неправильні параметри для навчання");
        }
    }
    else if (cmd == "learn start") {
        if (!isLearningActive) {
            isLearningActive = true;
            learningStartTime = millis();
            learningStartTemp = sensorData.tempRoom;
            learningCurrentFan = (heatingState.fanPower * 100) / 255;
            learningCurrentPump = (heatingState.pumpPower * 100) / 255;
            Serial.println("🔍 Почато процес навчання (2 хвилини)");
        }
    }
    else if (cmd == "learn stop") {
        isLearningActive = false;
        Serial.println("⏹ Процес навчання зупинено");
    }
    else {
        Serial.println("✗ Невідома команда навчання");
    }
}