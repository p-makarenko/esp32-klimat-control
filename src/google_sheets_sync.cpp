#include "google_sheets_sync.h"
#include "global_declarations.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#if ENABLE_ENERGY_MONITOR
#include "energy_monitor.h"
#endif

// Глобальні змінні
static SyncStats syncStats = {0, 0, 0, 0, false};
static bool lastDailySyncDone = false;

// preferences вже оголошено як extern в global_declarations.h

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

bool initGoogleSheetsSync() {
  Serial.println("\n🔄 Ініціалізація Google Sheets синхронізації...");

  // Відкриваємо NVS, читаємо timestamp, закриваємо
  Preferences sheetsPrefs;
  if (!sheetsPrefs.begin("sheets_sync", true)) {  // true = read-only
    Serial.println("⚠️ NVS sheets_sync не існує, буде створено при першій синхронізації");
    syncStats.lastSentTimestamp = 0;
  } else {
    syncStats.lastSentTimestamp = sheetsPrefs.getULong("last_ts", 0);
    sheetsPrefs.end();  // Закриваємо одразу!
  }

  if (syncStats.lastSentTimestamp > 0) {
    Serial.printf("📅 Останній відправлений timestamp: %lu\n", syncStats.lastSentTimestamp);
  } else {
    Serial.println("📅 Перша синхронізація - відправимо всі дані");
  }

  Serial.println("✓ Google Sheets синхронізація готова");
  return true;
}

// ============================================================================
// СИНХРОНІЗАЦІЯ
// ============================================================================

bool syncToGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  Wi-Fi не підключено, синхронізація пропущена");
    syncStats.failedSyncs++;
    return false;
  }

  if (syncStats.syncInProgress) {
    Serial.println("⚠️  Синхронізація вже виконується");
    return false;
  }

  Serial.println("\n📤 Початок синхронізації з Google Sheets...");
  Serial.printf("💾 Heap на початку: %u байт\n", ESP.getFreeHeap());
  Serial.printf("🕐 Останній відправлений timestamp: %lu\n", syncStats.lastSentTimestamp);

  syncStats.syncInProgress = true;

  // Виділяємо пам'ять для ВСЬОГО буфера (1440 записів)
  // Не можемо використовувати logStats.totalRecordsRAM, бо це НЕ розмір буфера!
  uint16_t bufferSize = HISTORY_BUFFER_SIZE;
  DataRecord* allRecords = (DataRecord*)malloc(bufferSize * sizeof(DataRecord));
  if (!allRecords) {
    Serial.println("❌ Недостатньо пам'яті для синхронізації");
    syncStats.syncInProgress = false;
    return false;
  }

  uint16_t actualCount = 0;
  readRAMData(allRecords, &actualCount);

  Serial.printf("📊 Прочитано записів: %u (буфер: %u)\n", actualCount, bufferSize);

  // Підраховуємо і збираємо нові записи
  uint16_t newCount = 0;
  for (uint16_t i = 0; i < actualCount; i++) {
    if (allRecords[i].timestamp > syncStats.lastSentTimestamp && allRecords[i].timestamp > 0) {
      newCount++;
    }
  }

  if (newCount == 0) {
    Serial.println("📭 Немає нових записів");
    free(allRecords);
    syncStats.syncInProgress = false;
    return true;
  }

  // Виділяємо пам'ять тільки для нових записів
  DataRecord* newRecords = (DataRecord*)malloc(newCount * sizeof(DataRecord));
  if (!newRecords) {
    Serial.println("❌ Недостатньо пам'яті для нових записів");
    free(allRecords);
    syncStats.syncInProgress = false;
    return false;
  }

  // Копіюємо нові записи
  uint16_t idx = 0;
  for (uint16_t i = 0; i < actualCount; i++) {
    if (allRecords[i].timestamp > syncStats.lastSentTimestamp && allRecords[i].timestamp > 0) {
      newRecords[idx++] = allRecords[i];
    }
  }

  free(allRecords); // Звільняємо великий масив

  // Сортуємо за timestamp для уникнення дублювання
  for (uint16_t i = 0; i < newCount - 1; i++) {
    for (uint16_t j = i + 1; j < newCount; j++) {
      if (newRecords[i].timestamp > newRecords[j].timestamp) {
        DataRecord temp = newRecords[i];
        newRecords[i] = newRecords[j];
        newRecords[j] = temp;
      }
    }
  }

  // Видаляємо дублікати (записи з однаковим timestamp)
  uint16_t uniqueCount = 0;
  for (uint16_t i = 0; i < newCount; i++) {
    bool isDuplicate = false;
    for (uint16_t j = 0; j < uniqueCount; j++) {
      if (newRecords[i].timestamp == newRecords[j].timestamp) {
        isDuplicate = true;
        break;
      }
    }
    if (!isDuplicate) {
      if (i != uniqueCount) {
        newRecords[uniqueCount] = newRecords[i];
      }
      uniqueCount++;
    }
  }

  if (uniqueCount < newCount) {
    Serial.printf("⚠️  Видалено %u дублікатів\n", newCount - uniqueCount);
    newCount = uniqueCount;
  }

  Serial.printf("📊 Знайдено %u унікальних записів для відправки\n", newCount);

  // Перевірка heap перед відправкою
  const uint32_t MIN_HEAP_FOR_SYNC = 60000; // 60KB мінімум
  if (ESP.getFreeHeap() < MIN_HEAP_FOR_SYNC) {
    Serial.printf("⚠️ Недостатньо пам'яті для синхронізації (є %u, треба >%u)\n",
                  ESP.getFreeHeap(), MIN_HEAP_FOR_SYNC);
    free(newRecords);
    syncStats.syncInProgress = false;
    return false;
  }

  // Розбиваємо на пакети по 100 записів
  const uint16_t BATCH_SIZE = 100;
  uint16_t totalSent = 0;

  for (uint16_t offset = 0; offset < newCount; offset += BATCH_SIZE) {
    uint16_t batchSize = min((uint16_t)BATCH_SIZE, (uint16_t)(newCount - offset));
    Serial.printf("📦 Пакет %u-%u з %u\n", offset + 1, offset + batchSize, newCount);

    if (sendBatchToSheets(&newRecords[offset], batchSize)) {
      totalSent += batchSize;

      // Оновлюємо timestamp після кожного успішного пакету
      unsigned long maxTimestamp = 0;
      for (uint16_t i = offset; i < offset + batchSize; i++) {
        if (newRecords[i].timestamp > maxTimestamp) {
          maxTimestamp = newRecords[i].timestamp;
        }
      }

      if (maxTimestamp > syncStats.lastSentTimestamp) {
        syncStats.lastSentTimestamp = maxTimestamp;
        // Зберігаємо в NVS з правильним open/close
        Preferences sheetsPrefs;
        if (sheetsPrefs.begin("sheets_sync", false)) {
          sheetsPrefs.putULong("last_ts", syncStats.lastSentTimestamp);
          sheetsPrefs.end();
        }
        Serial.printf("✓ Оновлено timestamp: %lu\n", maxTimestamp);
      }
    } else {
      Serial.printf("⚠️ Пакет не відправлено, зупиняємо\n");
      break;
    }
    delay(500);
  }

  if (totalSent > 0) {
    syncStats.totalRecordsSent += totalSent;
    syncStats.lastSyncTime = millis();
    Serial.printf("✅ Відправлено %u з %u записів\n", totalSent, newCount);
    free(newRecords);
    syncStats.syncInProgress = false;
    return true;
  } else {
    syncStats.failedSyncs++;
    Serial.printf("❌ Синхронізація помилка\n");
    free(newRecords);
    syncStats.syncInProgress = false;
    return false;
  }
}

bool sendBatchToSheets(DataRecord* records, uint16_t count) {
  if (count == 0) return true;

  // Перевірка WiFi з'єднання
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi не підключений");
    return false;
  }

  Serial.printf("📡 WiFi підключений, IP: %s\n", WiFi.localIP().toString().c_str());

  // Перевірка DNS
  IPAddress serverIP;
  if (!WiFi.hostByName("script.google.com", serverIP)) {
    Serial.println("❌ Помилка DNS: не вдалось розв'язати script.google.com");
    return false;
  }
  Serial.printf("✅ DNS OK: script.google.com = %s\n", serverIP.toString().c_str());

  Serial.printf("📤 Відправка %u записів через GET запити...\n", count);
  Serial.printf("💾 Heap: %u байт, найбільший блок: %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Примусове звільнення пам'яті перед SSL
  heap_caps_malloc_extmem_enable(1024); // Дозволяємо external RAM якщо є

  Serial.printf("💾 Після оптимізації: %u байт\n", ESP.getFreeHeap());

  WiFiClientSecure* client = new WiFiClientSecure();
  if (!client) {
    Serial.println("❌ Помилка створення SSL клієнта");
    return false;
  }

  client->setInsecure();
  client->setTimeout(10000);

  String csvData = "";
  for (uint16_t i = 0; i < count; i++) {
    time_t ts = records[i].timestamp;
    struct tm* timeinfo = localtime(&ts);
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", timeinfo);

    csvData += String(dateStr) + ",";
    csvData += String(records[i].tempCarrier, 1) + ",";
    csvData += String(records[i].tempRoom, 1) + ",";
    csvData += String(records[i].tempBME, 1) + ",";
    csvData += String(records[i].humidity, 1) + ",";
    csvData += String(records[i].pumpPower) + ",";
    csvData += String(records[i].fanPower) + ",";
    csvData += String(records[i].extractorPower) + ",";
    csvData += String(records[i].mode) + ",";

    // Додаємо енергоспоживання (якщо доступно)
    #if ENABLE_ENERGY_MONITOR
    EnergyMeasurements energy = getEnergyMeasurements();
    csvData += String(energy.power, 1);
    #else
    csvData += "0";
    #endif
    csvData += "\n";
  }

  Serial.printf("📤 Відправка %u записів одним пакетом...\n", count);

  if (!client->connect("script.google.com", 443)) {
    Serial.printf("❌ Підключення не вдалось (heap: %u)\n", ESP.getFreeHeap());
    delete client;
    return false;
  }

  String path = String(GOOGLE_SCRIPT_URL).substring(String(GOOGLE_SCRIPT_URL).indexOf("/macros"));

  client->println("POST " + path + " HTTP/1.1");
  client->println("Host: script.google.com");
  client->println("Content-Type: text/csv");
  client->println("Content-Length: " + String(csvData.length()));
  client->println("Connection: close");
  client->println();
  client->print(csvData);

  unsigned long timeout = millis();
  bool success = false;

  while (client->connected() && millis() - timeout < 10000) {
    if (client->available()) {
      String line = client->readStringUntil('\n');
      if (line.indexOf("HTTP/1.1 200") >= 0 || line.indexOf("HTTP/1.1 302") >= 0) {
        success = true;
      }
      if (line.indexOf("OK") >= 0) {
        success = true;
        break;
      }
    }
    delay(10);
  }

  client->stop();
  delete client;

  if (success) {
    Serial.println("✅ Всі записи відправлено успішно");
    return true;
  } else {
    Serial.println("❌ Помилка відправки");
    return false;
  }
}

// ============================================================================
// АВТОМАТИЧНА СИНХРОНІЗАЦІЯ
// ============================================================================

void autoSyncTask() {
  static unsigned long lastCheckTime = 0;
  unsigned long now = millis();

  // Перевіряємо не частіше ніж раз на хвилину
  if (now - lastCheckTime < 60000) return;
  lastCheckTime = now;

  // Пропускаємо якщо вже синхронізуємось
  if (syncStats.syncInProgress) return;

  // Перевіряємо чи час для щоденної синхронізації
  if (checkDailySyncTime() && !lastDailySyncDone) {
    Serial.println("🕐 Час щоденної синхронізації (23:59)");
    if (syncToGoogleSheets()) {
      lastDailySyncDone = true;
    }
    return;
  }

  // Скидаємо прапорець щоденної синхронізації о 00:00
  time_t nowTime;
  time(&nowTime);
  struct tm* timeInfo = localtime(&nowTime);
  if (timeInfo->tm_hour == 0 && timeInfo->tm_min == 0) {
    lastDailySyncDone = false;
  }

  // Перевіряємо інтервал 30 хвилин
  if (syncStats.lastSyncTime == 0 || (now - syncStats.lastSyncTime) >= SYNC_INTERVAL_MS) {
    // Перевіряємо чи є достатньо нових записів
    LoggerStats logStats = getLoggerStats();

    // Виділяємо пам'ять в heap замість стеку
    DataRecord* buffer = (DataRecord*)malloc(1440 * sizeof(DataRecord));
    if (!buffer) {
      Serial.println("❌ Помилка виділення пам'яті для autoSync");
      return;
    }

    uint16_t count = 0;
    readRAMData(buffer, &count);

    uint16_t newRecords = 0;
    for (uint16_t i = 0; i < count; i++) {
      if (buffer[i].timestamp > syncStats.lastSentTimestamp && buffer[i].timestamp > 0) {
        newRecords++;
      }
    }

    free(buffer); // Звільняємо пам'ять

    if (newRecords >= SYNC_MIN_NEW_RECORDS) {
      Serial.printf("🤖 Автоматична синхронізація: %u нових записів\n", newRecords);
      syncToGoogleSheets();
    }
  }
}

bool checkDailySyncTime() {
  time_t now;
  time(&now);
  struct tm* timeInfo = localtime(&now);

  return (timeInfo->tm_hour == SYNC_DAILY_HOUR &&
          timeInfo->tm_min == SYNC_DAILY_MINUTE);
}

// ============================================================================
// СТАТИСТИКА
// ============================================================================

SyncStats getSyncStats() {
  return syncStats;
}

void printSyncInfo() {
  Serial.println("\n📊 СТАТИСТИКА GOOGLE SHEETS СИНХРОНІЗАЦІЇ");
  Serial.println("==========================================");
  Serial.printf("Останній відправлений timestamp: %lu\n", syncStats.lastSentTimestamp);
  Serial.printf("Всього відправлено за сесію: %u записів\n", syncStats.totalRecordsSent);
  Serial.printf("Невдалих синхронізацій: %u\n", syncStats.failedSyncs);
  Serial.printf("Статус: %s\n", syncStats.syncInProgress ? "В процесі..." : "Готово");

  if (syncStats.lastSyncTime > 0) {
    unsigned long timeSinceSync = (millis() - syncStats.lastSyncTime) / 1000;
    Serial.printf("Час з останньої синхронізації: %lu секунд\n", timeSinceSync);
  }
  Serial.println("==========================================");
}

// ============================================================================
// УТИЛІТИ
// ============================================================================

unsigned long getLastSentTimestamp() {
  return syncStats.lastSentTimestamp;
}

void saveLastSentTimestamp(unsigned long timestamp) {
  syncStats.lastSentTimestamp = timestamp;
  Preferences sheetsPrefs;
  if (sheetsPrefs.begin("sheets_sync", false)) {
    sheetsPrefs.putULong("last_ts", timestamp);
    sheetsPrefs.end();
  }
}
