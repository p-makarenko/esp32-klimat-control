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

  // Відкриваємо NVS, читаємо sequence та timestamp, закриваємо
  Preferences sheetsPrefs;
  if (!sheetsPrefs.begin("sheets_sync", true)) {  // true = read-only
    Serial.println("⚠️ NVS sheets_sync не існує, буде створено при першій синхронізації");
    syncStats.lastSentSequence = 0;
    syncStats.lastSentTimestamp = 0;
  } else {
    // Спочатку перевіряємо нові значення (sequence)
    syncStats.lastSentSequence = sheetsPrefs.getULong("last_seq", 0);
    // Для совместимості читаємо і старий timestamp
    syncStats.lastSentTimestamp = sheetsPrefs.getULong("last_ts", 0);
    sheetsPrefs.end();  // Закриваємо одразу!
  }

  if (syncStats.lastSentSequence > 0) {
    Serial.printf("📊 Останній відправлений sequence: %lu\n", syncStats.lastSentSequence);
  } else if (syncStats.lastSentTimestamp > 0) {
    Serial.printf("📅 Legacy - останній timestamp: %lu (буде перейдено на sequence)\n", syncStats.lastSentTimestamp);
  } else {
    Serial.println("📅 Перша синхронізація - почнемо з sequence=0");
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

  // Google має rate limiting - мінімум 60 сек між запитами
  static unsigned long lastSyncAttempt = 0;
  unsigned long now = millis();
  if (lastSyncAttempt > 0 && (now - lastSyncAttempt) < 60000) {
    unsigned long waitTime = (60000 - (now - lastSyncAttempt)) / 1000;
    Serial.printf("⏸️ Google rate limit: зачекайте %lu сек\n", waitTime);
    return false;
  }
  lastSyncAttempt = now;

  if (xSemaphoreTake(getSyncMutex(), pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("⚠️  Синхронізація вже виконується");
    return false;
  }

  if (syncStats.syncInProgress) {
    xSemaphoreGive(getSyncMutex());
    Serial.println("⚠️  Синхронізація вже виконується");
    return false;
  }

  Serial.println("\n📤 Початок синхронізації з Google Sheets...");
  Serial.printf("💾 Heap: %u, макс блок: %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  Serial.printf("🕐 Останній sequence: %lu\n", syncStats.lastSentSequence);

  syncStats.syncInProgress = true;
  xSemaphoreGive(getSyncMutex());

  // Спочатку просто підраховуємо скільки нових записів
  uint16_t newCount = 0;
  uint32_t minSeq = 0xFFFFFFFF, maxSeq = 0;

  // Беремо мютекс для читання буфера
  if (xSemaphoreTake(getRamBufferMutex(), pdMS_TO_TICKS(500)) != pdTRUE) {
    Serial.println("❌ Не можу отримати доступ до RAM буфера");
    syncStats.syncInProgress = false;
    return false;
  }

  // Проходимо по буферу ОДИН раз без malloc
  for (uint16_t i = 0; i < 1440; i++) {
    DataRecord* rec = &ramBuffer[i];
    if (rec->sequenceNumber > 0) {
      if (rec->sequenceNumber < minSeq) minSeq = rec->sequenceNumber;
      if (rec->sequenceNumber > maxSeq) maxSeq = rec->sequenceNumber;

      if (rec->sequenceNumber > syncStats.lastSentSequence) {
        newCount++;
      }
    }
  }

  xSemaphoreGive(getRamBufferMutex());

  Serial.printf("📊 Діапазон sequence: %lu - %lu\n", minSeq == 0xFFFFFFFF ? 0 : minSeq, maxSeq);
  Serial.printf("📊 Знайдено нових: %u записів\n", newCount);

  if (newCount == 0) {
    Serial.println("📭 Немає нових записів");
    syncStats.syncInProgress = false;
    return true;
  }

  // Копіюємо нові записи у малий буфер (до 30)
  uint16_t toSend = newCount > 30 ? 30 : newCount;
  DataRecord* sendBuffer = (DataRecord*)malloc(toSend * sizeof(DataRecord));
  if (!sendBuffer) {
    syncStats.syncInProgress = false;
    Serial.println("❌ Не вдалось виділити буфер для 30 записів");
    return false;
  }

  // Копіюємо нові записи з RAM буфера прямо
  if (xSemaphoreTake(getRamBufferMutex(), pdMS_TO_TICKS(500)) != pdTRUE) {
    free(sendBuffer);
    syncStats.syncInProgress = false;
    Serial.println("❌ Не можу отримати доступ до RAM буфера при копіюванні");
    return false;
  }

  uint16_t copied = 0;
  for (uint16_t i = 0; i < 1440 && copied < toSend; i++) {
    if (ramBuffer[i].sequenceNumber > syncStats.lastSentSequence && ramBuffer[i].sequenceNumber > 0) {
      sendBuffer[copied++] = ramBuffer[i];
    }
  }

  xSemaphoreGive(getRamBufferMutex());

  // Сортуємо малий буфер
  for (uint16_t i = 0; i < copied - 1; i++) {
    for (uint16_t j = i + 1; j < copied; j++) {
      if (sendBuffer[i].sequenceNumber > sendBuffer[j].sequenceNumber) {
        DataRecord temp = sendBuffer[i];
        sendBuffer[i] = sendBuffer[j];
        sendBuffer[j] = temp;
      }
    }
  }

  uint32_t lastSeq = sendBuffer[copied - 1].sequenceNumber;

  Serial.printf("💾 Heap перед SSL: %u байт\n", ESP.getFreeHeap());
  Serial.printf("📦 Відправка %u записів\n", copied);

  bool success = sendBatchToSheets(sendBuffer, copied);
  free(sendBuffer);

  if (success) {
    syncStats.lastSentSequence = lastSeq;
    Preferences sheetsPrefs;
    if (sheetsPrefs.begin("sheets_sync", false)) {
      sheetsPrefs.putULong("last_seq", lastSeq);
      sheetsPrefs.end();
    }
    syncStats.totalRecordsSent += copied;
    syncStats.lastSyncTime = millis();
    Serial.printf("✅ Відправлено %u записів, seq: %lu\n", copied, lastSeq);
  } else {
    syncStats.failedSyncs++;
    Serial.println("❌ Помилка відправки");
  }

  syncStats.syncInProgress = false;
  return success;
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
  client->setTimeout(15000);  // 15 сек timeout (Google rate limit = 30-60 сек блокування)

  // Використовуємо ; як роздільник CSV (для європейської локалі Google Sheets)
  // Числа з десятковою КОМОЮ щоб Sheets не плутав з датами
  String csvData = "";
  for (uint16_t i = 0; i < count; i++) {
    time_t ts = records[i].timestamp;
    struct tm* timeinfo = localtime(&ts);
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Форматуємо числа з комою як десятковий розділювач
    char tempCarrier[10], tempRoom[10], tempBME[10], humidity[10];
    snprintf(tempCarrier, sizeof(tempCarrier), "%.1f", records[i].tempCarrier);
    snprintf(tempRoom, sizeof(tempRoom), "%.1f", records[i].tempRoom);
    snprintf(tempBME, sizeof(tempBME), "%.1f", records[i].tempBME);
    snprintf(humidity, sizeof(humidity), "%.1f", records[i].humidity);

    // Замінюємо крапку на кому
    for (char* p = tempCarrier; *p; p++) if (*p == '.') *p = ',';
    for (char* p = tempRoom; *p; p++) if (*p == '.') *p = ',';
    for (char* p = tempBME; *p; p++) if (*p == '.') *p = ',';
    for (char* p = humidity; *p; p++) if (*p == '.') *p = ',';

    // Роздільник полів - крапка з комою (;)
    csvData += String(dateStr) + ";";
    csvData += String(records[i].sequenceNumber) + ";";
    csvData += String(tempCarrier) + ";";
    csvData += String(tempRoom) + ";";
    csvData += String(tempBME) + ";";
    csvData += String(humidity) + ";";
    csvData += String(records[i].pumpPower) + ";";
    csvData += String(records[i].fanPower) + ";";
    csvData += String(records[i].extractorPower) + ";";
    csvData += String(records[i].mode) + ";";

    // Додаємо енергоспоживання (якщо доступно)
    #if ENABLE_ENERGY_MONITOR
    EnergyMeasurements energy = getEnergyMeasurements();
    char powerStr[10];
    snprintf(powerStr, sizeof(powerStr), "%.1f", energy.power);
    for (char* p = powerStr; *p; p++) if (*p == '.') *p = ',';
    csvData += String(powerStr);
    #else
    csvData += "0";
    #endif
    csvData += "\n";
  }

  Serial.printf("📤 Відправка %u записів одним пакетом...\n", count);

  // Виводимо перший рядок для діагностики
  int firstNewline = csvData.indexOf('\n');
  if (firstNewline > 0) {
    Serial.println("📋 Перший рядок:");
    Serial.println(csvData.substring(0, firstNewline));
  }

  Serial.println("🔌 Підключення до script.google.com:443...");
  unsigned long connectStart = millis();

  if (!client->connect("script.google.com", 443)) {
    unsigned long connectTime = millis() - connectStart;
    Serial.printf("❌ SSL підключення не вдалось за %lu мс (heap: %u)\n", connectTime, ESP.getFreeHeap());
    Serial.printf("❌ Можлива причина: SSL handshake timeout або Google блокує\n");
    delete client;
    return false;
  }

  Serial.printf("✅ SSL з'єднання встановлено за %lu мс\n", millis() - connectStart);

  String path = String(GOOGLE_SCRIPT_URL).substring(String(GOOGLE_SCRIPT_URL).indexOf("/macros"));

  client->println("POST " + path + " HTTP/1.1");
  client->println("Host: script.google.com");
  client->println("Content-Type: text/csv");
  client->println("Content-Length: " + String(csvData.length()));
  client->println("Connection: close");
  client->println();
  client->print(csvData);

  Serial.printf("📤 POST відправлено (%u байт CSV)\n", csvData.length());
  Serial.println("⏳ Очікування відповіді від Google Apps Script...");

  unsigned long timeout = millis();
  bool success = false;

  // Чекаємо на відповідь від сервера
  unsigned long loopStart = millis();
  bool timedOut = false;
  bool disconnected = false;
  unsigned long lastStatusReport = millis();

  while (millis() - timeout < 15000) {  // 15 секунд
    // Показуємо статус кожні 2 секунди
    if (millis() - lastStatusReport >= 2000) {
      Serial.printf("⏳ Очікування %lu мс, connected=%d, available=%d\n",
                    millis() - timeout, client->connected(), client->available());
      lastStatusReport = millis();
    }

    if (client->available()) {
      String line = client->readStringUntil('\n');
      if (line.length() > 0) {
        Serial.printf("📨 HTTP: %s\n", line.c_str());

        // Перевіряємо HTTP статус
        if (line.indexOf("HTTP/1.1 200") >= 0 || line.indexOf("HTTP/1.1 202") >= 0) {
          success = true;
          Serial.println("✅ HTTP 200/202 OK");
        }
        if (line.indexOf("HTTP/1.1 302") >= 0) {
          success = true;
          Serial.println("✅ HTTP 302 Redirect OK");
        }
        if (line.indexOf("HTTP/1.1") >= 0) {
          // Прочитали HTTP рядок, чекаємо на залишок відповіді
          delay(200);
          while (client->available()) {
            String data = client->readStringUntil('\n');
            if (data.length() > 0) Serial.printf("📨 Body: %s\n", data.c_str());
          }
          break;
        }
      }
    }
    if (!client->connected()) {
      Serial.println("⚠️ З'єднання розірвано сервером");
      disconnected = true;
      break;
    }
    delay(100);
    yield();
  }

  if (millis() - timeout >= 15000) {
    Serial.println("⏱️ Timeout 15 сек (можливо Google rate limit)");
    timedOut = true;
  }

  Serial.printf("🔍 Цикл завершено: success=%d, timeout=%d, disconn=%d, час=%lu мс\n",
                success, timedOut, disconnected, millis() - loopStart);

  // Примусово очищуємо буфер перед закриттям
  client->flush();
  delay(100);
  client->stop();
  delete client;
  client = nullptr;

  Serial.printf("💾 Heap після SSL: %u байт\n", ESP.getFreeHeap());

  if (success) {
    Serial.println("✅ Записи відправлено");
    return true;
  } else {
    Serial.println("❌ Timeout або помилка HTTP");
    return false;
  }
}

// ============================================================================
// АВТОМАТИЧНА СИНХРОНІЗАЦІЯ
// ============================================================================

void autoSyncTask() {
  static unsigned long lastCheckTime = 0;
  static bool startupDelayDone = false;
  unsigned long now = millis();

  // Затримка 5 хвилин після старту - щоб не синхронізувати одразу
  if (!startupDelayDone) {
    if (now < 300000) return;  // 5 хвилин
    startupDelayDone = true;
    Serial.println("📤 Автосинхронізація активована (затримка старту завершена)");
  }

  // Перевіряємо не частіше ніж раз на хвилину
  if (now - lastCheckTime < 60000) return;
  lastCheckTime = now;

  // Пропускаємо якщо вже синхронізуємось
  if (syncStats.syncInProgress) return;

  // Перевіряємо чи час для щоденної синхронізації (тільки о 23:59)
  if (checkDailySyncTime() && !lastDailySyncDone) {
    Serial.println("🕐 Час щоденної синхронізації (23:59)");
    if (syncToGoogleSheets()) {
      lastDailySyncDone = true;
    }
    return;
  }

  // Скидаємо прапорець щоденної синхронізації о 00:05 (не 00:00 щоб уникнути race condition)
  time_t nowTime;
  time(&nowTime);
  struct tm* timeInfo = localtime(&nowTime);
  if (timeInfo->tm_hour == 0 && timeInfo->tm_min == 5) {
    lastDailySyncDone = false;
  }

  // Перевіряємо інтервал (ТІЛЬКИ якщо пройшло достатньо часу з останньої)
  // НЕ синхронізуємо одразу після старту (lastSyncTime == 0 ігноруємо)
  if (syncStats.lastSyncTime > 0 && (now - syncStats.lastSyncTime) >= SYNC_INTERVAL_MS) {
    // Перевіряємо чи є достатньо нових записів БЕЗ malloc
    // Використовуємо той самий підхід як в syncToGoogleSheets

    if (xSemaphoreTake(getRamBufferMutex(), pdMS_TO_TICKS(500)) != pdTRUE) {
      return; // Буфер зайнятий, спробуємо наступного разу
    }

    uint16_t newRecords = 0;
    for (uint16_t i = 0; i < 1440; i++) {
      if (ramBuffer[i].sequenceNumber > syncStats.lastSentSequence && ramBuffer[i].sequenceNumber > 0) {
        newRecords++;
      }
    }

    xSemaphoreGive(getRamBufferMutex());

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
