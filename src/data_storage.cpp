#include "data_storage.h"
#include "system_core.h"
#include <Arduino.h>
#include "advanced_climate_logic.h"
#include "utility_functions.h"
// Р—РјС–РЅРЅС– С–СЃС‚РѕСЂС–С—
HistoryData history[HISTORY_BUFFER_SIZE];
int historyIndex = 0;
bool historyInitialized = false;

void addToHistory() {
  unsigned long now = millis();
  
  if (now - getLastHistorySave() < 60000) { // Р—Р±РµСЂС–РіР°С”РјРѕ РєРѕР¶РЅСѓ С…РІРёР»РёРЅСѓ
    return;
  }
  
  setLastHistorySave(now);
  
  if (xSemaphoreTake(getHistoryMutex(), portMAX_DELAY)) {
    history[historyIndex].timestamp = now;
    history[historyIndex].tempCarrier = sensorData.tempCarrier;
    history[historyIndex].tempRoom = sensorData.tempRoom;
    history[historyIndex].tempBME = sensorData.tempBME;
    history[historyIndex].humidity = sensorData.humidity;
    history[historyIndex].pressure = sensorData.pressure;
    history[historyIndex].pumpPower = (heatingState.pumpPower * 100) / 255;
    history[historyIndex].fanPower = (heatingState.fanPower * 100) / 255;
    history[historyIndex].extractorPower = (heatingState.extractorPower * 100) / 255;
    history[historyIndex].mode = getCurrentMode();
    
    historyIndex = (historyIndex + 1) % HISTORY_BUFFER_SIZE;
    if (!historyInitialized && historyIndex == 0) {
      historyInitialized = true;
    }
    
    // Р’РёРІС–Рґ РґР»СЏ РІС–РґР»Р°РіРѕРґР¶РµРЅРЅСЏ (РєРѕР¶РЅС– 10 Р·Р°РїРёСЃС–РІ)
    if (historyIndex % 10 == 0) {
      Serial.printf("[HISTORY] Р—Р°РїРёСЃР°РЅРѕ Р·Р°РїРёСЃ %d\n", historyIndex);
    }
    
    xSemaphoreGive(getHistoryMutex());
  }
}

void timeTask(void *parameter) {
  Serial.println("вњ“ Р—Р°РґР°С‡Сѓ С‡Р°СЃСѓ Р·Р°РїСѓС‰РµРЅРѕ");
  
  // Р§РµРєР°С”РјРѕ РЅР° С–РЅС–С†С–Р°Р»С–Р·Р°С†С–СЋ С–РЅС€РёС… РєРѕРјРїРѕРЅРµРЅС‚С–РІ
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  unsigned long lastDebugPrint = 0;
  
  while (1) {
    // РЎРёРЅС…СЂРѕРЅС–Р·Р°С†С–СЏ С‡Р°СЃСѓ
    syncTime();
    
    // РћРЅРѕРІР»РµРЅРЅСЏ Р»РѕРєР°Р»СЊРЅРѕРіРѕ С‡Р°СЃСѓ РєРѕР¶РЅСѓ СЃРµРєСѓРЅРґСѓ
    if (xSemaphoreTake(getTimeMutex(), portMAX_DELAY)) {
      time(&currentTime);
      localtime_r(&currentTime, &timeInfo);
      xSemaphoreGive(getTimeMutex());
    }
    
    // Р’РёРІС–Рґ СЃС‚Р°С‚СѓСЃСѓ СЃРёРЅС…СЂРѕРЅС–Р·Р°С†С–С— (РєРѕР¶РЅС– 30 СЃРµРєСѓРЅРґ)
    unsigned long now = millis();
    if (now - lastDebugPrint > 30000) {
      lastDebugPrint = now;
      if (isTimeSynced()) {
        Serial.printf("[TIME] РЎРёРЅС…СЂРѕРЅС–Р·РѕРІР°РЅРѕ: %s\n", getTimeString().c_str());
      } else {
        Serial.println("[TIME] вљ  РќРµРјР°С” СЃРёРЅС…СЂРѕРЅС–Р·Р°С†С–С—");
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 СЃРµРєСѓРЅРґР°
  }
}

// Р”РѕРґР°С‚РєРѕРІС– С„СѓРЅРєС†С–С— РґР»СЏ СЂРѕР±РѕС‚Рё Р· С–СЃС‚РѕСЂС–С”СЋ
HistoryData* getHistoryBuffer() {
    return history;
}

int getHistoryIndex() {
    return historyIndex;
}

bool isHistoryInitialized() {
    return historyInitialized;
}

void setHistoryInitialized(bool initialized) {
    historyInitialized = initialized;
}
