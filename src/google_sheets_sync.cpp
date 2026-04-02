#include "google_sheets_sync.h"
#include "global_declarations.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#if ENABLE_ENERGY_MONITOR
#include "energy_monitor.h"
#endif

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ
// ============================================================================

// Ініціалізуємо структуру нулями.
// sequence зберігається ТІЛЬКИ тут.
static SyncStats syncStats = {0, 0, 0, 0, 0, false};
static bool lastDailySyncDone = false;

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

bool initGoogleSheetsSync() {
  Serial.println("\n🔄 [Sync] Ініціалізація...");

  Preferences sheetsPrefs;
  if (!sheetsPrefs.begin("sheets_sync", true)) {
    Serial.println("⚠️ [Sync] NVS не знайдено, старт з нуля.");
    syncStats.lastSentTimestamp = 0;
    syncStats.lastSentSequence = 0;
  } else {
    syncStats.lastSentTimestamp = sheetsPrefs.getULong("last_ts", 0);
    syncStats.lastSentSequence = sheetsPrefs.getULong("last_seq", 0);
    sheetsPrefs.end();
  }

  Serial.printf("📊 [Sync] Стан: Seq=%lu, TS=%lu\n", 
                syncStats.lastSentSequence, syncStats.lastSentTimestamp);
  
  return true;
}

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================================

// Компаратор для qsort (сортування DataRecord)
int compareRecords(const void* a, const void* b) {
    DataRecord* recA = (DataRecord*)a;
    DataRecord* recB = (DataRecord*)b;
    
    // Сортуємо по Sequence
    if (recA->sequenceNumber < recB->sequenceNumber) return -1;
    if (recA->sequenceNumber > recB->sequenceNumber) return 1;
    return 0;
}

// ============================================================================
// СИНХРОНІЗАЦІЯ
// ============================================================================

bool syncToGoogleSheets() {
  // 1. Базові перевірки
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ [Sync] Немає Wi-Fi");
    syncStats.failedSyncs++;
    return false;
  }

  if (syncStats.syncInProgress) {
    Serial.println("⚠️ [Sync] Вже виконується");
    return false;
  }

  // 2. Перевірка вільної пам'яті ДО початку
  // Нам потрібно ~23KB для буфера + ~55KB для SSL handshake. Разом ~80KB.
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t requiredHeap = (HISTORY_BUFFER_SIZE * sizeof(DataRecord)) + 55000;

  if (freeHeap < requiredHeap) {
    Serial.printf("❌ [Sync] Критично мало RAM! Free: %u, Req: %u\n", freeHeap, requiredHeap);
    syncStats.failedSyncs++;
    return false;
  }

  Serial.printf("💾 [Sync] Heap: %u вільно, %u потрібно\n", freeHeap, requiredHeap);

  syncStats.syncInProgress = true;
  Serial.println("\n📤 [Sync] Початок сесії...");

  // 3. Виділення пам'яті
  DataRecord* buffer = (DataRecord*)malloc(HISTORY_BUFFER_SIZE * sizeof(DataRecord));
  if (!buffer) {
    Serial.println("❌ [Sync] malloc failed!");
    syncStats.syncInProgress = false;
    return false;
  }

  // 4. Читання та Фільтрація
  uint16_t totalCount = 0;
  readRAMData(buffer, &totalCount); // Припускаємо, що це копіює дані в buffer

  uint16_t recordsToSend = 0;
  
  // "In-place" фільтрація: зсуваємо потрібні записи на початок масиву
  for (uint16_t i = 0; i < totalCount; i++) {
    bool isNew = false;
    // Головний критерій - Sequence
    if (buffer[i].sequenceNumber > syncStats.lastSentSequence) {
        isNew = true;
    } 
    // Fallback критерій - Timestamp (якщо sequence скидався)
    else if (syncStats.lastSentSequence == 0 && buffer[i].timestamp > syncStats.lastSentTimestamp) {
        isNew = true;
    }

    if (isNew && buffer[i].timestamp > 1000000) { // Проста перевірка на валідний час
       if (i != recordsToSend) {
           buffer[recordsToSend] = buffer[i]; // Копіюємо на початок
       }
       recordsToSend++;
    }
  }

  if (recordsToSend == 0) {
    Serial.println("📭 [Sync] Немає нових даних.");
    free(buffer);
    syncStats.syncInProgress = false;
    return true;
  }

  // 5. Сортування відфільтрованих даних
  qsort(buffer, recordsToSend, sizeof(DataRecord), compareRecords);

  Serial.printf("📊 [Sync] Готуємо до відправки %u записів (Seq %lu -> %lu)\n",
                recordsToSend, buffer[0].sequenceNumber, buffer[recordsToSend-1].sequenceNumber);

  // 5.5 Якщо записів багато і мало пам'яті - обмежуємо кількість
  uint32_t heapBeforeSSL = ESP.getFreeHeap();
  const uint16_t MAX_RECORDS_LOW_MEM = 100;  // Максимум при низькій пам'яті

  if (heapBeforeSSL < 60000 && recordsToSend > MAX_RECORDS_LOW_MEM) {
    Serial.printf("⚠️ [Sync] Низька пам'ять (%u), обмежуємо до %u записів\n",
                  heapBeforeSSL, MAX_RECORDS_LOW_MEM);
    recordsToSend = MAX_RECORDS_LOW_MEM;
  }

  // 6. Підготовка SSL
  Serial.printf("💾 [Sync] Heap перед SSL: %u bytes\n", heapBeforeSSL);

  WiFiClientSecure sslClient;
  sslClient.setInsecure(); // Для тестів ок, для проду краще certs
  sslClient.setTimeout(20000); // Збільшено до 20 сек

  Serial.println("🔐 [Sync] Підключення до script.google.com:443...");
  if (!sslClient.connect("script.google.com", 443)) {
    char sslErrorBuf[128];
    int sslError = sslClient.lastError(sslErrorBuf, sizeof(sslErrorBuf));
    Serial.println("❌ [Sync] Google connection failed");
    Serial.printf("🔒 [Sync] SSL error: %d - %s\n", sslError, sslErrorBuf);
    Serial.printf("📶 [Sync] WiFi RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("💾 [Sync] Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("📊 [Sync] Min free heap: %u bytes\n", ESP.getMinFreeHeap());

    // Діагностика типових помилок
    if (ESP.getFreeHeap() < 45000) {
      Serial.println("💡 [Sync] Причина: недостатньо RAM для SSL handshake");
    }

    free(buffer);
    syncStats.syncInProgress = false;
    syncStats.failedSyncs++;
    return false;
  }
  Serial.println("✅ [Sync] SSL з'єднання встановлено");

  // 7. Відправка пакетами
  uint16_t sessionSentCount = 0;

  for (uint16_t offset = 0; offset < recordsToSend; offset += SYNC_BATCH_SIZE) {
    uint16_t currentBatchSize = (recordsToSend - offset) > SYNC_BATCH_SIZE ?
                                 SYNC_BATCH_SIZE : (recordsToSend - offset);

    // Передаємо адресу початку поточного пакету в буфері
    if (sendBatchToSheets(&sslClient, &buffer[offset], currentBatchSize)) {
        sessionSentCount += currentBatchSize;

        // Визначаємо останній успішний sequence в цьому пакеті
        DataRecord* lastRec = &buffer[offset + currentBatchSize - 1];

        // Оновлюємо статистику в пам'яті
        syncStats.lastSentSequence = lastRec->sequenceNumber;
        syncStats.lastSentTimestamp = lastRec->timestamp;

        // ! ВАЖЛИВО: Оновлюємо NVS тільки якщо пакет успішний.
        // Це дозволяє відновитись з правильного місця при ребуті.
        Preferences prefs;
        if (prefs.begin("sheets_sync", false)) {
            prefs.putULong("last_seq", syncStats.lastSentSequence);
            prefs.putULong("last_ts", syncStats.lastSentTimestamp);
            prefs.end();
        }

        Serial.printf("✅ [Sync] Пакет %u/%u OK. Last Seq: %lu\n",
                      offset/SYNC_BATCH_SIZE + 1, (recordsToSend + SYNC_BATCH_SIZE - 1)/SYNC_BATCH_SIZE,
                      syncStats.lastSentSequence);
    } else {
        Serial.println("❌ [Sync] Помилка пакету. Переривання.");
        break; // Зупиняємо цикл, щоб не слати дірки
    }

    // Yield для запобігання Watchdog timeout при великих об'ємах
    delay(50);
    yield();  // Додаткова поступка для web server
  }

  // 8. Очищення
  sslClient.stop();
  free(buffer);

  syncStats.lastSyncTime = millis();
  syncStats.totalRecordsSent += sessionSentCount;
  syncStats.syncInProgress = false;

  return (sessionSentCount > 0);
}

bool sendBatchToSheets(WiFiClientSecure* client, DataRecord* records, uint16_t count) {
  if (count == 0) return true;

  // Використовуємо HTTPClient для автоматичної обробки redirect

  // Формуємо CSV String
  String csvData = "";
  csvData.reserve(count * 80);

  for (uint16_t i = 0; i < count; i++) {
    time_t ts = records[i].timestamp;
    struct tm* tm = localtime(&ts);

    char line[130];
#if ENABLE_ENERGY_MONITOR
    EnergyMeasurements em = getEnergyMeasurements();
    snprintf(line, sizeof(line),
             "%04d-%02d-%02d %02d:%02d:%02d,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%.1f,%.1f\n",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec,
             records[i].tempCarrier, records[i].tempRoom, records[i].tempBME,
             records[i].humidity, records[i].pumpPower, records[i].fanPower,
             records[i].extractorPower, records[i].mode,
             em.error ? 0.0f : em.voltage,
             em.error ? 0.0f : em.power);
#else
    snprintf(line, sizeof(line),
             "%04d-%02d-%02d %02d:%02d:%02d,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d,0,0\n",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec,
             records[i].tempCarrier, records[i].tempRoom, records[i].tempBME,
             records[i].humidity, records[i].pumpPower, records[i].fanPower,
             records[i].extractorPower, records[i].mode);
#endif
    csvData += line;
  }

  Serial.printf("📊 [Sync] Sending %u bytes\n", csvData.length());
  Serial.printf("🔍 Sample: %.80s\n", csvData.c_str());

  // Пряме SSL з правильними headers (без HTTPClient)
  if (!client->connected()) {
      Serial.println("🔄 Reconnect...");
      client->stop();
      delay(100);
      yield();  // Дозволяємо іншим задачам працювати
      if (!client->connect("script.google.com", 443)) {
          Serial.println("❌ Reconnect fail");
          Serial.printf("💾 Free heap: %u bytes\n", ESP.getFreeHeap());
          return false;
      }
  }

  String path = String(GOOGLE_SCRIPT_URL);
  path = path.substring(path.indexOf("/macros"));

  // POST з application/x-www-form-urlencoded (замість text/plain)
  // Це запобігає redirect на cached endpoint
  String postData = "data=" + csvData;
  postData.replace("\n", "%0A");
  postData.replace(",", "%2C");
  postData.replace(":", "%3A");
  postData.replace(" ", "+");

  client->println("POST " + path + " HTTP/1.1");
  client->println("Host: script.google.com");
  client->println("User-Agent: ESP32");
  client->println("Content-Type: application/x-www-form-urlencoded");
  client->println("Content-Length: " + String(postData.length()));
  client->println("Connection: close");
  client->println();
  client->print(postData);
  client->flush();

  Serial.println("📤 Request sent, waiting response...");

  unsigned long timeout = millis();
  while (!client->available() && millis() - timeout < 15000) {
      delay(10);
      yield();  // Дозволяємо іншим задачам працювати
  }

  if (!client->available()) {
      Serial.println("❌ Timeout waiting for response");
      client->stop();
      return false;
  }

  String response = "";
  while (client->available()) {
      response += (char)client->read();
  }

  // Обробка redirect 302
  if (response.indexOf("302") >= 0 && response.indexOf("Location:") >= 0) {
      Serial.println("🔀 Redirect detected, following...");

      int locIdx = response.indexOf("Location: ") + 10;
      int endIdx = response.indexOf("\r", locIdx);
      if (endIdx < 0) endIdx = response.indexOf("\n", locIdx);

      String redirectUrl = response.substring(locIdx, endIdx);
      redirectUrl.trim();

      Serial.println(redirectUrl.substring(0, 120));

      // Парсинг URL
      int hostStart = redirectUrl.indexOf("://") + 3;
      int pathStart = redirectUrl.indexOf("/", hostStart);
      String newHost = redirectUrl.substring(hostStart, pathStart);
      String newPath = redirectUrl.substring(pathStart);

      // Новий запит
      client->stop();
      delay(200);

      if (!client->connect(newHost.c_str(), 443)) {
          Serial.println("❌ Redirect failed - connection to " + newHost + " failed");
          Serial.printf("💾 Free heap: %u bytes\n", ESP.getFreeHeap());
          client->stop();  // ВАЖЛИВО: закриваємо клієнт при помилці
          return false;
      }

      // Redirect endpoint приймає тільки GET - redirect URL вже містить дані
      // Просто робимо GET на цей URL
      Serial.println("⚠️ Switching to GET for redirect endpoint");

      client->println("GET " + newPath + " HTTP/1.1");
      client->println("Host: " + newHost);
      client->println("User-Agent: ESP32");
      client->println("Connection: close");
      client->println();
      client->flush();

      timeout = millis();
      while (!client->available() && millis() - timeout < 15000) {
          delay(10);
          yield();  // Дозволяємо іншим задачам працювати
      }

      if (!client->available()) {
          Serial.println("❌ Redirect timeout");
          client->stop();
          return false;
      }

      response = "";
      while (client->available()) {
          response += (char)client->read();
      }
      client->stop();  // Завжди закриваємо після читання

      Serial.printf("📥 Final response (%d bytes):\n", response.length());
  }

  Serial.println(response.substring(0, 400));

  bool success = (response.indexOf("OK:") >= 0 || response.indexOf("200 OK") >= 0);
  if (success) Serial.println("✅ Success");

  return success;
}

// ============================================================================
// ІНШІ ФУНКЦІЇ (AutoSync, Utils...) - 
// (Тут зміни мінімальні, головне - прибрати static lastSentSequence)
// ============================================================================

void autoSyncTask() {
    static unsigned long lastCheck = 0;
    unsigned long now = millis();

    // Перевіряємо не частіше ніж раз на хвилину
    if (now - lastCheck < 60000) return;
    lastCheck = now;

    if (syncStats.syncInProgress) return;

    // Перевіряємо чи час для щоденної синхронізації
    if (checkDailySyncTime() && !lastDailySyncDone) {
        Serial.println("🕐 [Sync] Щоденна синхронізація (23:59)");
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

    // Перевірка інтервалу 30 хвилин
    if (syncStats.lastSyncTime == 0 || (now - syncStats.lastSyncTime) > SYNC_INTERVAL_MS) {
        Serial.println("⏰ [Sync] Auto-trigger");
        syncToGoogleSheets();
    }
}

// Гетери/Сетери тепер працюють з syncStats
SyncStats getSyncStats() { return syncStats; }
unsigned long getLastSentTimestamp() { return syncStats.lastSentTimestamp; }
uint32_t getLastSentSequence() { return syncStats.lastSentSequence; }

void saveLastSentTimestamp(unsigned long ts) {
    // Legacy support
    syncStats.lastSentTimestamp = ts;
    // Sequence не міняємо
    Preferences p;
    if(p.begin("sheets_sync", false)){
        p.putULong("last_ts", ts);
        p.end();
    }
}

void setLastSentSequence(uint32_t seq) {
    syncStats.lastSentSequence = seq;
    Preferences p;
    if(p.begin("sheets_sync", false)){
        p.putULong("last_seq", seq);
        p.end();
    }
}

bool checkDailySyncTime() {
    time_t now;
    time(&now);
    struct tm* timeInfo = localtime(&now);

    return (timeInfo->tm_hour == SYNC_DAILY_HOUR &&
            timeInfo->tm_min == SYNC_DAILY_MINUTE);
}

void printSyncInfo() {
    Serial.println("\n📊 СТАТИСТИКА GOOGLE SHEETS СИНХРОНІЗАЦІЇ");
    Serial.println("==========================================");
    Serial.printf("Останній sequence: %lu\n", syncStats.lastSentSequence);
    Serial.printf("Останній timestamp: %lu\n", syncStats.lastSentTimestamp);
    Serial.printf("Всього відправлено: %u записів\n", syncStats.totalRecordsSent);
    Serial.printf("Невдалих синхронізацій: %u\n", syncStats.failedSyncs);
    Serial.printf("Статус: %s\n", syncStats.syncInProgress ? "В процесі..." : "Готово");

    if (syncStats.lastSyncTime > 0) {
        unsigned long timeSinceSync = (millis() - syncStats.lastSyncTime) / 1000;
        Serial.printf("Час з останньої синхронізації: %lu секунд\n", timeSinceSync);
    }
    Serial.println("==========================================");
}