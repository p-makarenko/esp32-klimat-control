#include "data_logger.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "config_manager.h"
#include <time.h>
#include <cmath>
#include <ArduinoJson.h>

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ
// ============================================================================

LoggerStats loggerStats;
DataRecord ramBuffer[HISTORY_BUFFER_SIZE];  // RAM буфер для історії
uint16_t ramBufferIndex = 0;
uint32_t globalSequence = 0;  // Глобальний лічильник для унікальності записів

// Тимчасовий буфер для агрегації (5 записів по 1 хвилині = 5 хвилин)
DataRecord aggregationBuffer[5];
uint8_t aggregationBufferIndex = 0;


// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

bool initDataLogger() {
  Serial.println("\n🗄️ Ініціалізація системи логування даних...");

  // Завантажуємо sequence counter з NVS
  Preferences prefs;
  if (prefs.begin("data_logger", true)) {  // read-only
    globalSequence = prefs.getULong("seq_num", 0);
    prefs.end();
    Serial.printf("✓ Завантажено sequence counter: %lu\n", globalSequence);
  }

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

  // Перевіряємо поточний файл
  if (SPIFFS.exists(LOG_CURRENT_FILE)) {
    File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
    if (file) {
      loggerStats.currentFileSize = file.size();
      bool corrupted = false;
      if (file.size() > 0) {
        uint8_t firstByte = file.read();
        if (firstByte == 0xFF) {
          corrupted = true;
        }
      }
      file.close();

      if (corrupted) {
        Serial.println("⚠️ Лог-файл пошкоджений (0xFF) - видаляємо і створюємо заново");
        SPIFFS.remove(LOG_CURRENT_FILE);
        File newFile = SPIFFS.open(LOG_CURRENT_FILE, "w");
        if (newFile) {
          newFile.close();
          loggerStats.currentFileSize = 0;
          Serial.println("✓ Створено новий лог-файл");
        }
      } else {
        Serial.printf("✓ Знайдено існуючий лог-файл: %u байт\n", loggerStats.currentFileSize);
      }
    }
  } else {
    // Створюємо новий файл
    File file = SPIFFS.open(LOG_CURRENT_FILE, "w");
    if (file) {
      file.close();
      Serial.println("✓ Створено новий лог-файл");
    }
  }

  // Підрахунок архівних файлів та підрахунок записів в current.csv
  File root = SPIFFS.open("/");
  loggerStats.archiveFilesCount = 0;
  loggerStats.totalRecordsSPIFFS = 0;

  if (root) {
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      size_t fileSize = file.size();

      if (fileName.indexOf("archive_") >= 0) {
        loggerStats.archiveFilesCount++;
        loggerStats.totalRecordsSPIFFS += fileSize / 80;
        Serial.printf("  📁 %s (%u bytes)\n", fileName.c_str(), fileSize);
      } else if (fileName == LOG_CURRENT_FILE || fileName.indexOf("current") >= 0) {
        loggerStats.totalRecordsSPIFFS += fileSize / 80;
        loggerStats.currentFileSize = fileSize;
        Serial.printf("  📄 %s (%u bytes)\n", fileName.c_str(), fileSize);
      }

      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  Serial.printf("✓ Знайдено %d архівів, ~%u записів в SPIFFS\n",
                loggerStats.archiveFilesCount, loggerStats.totalRecordsSPIFFS);

  loggerStats.totalRecordsRAM = 0;
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
  unsigned long currentTs = getCurrentTimestamp();

  DataRecord record;
  record.timestamp = currentTs;
  record.sequenceNumber = ++globalSequence;
  record.dataVersion = 2;
  record.tempCarrier = sensorData.tempCarrier;
  record.tempRoom = sensorData.tempRoom;
  record.tempBME = sensorData.tempBME;
  record.humidity = sensorData.humidity;
  record.co2Level = sensorData.co2Level;
  record.pumpPower = (heatingState.pumpPower * 100) / 255;
  record.fanPower = (heatingState.fanPower * 100) / 255;
  record.extractorPower = (heatingState.extractorPower * 100) / 255;
  record.mode = heatingState.emergencyMode ? 3 : (heatingState.forceMode ? 2 : (heatingState.manualMode ? 1 : 0));

  // Зберігаємо у RAM буфер
  if (xSemaphoreTake(getRamBufferMutex(), pdMS_TO_TICKS(100)) == pdTRUE) {
    ramBuffer[ramBufferIndex] = record;
    ramBufferIndex = (ramBufferIndex + 1) % HISTORY_BUFFER_SIZE;
    xSemaphoreGive(getRamBufferMutex());
  }

  loggerStats.totalRecordsRAM++;
  loggerStats.lastLogTimeRAM = millis();

  // Додаємо у буфер агрегації для SPIFFS
  aggregationBuffer[aggregationBufferIndex] = record;
  aggregationBufferIndex++;
  if (aggregationBufferIndex >= 5) {
    aggregateAndSave();
  }
}

void aggregateAndSave() {
  if (aggregationBufferIndex == 0) return;

  // Зберігаємо у SPIFFS
  logDataToSPIFFS();
}

void logDataToSPIFFS() {
  if (aggregationBufferIndex == 0) return;

  // НЕ записувати в SPIFFS якщо OTA активна
  if (isOTAInProgress()) {
    Serial.println("⏸️ [SPIFFS] Пропущено запис - OTA активна");
    return;
  }

  Serial.printf("💾 [SPIFFS] Writing %d aggregated records...\n", aggregationBufferIndex);
  yield();

  // Надійне відкриття: якщо файл існує, пробуємо "a", якщо не відкрився - пробуємо "r+" зі seek(end)
  File file = SPIFFS.open(LOG_CURRENT_FILE, "a");
  if (!file) {
    if (SPIFFS.exists(LOG_CURRENT_FILE)) {
      file = SPIFFS.open(LOG_CURRENT_FILE, "r+");
      if (file) {
        file.seek(file.size());
      }
    } else {
      file = SPIFFS.open(LOG_CURRENT_FILE, "w");
    }
  }

  if (!file) {
    Serial.printf("❌ [SPIFFS] Failed to open file for append/write. Used: %u / %u bytes\n",
                  SPIFFS.usedBytes(), SPIFFS.totalBytes());

    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    if (total > 0 && ((used * 100) / total) > 85) {
      Serial.println("⚠️ [SPIFFS] Пам'ять заповнена >85% - видаляємо лише один найстаріший архів");
      rotateLogFiles();
    }

    aggregationBufferIndex = 0;
    return;
  }

  size_t sizeBefore = file.size();

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

  // Агрегуємо CO2
  float sumCO2 = 0;
  for (uint8_t i = 0; i < aggregationBufferIndex; i++) {
    sumCO2 += aggregationBuffer[i].co2Level;
  }

  // Формуємо CSV рядок
  String line = String(aggRecord.timestamp) + ",";
  line += String(sumTempCarrier / count, 1) + ",";
  line += String(sumTempRoom / count, 1) + ",";
  line += String(sumTempBME / count, 1) + ",";
  line += String(sumHumidity / count, 1) + ",";
  line += String(sumCO2 / count, 0) + ",";
  line += String(sumPump / count) + ",";
  line += String(sumFan / count) + ",";
  line += String(sumExtractor / count) + ",";
  line += String(aggregationBuffer[0].mode);

  size_t written = file.print(line + "\n");
  file.close();

  yield();

  loggerStats.totalRecordsSPIFFS++;
  loggerStats.lastLogTimeSPIFFS = millis();
  loggerStats.currentFileSize = sizeBefore + written;

  Serial.printf("✓ [SPIFFS] Written %u bytes, total: %u\n", written, loggerStats.currentFileSize);

  // Перевіряємо чи потрібна ротація
  if (loggerStats.currentFileSize > LOG_FILE_MAX_SIZE) {
    rotateLogFiles();
  }

  aggregationBufferIndex = 0;
}

// ============================================================================
// ЧИТАННЯ ДАНИХ
// ============================================================================

bool readRAMData(DataRecord* buffer, uint16_t* count) {
  if (!buffer || !count) return false;

  if (xSemaphoreTake(getRamBufferMutex(), pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  uint16_t total = (loggerStats.totalRecordsRAM > HISTORY_BUFFER_SIZE) ? HISTORY_BUFFER_SIZE : (uint16_t)loggerStats.totalRecordsRAM;
  uint16_t startIdx = (loggerStats.totalRecordsRAM >= HISTORY_BUFFER_SIZE) ? ramBufferIndex : 0;

  for (uint16_t i = 0; i < total; i++) {
    uint16_t idx = (startIdx + i) % HISTORY_BUFFER_SIZE;
    buffer[i] = ramBuffer[idx];
  }

  *count = total;
  xSemaphoreGive(getRamBufferMutex());
  return true;
}

// Читання частини даних з RAM у хронологічному порядку (chunked read)
bool readRAMDataChunk(DataRecord* buffer, uint16_t offset, uint16_t count) {
  if (!buffer) return false;

  if (xSemaphoreTake(getRamBufferMutex(), pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }

  uint16_t total = (loggerStats.totalRecordsRAM > HISTORY_BUFFER_SIZE) ? HISTORY_BUFFER_SIZE : (uint16_t)loggerStats.totalRecordsRAM;
  if (offset >= total) {
    xSemaphoreGive(getRamBufferMutex());
    return false;
  }

  uint16_t recordsToCopy = ((offset + count) > total) ? (total - offset) : count;
  uint16_t startIdx = (loggerStats.totalRecordsRAM >= HISTORY_BUFFER_SIZE) ? ramBufferIndex : 0;

  for (uint16_t i = 0; i < recordsToCopy; i++) {
    uint16_t idx = (startIdx + offset + i) % HISTORY_BUFFER_SIZE;
    buffer[i] = ramBuffer[idx];
  }

  xSemaphoreGive(getRamBufferMutex());
  return true;
}

void parseCSVFileToJson(File& file, JsonArray& dataArray, unsigned long startTimestamp, unsigned long endTimestamp) {
  char buffer[256];
  size_t bufIdx = 0;
  int lineCount = 0;

  while (file.available()) {
    if (++lineCount % 20 == 0) {
      yield();
    }

    int c = file.read();
    if (c == -1) break;

    if (c == '\n') {
      if (bufIdx > 0) {
        buffer[bufIdx] = '\0';

        unsigned long timestamp = strtoul(buffer, NULL, 10);

        // Якщо вказано конкретний діапазон (startTimestamp > 0) - фільтруємо
        // Якщо startTimestamp == 0 - читаємо весь вміст файлу
        bool inRange = (startTimestamp == 0) || (timestamp >= startTimestamp && timestamp <= endTimestamp);

        if (inRange) {
          JsonObject record = dataArray.add<JsonObject>();

          char tempBuf[256];
          strcpy(tempBuf, buffer);
          char* field = strtok(tempBuf, ",");
          int fieldIdx = 0;

          while (field && fieldIdx < 10) {
            if (fieldIdx == 0) record["timestamp"] = strtoul(field, NULL, 10);
            else if (fieldIdx == 1) record["tempCarrier"] = atof(field);
            else if (fieldIdx == 2) record["tempRoom"] = atof(field);
            else if (fieldIdx == 3) record["tempBME"] = atof(field);
            else if (fieldIdx == 4) record["humidity"] = atof(field);
            else if (fieldIdx == 5) record["co2Level"] = atoi(field);
            else if (fieldIdx == 6) record["pumpPower"] = atoi(field);
            else if (fieldIdx == 7) record["fanPower"] = atoi(field);
            else if (fieldIdx == 8) record["extractorPower"] = atoi(field);
            else if (fieldIdx == 9) record["mode"] = atoi(field);

            field = strtok(NULL, ",");
            fieldIdx++;
          }
        }
      }
      bufIdx = 0;
    } else if (c != '\r') {
      if (bufIdx < sizeof(buffer) - 1) {
        buffer[bufIdx++] = c;
      }
    }
  }
}

bool readSPIFFSData(const char* startDate, const char* endDate, String& jsonData) {
  unsigned long startTimestamp = (startDate && strlen(startDate) > 0) ? stringToTimestamp(startDate, false) : 0;
  unsigned long endTimestamp = (endDate && strlen(endDate) > 0) ? stringToTimestamp(endDate, true) : 0;

  JsonDocument doc;
  JsonArray dataArray = doc["data"].to<JsonArray>();

  // 1. Читаємо поточний файл
  File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
  if (file) {
    parseCSVFileToJson(file, dataArray, startTimestamp, endTimestamp);
    file.close();
  }

  // 2. Читаємо архівні файли з кореня SPIFFS (archive_*.csv)
  File root = SPIFFS.open("/");
  if (root) {
    File archiveFile = root.openNextFile();
    while (archiveFile) {
      String fileName = archiveFile.name();
      if (fileName.indexOf("archive_") >= 0) {
        parseCSVFileToJson(archiveFile, dataArray, startTimestamp, endTimestamp);
      }
      archiveFile.close();
      archiveFile = root.openNextFile();
    }
    root.close();
  }

  serializeJson(doc, jsonData);
  return true;
}

// Допоміжна функція для парсингу CSV файлу в рядок
static void parseCSVFileToString(File& file, String& csvData,
                                  unsigned long startTimestamp, unsigned long endTimestamp) {
  char buffer[256];
  int bufIdx = 0;
  int lineCount = 0;

  while (file.available()) {
    if (++lineCount % 20 == 0) {
      yield();
    }

    int c = file.read();
    if (c == -1) break;

    if (c == '\n') {
      if (bufIdx > 0) {
        buffer[bufIdx] = '\0';
        unsigned long timestamp = strtoul(buffer, NULL, 10);

        bool inRange = (startTimestamp == 0) || (timestamp >= startTimestamp && timestamp <= endTimestamp);
        if (inRange) {
          csvData += buffer;
          csvData += "\n";
        }
      }
      bufIdx = 0;
    } else if (c != '\r') {
      if (bufIdx < sizeof(buffer) - 1) {
        buffer[bufIdx++] = c;
      }
    }
  }
}

bool readSPIFFSDataCSV(const char* startDate, const char* endDate, String& csvData) {
  unsigned long startTimestamp = (startDate && strlen(startDate) > 0) ? stringToTimestamp(startDate, false) : 0;
  unsigned long endTimestamp = (endDate && strlen(endDate) > 0) ? stringToTimestamp(endDate, true) : 0;

  csvData = "timestamp,tempCarrier,tempRoom,tempBME,humidity,co2Level,pumpPower,fanPower,extractorPower,mode\n";

  // 1. Читаємо поточний файл
  File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
  if (file) {
    parseCSVFileToString(file, csvData, startTimestamp, endTimestamp);
    file.close();
  }

  // 2. Читаємо архівні файли з кореня SPIFFS (archive_*.csv)
  File root = SPIFFS.open("/");
  if (root) {
    File archiveFile = root.openNextFile();
    while (archiveFile) {
      String fileName = archiveFile.name();
      if (fileName.indexOf("archive_") >= 0) {
        parseCSVFileToString(archiveFile, csvData, startTimestamp, endTimestamp);
      }
      archiveFile.close();
      archiveFile = root.openNextFile();
    }
    root.close();
  }

  return true;
}

// ============================================================================
// УПРАВЛІННЯ ФАЙЛАМИ
// ============================================================================

void rotateLogFiles() {
  Serial.println("🔄 Ротація лог-файлів...");

  File countRoot = SPIFFS.open("/");
  uint8_t count = 0;
  if (countRoot) {
    File f = countRoot.openNextFile();
    while (f) {
      String fn = f.name();
      if (fn.indexOf("archive_") >= 0) count++;
      f.close();
      f = countRoot.openNextFile();
    }
    countRoot.close();
  }
  loggerStats.archiveFilesCount = count;

  if (loggerStats.archiveFilesCount >= LOG_MAX_FILES) {
    String oldestFile = "";
    unsigned long oldestTime = ULONG_MAX;

    File root = SPIFFS.open("/");
    if (root) {
      File file = root.openNextFile();
      while (file) {
        String fileName = file.name();
        if (fileName.indexOf("archive_") >= 0) {
          time_t fileTime = file.getLastWrite();
          if (fileTime < oldestTime) {
            oldestTime = fileTime;
            oldestFile = fileName;
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }

    if (oldestFile.length() > 0) {
      SPIFFS.remove(oldestFile);
      Serial.printf("  Видалено старий архів: %s\n", oldestFile.c_str());
      if (loggerStats.archiveFilesCount > 0) loggerStats.archiveFilesCount--;
    }
  }

  String archiveName = String(LOG_ARCHIVE_PREFIX) + String(getCurrentTimestamp()) + ".csv";

  if (SPIFFS.rename(LOG_CURRENT_FILE, archiveName.c_str())) {
    Serial.printf("  Створено архів: %s\n", archiveName.c_str());
    loggerStats.archiveFilesCount++;

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

  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  if (totalBytes == 0) return;
  float usagePercent = (float)usedBytes / totalBytes * 100;

  if (usagePercent > 90.0) {
    Serial.printf("⚠️  SPIFFS критично заповнений: %.1f%% - видаляю найстаріший архів\n", usagePercent);
    rotateLogFiles();
    return;
  }

  unsigned long cutoffTime = getCurrentTimestamp() - (LOG_RETENTION_DAYS * 24 * 60 * 60);
  uint8_t deletedCount = 0;

  File root = SPIFFS.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      if (fileName.indexOf("archive_") >= 0) {
        int startIdx = fileName.indexOf("archive_") + 8;
        int endIdx = fileName.indexOf(".csv");
        String timestampStr = fileName.substring(startIdx, endIdx);
        unsigned long fileTimestamp = timestampStr.toInt();

        if (fileTimestamp > 0 && fileTimestamp < cutoffTime) {
          file.close();
          SPIFFS.remove(fileName);
          Serial.printf("  Видалено: %s\n", fileName.c_str());
          deletedCount++;
        }
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  Serial.printf("✓ Видалено %d старих файлів\n", deletedCount);
  if (loggerStats.archiveFilesCount >= deletedCount) {
    loggerStats.archiveFilesCount -= deletedCount;
  } else {
    loggerStats.archiveFilesCount = 0;
  }
}

bool formatSPIFFS() {
  Serial.println("⚠️  formatSPIFFS() ВИМКНЕНО - використовуйте Factory Reset або перезавантаження");
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

unsigned long stringToTimestamp(const char* dateStr, bool endOfDay) {
  struct tm timeinfo = {0};

  int parsed = sscanf(dateStr, "%d-%d-%d %d:%d:%d",
             &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday,
             &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);

  if (parsed < 3) {
    parsed = sscanf(dateStr, "%d-%d-%d",
               &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday);
  }

  if (parsed >= 3) {
    timeinfo.tm_year -= 1900;
    timeinfo.tm_mon -= 1;
    timeinfo.tm_isdst = -1;

    if (parsed == 3 && endOfDay) {
      timeinfo.tm_hour = 23;
      timeinfo.tm_min = 59;
      timeinfo.tm_sec = 59;
    }

    return (unsigned long)mktime(&timeinfo);
  }

  return 0;
}

// ============================================================================
// FREERTOS ТАСК
// ============================================================================

void dataLoggerTask(void *parameter) {
  Serial.println("✓ Таск логування даних запущено");

  unsigned long lastRAMLog = 0;
  unsigned long lastSPIFFSLog = 0;
  unsigned long lastCleanup = 0;
  const unsigned long CLEANUP_INTERVAL = 24UL * 60UL * 60UL * 1000UL;

  while (1) {
    unsigned long now = millis();

    // Запис в RAM кожну хвилину
    if (now - lastRAMLog >= LOG_INTERVAL_RAM) {
      logDataToRAM();
      lastRAMLog = now;
    }

    // Резервний запис в SPIFFS за налаштованим інтервалом
    unsigned long spiffsInterval = (unsigned long)config.spiffsLogInterval * 60000UL;
    if (spiffsInterval == 0) spiffsInterval = 300000UL;
    if (now - lastSPIFFSLog >= spiffsInterval) {
      if (aggregationBufferIndex > 0) {
        logDataToSPIFFS();
      }
      lastSPIFFSLog = now;
    }

    // Очищення старих логів раз на добу
    if (now - lastCleanup >= CLEANUP_INTERVAL) {
      cleanOldLogs();
      lastCleanup = now;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
