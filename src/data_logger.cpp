#include "data_logger.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include <time.h>
#include <ArduinoJson.h>

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ
// ============================================================================

LoggerStats loggerStats;
DataRecord ramBuffer[1440];  // 24 години × 60 хвилин = 1440 записів
uint16_t ramBufferIndex = 0;

// Тимчасовий буфер для агрегації (5 записів по 1 хвилині = 5 хвилин)
DataRecord aggregationBuffer[5];
uint8_t aggregationBufferIndex = 0;

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

bool initDataLogger() {
  Serial.println("\n🗄️ Ініціалізація системи логування даних...");

  // Ініціалізація SPIFFS
  if (!SPIFFS.begin(true)) {  // true = format on fail
    Serial.println("❌ Помилка ініціалізації SPIFFS!");
    return false;
  }

  Serial.println("✓ SPIFFS ініціалізовано");

  // Отримуємо інформацію про SPIFFS
  loggerStats.spiffsTotalBytes = SPIFFS.totalBytes();
  loggerStats.spiffsUsedBytes = SPIFFS.usedBytes();

  Serial.printf("  Загальний обсяг: %u байт (%.2f МБ)\n",
                loggerStats.spiffsTotalBytes,
                loggerStats.spiffsTotalBytes / 1024.0 / 1024.0);
  Serial.printf("  Використано: %u байт (%.2f МБ)\n",
                loggerStats.spiffsUsedBytes,
                loggerStats.spiffsUsedBytes / 1024.0 / 1024.0);
  Serial.printf("  Вільно: %u байт (%.2f МБ)\n",
                loggerStats.spiffsTotalBytes - loggerStats.spiffsUsedBytes,
                (loggerStats.spiffsTotalBytes - loggerStats.spiffsUsedBytes) / 1024.0 / 1024.0);

  // Створюємо директорію для логів якщо не існує
  if (!SPIFFS.exists("/logs")) {
    SPIFFS.mkdir("/logs");
    Serial.println("✓ Створено директорію /logs");
  }

  // Перевіряємо поточний файл
  if (SPIFFS.exists(LOG_CURRENT_FILE)) {
    File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
    if (file) {
      loggerStats.currentFileSize = file.size();
      Serial.printf("✓ Знайдено існуючий лог-файл: %u байт\n", loggerStats.currentFileSize);
      file.close();
    }
  } else {
    // Створюємо новий файл БЕЗ заголовка (економія 50% місця)
    File file = SPIFFS.open(LOG_CURRENT_FILE, "w");
    if (file) {
      file.close();
      Serial.println("✓ Створено новий лог-файл");
    }
  }

  // Підрахунок архівних файлів
  File root = SPIFFS.open("/logs");
  File file = root.openNextFile();
  loggerStats.archiveFilesCount = 0;
  while (file) {
    String fileName = file.name();
    if (fileName.startsWith("/logs/archive_")) {
      loggerStats.archiveFilesCount++;
    }
    file = root.openNextFile();
  }

  Serial.printf("✓ Знайдено %d архівних файлів\n", loggerStats.archiveFilesCount);

  // Ініціалізація лічильників
  loggerStats.totalRecordsRAM = 0;
  loggerStats.totalRecordsSPIFFS = 0;
  loggerStats.lastLogTimeRAM = 0;
  loggerStats.lastLogTimeSPIFFS = 0;

  // Очищуємо буфери
  memset(ramBuffer, 0, sizeof(ramBuffer));
  memset(aggregationBuffer, 0, sizeof(aggregationBuffer));

  Serial.println("✅ Систему логування ініціалізовано успішно!\n");
  return true;
}

// ============================================================================
// ЗАПИС ДАНИХ
// ============================================================================

void logDataToRAM() {
  // Створюємо новий запис
  DataRecord record;
  record.timestamp = getCurrentTimestamp();
  record.tempCarrier = sensorData.tempCarrier;
  record.tempRoom = sensorData.tempRoom;
  record.tempBME = sensorData.tempBME;
  record.humidity = sensorData.humidity;
  // Конвертуємо потужності з 0-255 до 0-100%
  record.pumpPower = (heatingState.pumpPower * 100) / 255;
  record.fanPower = (heatingState.fanPower * 100) / 255;
  record.extractorPower = (heatingState.extractorPower * 100) / 255;

  // Визначаємо режим: 0=AUTO, 1=MANUAL, 2=FORCE, 3=EMERGENCY
  if (heatingState.emergencyMode) {
    record.mode = 3;
  } else if (heatingState.forceMode) {
    record.mode = 2;
  } else if (heatingState.manualMode) {
    record.mode = 1;
  } else {
    record.mode = 0;
  }

  // Зберігаємо у RAM буфер (циклічний буфер)
  ramBuffer[ramBufferIndex] = record;
  ramBufferIndex = (ramBufferIndex + 1) % 1440;

  loggerStats.totalRecordsRAM++;
  loggerStats.lastLogTimeRAM = millis();

  // Додаємо у буфер агрегації
  aggregationBuffer[aggregationBufferIndex] = record;
  aggregationBufferIndex++;

  // Якщо буфер агрегації заповнений (5 записів = 5 хвилин), агрегуємо і зберігаємо
  if (aggregationBufferIndex >= 5) {
    aggregateAndSave();
    aggregationBufferIndex = 0;
  }
}

void aggregateAndSave() {
  if (aggregationBufferIndex == 0) return;

  // Створюємо агрегований запис
  AggregatedRecord aggRecord;
  aggRecord.timestamp = aggregationBuffer[0].timestamp;  // Початок інтервалу

  // Ініціалізація мін/макс значень
  aggRecord.minTempCarrier = aggregationBuffer[0].tempCarrier;
  aggRecord.maxTempCarrier = aggregationBuffer[0].tempCarrier;
  aggRecord.minTempRoom = aggregationBuffer[0].tempRoom;
  aggRecord.maxTempRoom = aggregationBuffer[0].tempRoom;

  // Обнуляємо суми
  float sumTempCarrier = 0, sumTempRoom = 0, sumTempBME = 0, sumHumidity = 0;
  uint32_t sumPump = 0, sumFan = 0, sumExtractor = 0;
  uint8_t modeCount[4] = {0, 0, 0, 0};  // AUTO, MANUAL, FORCE, EMERGENCY

  // Агрегуємо дані
  for (uint8_t i = 0; i < aggregationBufferIndex; i++) {
    DataRecord& rec = aggregationBuffer[i];

    sumTempCarrier += rec.tempCarrier;
    sumTempRoom += rec.tempRoom;
    sumTempBME += rec.tempBME;
    sumHumidity += rec.humidity;
    sumPump += rec.pumpPower;
    sumFan += rec.fanPower;
    sumExtractor += rec.extractorPower;

    // Мін/макс
    if (rec.tempCarrier < aggRecord.minTempCarrier) aggRecord.minTempCarrier = rec.tempCarrier;
    if (rec.tempCarrier > aggRecord.maxTempCarrier) aggRecord.maxTempCarrier = rec.tempCarrier;
    if (rec.tempRoom < aggRecord.minTempRoom) aggRecord.minTempRoom = rec.tempRoom;
    if (rec.tempRoom > aggRecord.maxTempRoom) aggRecord.maxTempRoom = rec.tempRoom;

    // Підрахунок режимів
    if (rec.mode < 4) modeCount[rec.mode]++;
  }

  // Обчислюємо середні
  uint8_t count = aggregationBufferIndex;
  aggRecord.avgTempCarrier = sumTempCarrier / count;
  aggRecord.avgTempRoom = sumTempRoom / count;
  aggRecord.avgTempBME = sumTempBME / count;
  aggRecord.avgHumidity = sumHumidity / count;
  aggRecord.avgPumpPower = sumPump / count;
  aggRecord.avgFanPower = sumFan / count;
  aggRecord.avgExtractorPower = sumExtractor / count;

  // Домінуючий режим
  aggRecord.mode = 0;
  uint8_t maxCount = modeCount[0];
  for (uint8_t i = 1; i < 4; i++) {
    if (modeCount[i] > maxCount) {
      maxCount = modeCount[i];
      aggRecord.mode = i;
    }
  }

  // Зберігаємо у SPIFFS
  logDataToSPIFFS();
}

void logDataToSPIFFS() {
  if (aggregationBufferIndex == 0) return;

  // Відкриваємо файл для дозапису
  File file = SPIFFS.open(LOG_CURRENT_FILE, "a");
  if (!file) {
    Serial.println("❌ Помилка відкриття файлу для запису");
    return;
  }

  // Агрегуємо дані перед записом
  AggregatedRecord aggRecord;
  aggRecord.timestamp = aggregationBuffer[0].timestamp;

  float sumTempCarrier = 0, sumTempRoom = 0, sumTempBME = 0, sumHumidity = 0;
  uint32_t sumPump = 0, sumFan = 0, sumExtractor = 0;

  for (uint8_t i = 0; i < aggregationBufferIndex; i++) {
    sumTempCarrier += aggregationBuffer[i].tempCarrier;
    sumTempRoom += aggregationBuffer[i].tempRoom;
    sumTempBME += aggregationBuffer[i].tempBME;
    sumHumidity += aggregationBuffer[i].humidity;
    sumPump += aggregationBuffer[i].pumpPower;
    sumFan += aggregationBuffer[i].fanPower;
    sumExtractor += aggregationBuffer[i].extractorPower;
  }

  uint8_t count = aggregationBufferIndex;

  // Формуємо CSV рядок
  String line = String(aggRecord.timestamp) + ",";
  line += String(sumTempCarrier / count, 1) + ",";
  line += String(sumTempRoom / count, 1) + ",";
  line += String(sumTempBME / count, 1) + ",";
  line += String(sumHumidity / count, 1) + ",";
  line += String(sumPump / count) + ",";
  line += String(sumFan / count) + ",";
  line += String(sumExtractor / count) + ",";
  line += String(aggregationBuffer[0].mode);

  // Записуємо
  file.println(line);
  file.close();

  loggerStats.totalRecordsSPIFFS++;
  loggerStats.lastLogTimeSPIFFS = millis();

  // Оновлюємо розмір файлу
  file = SPIFFS.open(LOG_CURRENT_FILE, "r");
  if (file) {
    loggerStats.currentFileSize = file.size();
    file.close();

    // Перевіряємо чи потрібна ротація
    if (loggerStats.currentFileSize > LOG_FILE_MAX_SIZE) {
      rotateLogFiles();
    }
  }
}

// ============================================================================
// ЧИТАННЯ ДАНИХ
// ============================================================================

bool readRAMData(DataRecord* buffer, uint16_t* count) {
  if (!buffer || !count) return false;

  // Копіюємо всі дані з RAM буфера
  memcpy(buffer, ramBuffer, sizeof(ramBuffer));
  *count = 1440;  // Завжди повертаємо всі 1440 записів (можуть бути нульові)

  return true;
}

// Читання частини даних з RAM (chunked read для економії пам'яті)
bool readRAMDataChunk(DataRecord* buffer, uint16_t offset, uint16_t count) {
  if (!buffer) return false;
  if (offset >= 1440) return false;
  if (offset + count > 1440) count = 1440 - offset;

  // Копіюємо тільки потрібну частину
  memcpy(buffer, &ramBuffer[offset], count * sizeof(DataRecord));

  return true;
}

bool readSPIFFSData(const char* startDate, const char* endDate, String& jsonData) {
  unsigned long startTimestamp = stringToTimestamp(startDate);
  unsigned long endTimestamp = stringToTimestamp(endDate);

  JsonDocument doc;  // Новий варіант ArduinoJson v7
  JsonArray dataArray = doc["data"].to<JsonArray>();

  // Читаємо поточний файл (БЕЗ заголовка)
  File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.length() == 0) continue;

      // Парсимо CSV рядок
      int idx = 0;
      unsigned long timestamp = line.substring(0, line.indexOf(',')).toInt();

      // Перевіряємо чи попадає у діапазон
      if (timestamp >= startTimestamp && timestamp <= endTimestamp) {
        JsonObject record = dataArray.add<JsonObject>();

        // Парсимо всі поля
        int start = 0;
        int end = line.indexOf(',');
        record["timestamp"] = timestamp;

        start = end + 1; end = line.indexOf(',', start);
        record["tempCarrier"] = line.substring(start, end).toFloat();

        start = end + 1; end = line.indexOf(',', start);
        record["tempRoom"] = line.substring(start, end).toFloat();

        start = end + 1; end = line.indexOf(',', start);
        record["tempBME"] = line.substring(start, end).toFloat();

        start = end + 1; end = line.indexOf(',', start);
        record["humidity"] = line.substring(start, end).toFloat();

        start = end + 1; end = line.indexOf(',', start);
        record["pumpPower"] = line.substring(start, end).toInt();

        start = end + 1; end = line.indexOf(',', start);
        record["fanPower"] = line.substring(start, end).toInt();

        start = end + 1; end = line.indexOf(',', start);
        record["extractorPower"] = line.substring(start, end).toInt();

        start = end + 1;
        record["mode"] = line.substring(start).toInt();
      }
    }
    file.close();
  }

  // TODO: Читання архівних файлів (якщо потрібно розширити діапазон)

  serializeJson(doc, jsonData);
  return true;
}

bool readSPIFFSDataCSV(const char* startDate, const char* endDate, String& csvData) {
  unsigned long startTimestamp = stringToTimestamp(startDate);
  unsigned long endTimestamp = stringToTimestamp(endDate);

  csvData = "timestamp,tempCarrier,tempRoom,tempBME,humidity,pumpPower,fanPower,extractorPower,mode\n";

  // Читаємо поточний файл (БЕЗ заголовка)
  File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.length() == 0) continue;

      unsigned long timestamp = line.substring(0, line.indexOf(',')).toInt();

      if (timestamp >= startTimestamp && timestamp <= endTimestamp) {
        csvData += line + "\n";
      }
    }
    file.close();
  }

  return true;
}

// ============================================================================
// УПРАВЛІННЯ ФАЙЛАМИ
// ============================================================================

void rotateLogFiles() {
  Serial.println("🔄 Ротація лог-файлів...");

  // Видаляємо найстаріший архів якщо перевищено ліміт
  if (loggerStats.archiveFilesCount >= LOG_MAX_FILES) {
    // Знаходимо найстаріший файл
    String oldestFile = "";
    unsigned long oldestTime = ULONG_MAX;

    File root = SPIFFS.open("/logs");
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      if (fileName.startsWith("/logs/archive_")) {
        time_t fileTime = file.getLastWrite();
        if (fileTime < oldestTime) {
          oldestTime = fileTime;
          oldestFile = fileName;
        }
      }
      file = root.openNextFile();
    }

    if (oldestFile.length() > 0) {
      SPIFFS.remove(oldestFile);
      Serial.printf("  Видалено старий архів: %s\n", oldestFile.c_str());
      loggerStats.archiveFilesCount--;
    }
  }

  // Створюємо ім'я архівного файлу з поточною датою
  String archiveName = String(LOG_ARCHIVE_PREFIX) + String(getCurrentTimestamp()) + ".csv";

  // Перейменовуємо поточний файл в архівний
  if (SPIFFS.rename(LOG_CURRENT_FILE, archiveName.c_str())) {
    Serial.printf("  Створено архів: %s\n", archiveName.c_str());
    loggerStats.archiveFilesCount++;

    // Створюємо новий поточний файл БЕЗ заголовка
    File file = SPIFFS.open(LOG_CURRENT_FILE, "w");
    if (file) {
      file.close();
      loggerStats.currentFileSize = 0;
      Serial.println("  Створено новий поточний файл");
    }
  }

  Serial.println("✓ Ротація завершена");
}

void cleanOldLogs() {
  Serial.println("🧹 Очищення старих логів...");

  unsigned long cutoffTime = getCurrentTimestamp() - (LOG_RETENTION_DAYS * 24 * 60 * 60);
  uint8_t deletedCount = 0;

  File root = SPIFFS.open("/logs");
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.startsWith("/logs/archive_")) {
      // Витягуємо timestamp з імені файлу
      int startIdx = String(LOG_ARCHIVE_PREFIX).length();
      int endIdx = fileName.indexOf(".csv");
      String timestampStr = fileName.substring(startIdx, endIdx);
      unsigned long fileTimestamp = timestampStr.toInt();

      if (fileTimestamp < cutoffTime) {
        file.close();
        SPIFFS.remove(fileName);
        Serial.printf("  Видалено: %s\n", fileName.c_str());
        deletedCount++;
      }
    }
    file = root.openNextFile();
  }

  Serial.printf("✓ Видалено %d старих файлів\n", deletedCount);
  loggerStats.archiveFilesCount -= deletedCount;
}

bool formatSPIFFS() {
  Serial.println("⚠️  ФОРМАТУВАННЯ SPIFFS...");

  if (SPIFFS.format()) {
    Serial.println("✓ SPIFFS відформатовано");
    return initDataLogger();  // Переініціалізуємо
  }

  Serial.println("❌ Помилка форматування SPIFFS");
  return false;
}

// ============================================================================
// ЕКСПОРТ ДАНИХ
// ============================================================================

String exportToCSV(const char* startDate, const char* endDate) {
  String csvData;
  readSPIFFSDataCSV(startDate, endDate, csvData);
  return csvData;
}

String exportToJSON(const char* startDate, const char* endDate) {
  String jsonData;
  readSPIFFSData(startDate, endDate, jsonData);
  return jsonData;
}

// ============================================================================
// СТАТИСТИКА
// ============================================================================

LoggerStats getLoggerStats() {
  // Оновлюємо інформацію про SPIFFS
  loggerStats.spiffsUsedBytes = SPIFFS.usedBytes();
  loggerStats.spiffsTotalBytes = SPIFFS.totalBytes();

  return loggerStats;
}

void printLoggerInfo() {
  LoggerStats stats = getLoggerStats();

  Serial.println("\n📊 СТАТИСТИКА ЛОГУВАННЯ:");
  Serial.printf("  RAM: %lu записів (%.1f годин)\n",
                stats.totalRecordsRAM,
                stats.totalRecordsRAM / 60.0);
  Serial.printf("  SPIFFS: %lu записів (%.1f днів)\n",
                stats.totalRecordsSPIFFS,
                stats.totalRecordsSPIFFS * 5 / 60.0 / 24.0);
  Serial.printf("  Поточний файл: %u байт\n", stats.currentFileSize);
  Serial.printf("  Архівні файли: %u\n", stats.archiveFilesCount);
  Serial.printf("  SPIFFS: %u / %u байт (%.1f%%)\n",
                stats.spiffsUsedBytes,
                stats.spiffsTotalBytes,
                (stats.spiffsUsedBytes * 100.0) / stats.spiffsTotalBytes);
  Serial.printf("  Останній запис RAM: %lu мс тому\n",
                millis() - stats.lastLogTimeRAM);
  Serial.printf("  Останній запис SPIFFS: %lu мс тому\n\n",
                millis() - stats.lastLogTimeSPIFFS);
}

// ============================================================================
// УТИЛІТИ
// ============================================================================

unsigned long getCurrentTimestamp() {
  // Отримуємо поточний Unix timestamp
  time_t now;
  time(&now);
  return (unsigned long)now;
}

String timestampToString(unsigned long timestamp) {
  time_t rawtime = (time_t)timestamp;
  struct tm* timeinfo = localtime(&rawtime);

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

  return String(buffer);
}

unsigned long stringToTimestamp(const char* dateStr) {
  // Парсимо рядок формату "YYYY-MM-DD" або "YYYY-MM-DD HH:MM:SS"
  struct tm timeinfo = {0};

  if (sscanf(dateStr, "%d-%d-%d %d:%d:%d",
             &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday,
             &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec) >= 3) {
    timeinfo.tm_year -= 1900;  // tm_year = роки з 1900
    timeinfo.tm_mon -= 1;       // tm_mon = 0-11

    return (unsigned long)mktime(&timeinfo);
  }

  return 0;
}

// ============================================================================
// FRERTOS ТАСК
// ============================================================================

void dataLoggerTask(void *parameter) {
  Serial.println("✓ Таск логування даних запущено");

  unsigned long lastRAMLog = 0;
  unsigned long lastCleanup = 0;
  const unsigned long CLEANUP_INTERVAL = 24UL * 60UL * 60UL * 1000UL;  // 24 години

  while (1) {
    unsigned long now = millis();

    // Запис в RAM кожну хвилину
    if (now - lastRAMLog >= LOG_INTERVAL_RAM) {
      logDataToRAM();
      lastRAMLog = now;
    }

    // Очищення старих логів раз на добу
    if (now - lastCleanup >= CLEANUP_INTERVAL) {
      cleanOldLogs();
      lastCleanup = now;
    }

    // Затримка
    vTaskDelay(pdMS_TO_TICKS(1000));  // Перевірка кожну секунду
  }
}
