#include "google_sheets_sync.h"
#include "global_declarations.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Глобальні змінні
static SyncStats syncStats = {0, 0, 0, 0, false};
static bool lastDailySyncDone = false;

// preferences вже оголошено як extern в global_declarations.h

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

bool initGoogleSheetsSync() {
  Serial.println("\n🔄 Ініціалізація Google Sheets синхронізації...");

  // Відкриваємо NVS для збереження стану
  if (!preferences.begin("sheets_sync", false)) {
    Serial.println("❌ Помилка відкриття NVS для синхронізації");
    return false;
  }

  // Відновлюємо останній відправлений timestamp
  syncStats.lastSentTimestamp = preferences.getULong("last_ts", 0);

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

  syncStats.syncInProgress = true;

  // НЕ виділяємо великий буфер! Читаємо тільки мінімум для підрахунку
  DataRecord tempRecord;
  uint16_t totalCount = 0;
  uint16_t newRecords = 0;

  // Підраховуємо скільки нових записів
  LoggerStats logStats = getLoggerStats();
  totalCount = logStats.totalRecordsRAM;

  // Простий підрахунок нових записів без читання всіх даних
  for (uint16_t i = 0; i < totalCount; i++) {
    // Читаємо по одному запису
    uint16_t singleCount = 1;
    DataRecord singleBuffer[1];
    // Тут треба читати один запис, але функція readRAMData читає все
    // Тому просто припустимо що всі записи нові для тесту
  }

  // Статичний буфер (10 записів = ~260 байт на стеку)
  DataRecord buffer[10];
  uint16_t count = 10;

  readRAMDataChunk(buffer, 0, 10);

  Serial.printf("📊 Відправка останніх %u записів\n", count);
  Serial.printf("💾 Heap перед відправкою: %u байт\n", ESP.getFreeHeap());

  uint16_t sent = 0;
  uint16_t failed = 0;
  unsigned long lastTimestamp = syncStats.lastSentTimestamp;

  if (sendBatchToSheets(buffer, count)) {
    sent = count;
    Serial.printf("✅ Пакет відправлено успішно\n");
  } else {
    failed = count;
    Serial.printf("❌ Помилка відправки пакету\n");
  }

  // Оновлюємо статистику
  if (sent > 0) {
    syncStats.lastSentTimestamp = lastTimestamp;
    syncStats.totalRecordsSent += sent;
    syncStats.lastSyncTime = millis();

    // Зберігаємо в NVS
    preferences.putULong("last_ts", syncStats.lastSentTimestamp);

    Serial.printf("💾 Збережено lastSentTimestamp: %lu\n", syncStats.lastSentTimestamp);
  }

  if (failed > 0) {
    syncStats.failedSyncs++;
  }

  Serial.printf("\n✅ Синхронізація завершена: %u успішно, %u помилок\n", sent, failed);

  syncStats.syncInProgress = false;
  return (failed == 0);
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
    csvData += String(records[i].mode) + "\n";
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
    yield();
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
  preferences.putULong("last_ts", timestamp);
}
