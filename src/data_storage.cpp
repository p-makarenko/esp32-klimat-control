#include "data_storage.h"
#include "system_core.h"
#include <Arduino.h>
#include "advanced_climate_logic.h"
#include "utility_functions.h"

// Глобальні змінні
HistoryData history[HISTORY_BUFFER_SIZE];
int historyIndex = 0;
bool historyInitialized = false;

void addToHistory() {
  unsigned long now = millis();
  
  if (now - getLastHistorySave() < 60000) { // Зберігаємо кожну хвилину
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
    
    // Вивід для відлагодження (кожні 10 записів)
    if (historyIndex % 10 == 0) {
      Serial.printf("[HISTORY] Записано запис %d\n", historyIndex);
    }
    
    xSemaphoreGive(getHistoryMutex());
  }
}

void timeTask(void *parameter) {
  // Задача часу запущено
  
  // Чекаємо на ініціалізацію інших компонентів
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  unsigned long lastDebugPrint = 0;
  
  while (1) {
    // Синхронізація часу
    syncTime();
    
    // Оновлення локального часу кожну секунду
    if (xSemaphoreTake(getTimeMutex(), portMAX_DELAY)) {
      time(&currentTime);
      localtime_r(&currentTime, &timeInfo);
      xSemaphoreGive(getTimeMutex());
    }
    
    // Вивід статусу синхронізації (кожні 30 секунд)
    unsigned long now = millis();
    if (now - lastDebugPrint > 30000) {
      lastDebugPrint = now;
      if (isTimeSynced()) {
        Serial.printf("[TIME] Синхронізовано: %s\n", getTimeString().c_str());
      } else {
        // Немає синхронізації
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 секунда
  }
}

// Додаткові функції для роботи з історією
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