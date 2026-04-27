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

  // SPIFFS не підтримує директорії - пропускаємо mkdir
  // Шлях /logs/current.csv створюється автоматично при відкритті файлу

  // Перевіряємо поточний файл
  if (SPIFFS.exists(LOG_CURRENT_FILE)) {
    File file = SPIFFS.open(LOG_CURRENT_FILE, "r");
    if (file) {
      loggerStats.currentFileSize = file.size();
      // Перевіряємо чи файл не пошкоджений (перший байт не має бути 0xFF)
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
    // Створюємо новий файл БЕЗ заголовка (економія 50% місця)
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
        // Приблизний підрахунок записів (~80 байт на запис)
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

  // Ініціалізація лічильників (НЕ обнуляємо totalRecordsSPIFFS - вже підраховано!)
  loggerStats.totalRecordsRAM = 0;
  // loggerStats.totalRecordsSPIFFS зберігає підраховане значення
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
  DataRecord record;
  record.timestamp = getCurrentTimestamp();
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

  // НЕ записувати в SPIFFS якщо OTA активна (конфлікт з flash)
  if (isOTAInProgress()) {
    Serial.println("⏸️ [SPIFFS] Пропущено запис - OTA активна");
    return;
  }

  Serial.printf("💾 [SPIFFS] Writing %d aggregated records...\n", aggregationBufferIndex);

  // Скидаємо watchdog перед файловою операцією
  yield();

  // Відкриваємо файл для дозапису (r+ = read/write без truncate)
  File file = SPIFFS.open(LOG_CURRENT_FILE, "r+");
  if (!file) {
    Serial.printf("❌ [SPIFFS] Failed to open file. Used: %u / %u bytes\n",
                  SPIFFS.usedBytes(), SPIFFS.totalBytes());

    // Спроба відновлення: очистити старі логи
    static uint8_t failCount = 0;
    failCount++;
    if (failCount >= 3) {
      Serial.println("⚠️ [SPIFFS] 3 помилки поспіль - видаляю ВСІ архіви...");

      // Агресивне очищення: видаляємо ВСІ архівні файли
      File root = SPIFFS.open("/");
      if (root) {
        File f = root.openNextFile();
        uint8_t deleted = 0;
        while (f) {
          String fname = f.name();
          f.close();
          if (fname.indexOf("archive_") >= 0) {
            SPIFFS.remove(fname);
            deleted++;
          }
          f = root.openNextFile();
        }
        root.close();
        Serial.printf("✅ [SPIFFS] Видалено %d архівів. Вільно: %u bytes\n",
                      deleted, SPIFFS.totalBytes() - SPIFFS.usedBytes());
      }

      failCount = 0;
    }
    return;
  }

  size_t sizeBefore = file.size();
  Serial.printf("✓ [SPIFFS] File opened, size before: %u bytes\n", sizeBefore);

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

  // Записуємо через seek до кінця (надійніше ніж "a" на SPIFFS)
  file.seek(sizeBefore);
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

  // Копіюємо всі дані з RAM буфера
  memcpy(buffer, ramBuffer, sizeof(ramBuffer));

  xSemaphoreGive(getRamBufferMutex());
  *count = HISTORY_BUFFER_SIZE;  // Повертаємо всі записи з буфера

  return true;
}

// Читання частини даних з RAM (chunked read для економії пам'яті)
bool readRAMDataChunk(DataRecord* buffer, uint16_t offset, uint16_t count) {
  if (!buffer) return false;
  if (offset >= HISTORY_BUFFER_SIZE) return false;
  if (offset + count > HISTORY_BUFFER_SIZE) count = HISTORY_BUFFER_SIZE - offset;

  // Копіюємо тільки потрібну частину
  memcpy(buffer, &ramBuffer[offset], count * sizeof(DataRecord));

  return true;
}

// Допоміжна функція для парсингу CSV файлу в JSON array
static void parseCSVFileToJson(File& file, JsonArray& dataArray,
                                unsigned long startTimestamp, unsigned long endTimestamp) {
  char buffer[256];
  int bufIdx = 0;
  int lineCount = 0;

  while (file.available()) {
    // Скидаємо watchdog кожні 20 рядків
    if (++lineCount % 20 == 0) {
      yield();
    }

    int c = file.read();
    if (c == -1) break;

    if (c == '\n') {
      if (bufIdx > 0) {
        buffer[bufIdx] = '\0';

        unsigned long timestamp = strtoul(buffer, NULL, 10);

        if (timestamp >= startTimestamp && timestamp <= endTimestamp) {
          JsonObject record = dataArray.add<JsonObject>();

          // Розбиваємо CSV на поля
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
  unsigned long startTimestamp = stringToTimestamp(startDate, false);
  unsigned long endTimestamp = stringToTimestamp(endDate, true);

  if (startTimestamp == 0 || endTimestamp == 0) {
    jsonData = "{\"data\":[]}";
    return true;
  }

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
    // Скидаємо watchdog кожні 20 рядків
    if (++lineCount % 20 == 0) {
      yield();
    }

    int c = file.read();
    if (c == -1) break;

    if (c == '\n') {
      if (bufIdx > 0) {
        buffer[bufIdx] = '\0';
        unsigned long timestamp = strtoul(buffer, NULL, 10);

        if (timestamp >= startTimestamp && timestamp <= endTimestamp) {
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
  unsigned long startTimestamp = stringToTimestamp(startDate, false);  // початок дня
  unsigned long endTimestamp = stringToTimestamp(endDate, true);       // кінець дня

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

  // Видаляємо найстаріший архів якщо перевищено ліміт
  if (loggerStats.archiveFilesCount >= LOG_MAX_FILES) {
    // Знаходимо найстаріший файл
    String oldestFile = "";
    unsigned long oldestTime = ULONG_MAX;

    File root = SPIFFS.open("/");
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

  // Перевірка критичного заповнення SPIFFS (>90%)
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  float usagePercent = (float)usedBytes / totalBytes * 100;

  if (usagePercent > 90.0) {
    Serial.printf("⚠️  SPIFFS критично заповнений: %.1f%% - видаляю ВСІ архіви!\n", usagePercent);

    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    uint8_t deletedCount = 0;
    while (file) {
      String fileName = file.name();
      if (fileName.indexOf("archive_") >= 0) {
        file.close();
        SPIFFS.remove(fileName);
        Serial.printf("  Видалено: %s\n", fileName.c_str());
        deletedCount++;
      }
      file = root.openNextFile();
    }
    Serial.printf("✓ Видалено %d архівів (критичне очищення)\n", deletedCount);
    loggerStats.archiveFilesCount = 0;
    return;
  }

  // Звичайне очищення: видаляємо тільки старі файли
  unsigned long cutoffTime = getCurrentTimestamp() - (LOG_RETENTION_DAYS * 24 * 60 * 60);
  uint8_t deletedCount = 0;

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.indexOf("archive_") >= 0) {
      // Витягуємо timestamp з імені файлу (формат: /archive_TIMESTAMP.csv)
      int startIdx = fileName.indexOf("archive_") + 8;  // після "archive_"
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
  // ВИМКНЕНО: форматування SPIFFS конфліктує з web server та призводить до краху
  // Замість цього використовуйте deleteOldArchives() або перезавантажте ESP32
  Serial.println("⚠️  formatSPIFFS() ВИМКНЕНО - використовуйте Factory Reset або перезавантаження");
  Serial.println("💡 Спочатку спробуйте: deleteOldArchives() для очищення місця");
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

unsigned long stringToTimestamp(const char* dateStr, bool endOfDay) {
  // Парсимо рядок формату "YYYY-MM-DD" або "YYYY-MM-DD HH:MM:SS"
  struct tm timeinfo = {0};

  // Спроба спочатку парсити з часом
  int parsed = sscanf(dateStr, "%d-%d-%d %d:%d:%d",
             &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday,
             &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);

  // Якщо не вдалось парсити час, спробуємо тільки дату
  if (parsed < 3) {
    parsed = sscanf(dateStr, "%d-%d-%d",
               &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday);
  }

  if (parsed >= 3) {
    timeinfo.tm_year -= 1900;  // tm_year = роки з 1900
    timeinfo.tm_mon -= 1;       // tm_mon = 0-11
    timeinfo.tm_isdst = -1;     // Автовизначення DST

    // Якщо тільки дата без часу і потрібен кінець дня
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
// FRERTOS ТАСК
// ============================================================================

void dataLoggerTask(void *parameter) {
  Serial.println("✓ Таск логування даних запущено");

  unsigned long lastRAMLog = 0;
  unsigned long lastSPIFFSLog = 0;
  unsigned long lastCleanup = 0;
  const unsigned long CLEANUP_INTERVAL = 24UL * 60UL * 60UL * 1000UL;  // 24 години

  while (1) {
    unsigned long now = millis();

    // Запис в RAM кожну хвилину
    if (now - lastRAMLog >= LOG_INTERVAL_RAM) {
      logDataToRAM();
      lastRAMLog = now;
    }

    // Резервний запис в SPIFFS за налаштованим інтервалом
    unsigned long spiffsInterval = (unsigned long)config.spiffsLogInterval * 60000UL;
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

    // Затримка
    vTaskDelay(pdMS_TO_TICKS(1000));  // Перевірка кожну секунду
  }
}
