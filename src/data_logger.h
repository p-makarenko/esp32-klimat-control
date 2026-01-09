#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "FS.h"
#include "SPIFFS.h"

// ============================================================================
// НАЛАШТУВАННЯ ЛОГУВАННЯ
// ============================================================================

#define LOG_INTERVAL_RAM        60000     // Запис в RAM кожну хвилину (1 хв)
#define LOG_INTERVAL_SPIFFS     300000    // Запис в SPIFFS кожні 5 хвилин (5 хв)
#define LOG_FILE_MAX_SIZE       500000    // Максимальний розмір файлу логів (~500 КБ)
#define LOG_MAX_FILES           6         // Максимум файлів (6 файлів × 500 КБ = ~3 МБ)
#define LOG_RETENTION_DAYS      30        // Зберігати дані за останні 30 днів

// Імена файлів
#define LOG_CURRENT_FILE        "/logs/current.csv"
#define LOG_ARCHIVE_PREFIX      "/logs/archive_"
#define LOG_CONFIG_FILE         "/logs/config.json"

// ============================================================================
// СТРУКТУРИ ДАНИХ
// ============================================================================

// Запис даних (один запис = одна хвилина)
struct DataRecord {
  unsigned long timestamp;      // Unix timestamp (секунди з 1970-01-01)
  float tempCarrier;           // Температура теплоносія
  float tempRoom;              // Температура кімнати
  float tempBME;               // Температура BME280
  float humidity;              // Вологість
  uint8_t pumpPower;           // Потужність насоса (0-100%)
  uint8_t fanPower;            // Потужність вентилятора (0-100%)
  uint8_t extractorPower;      // Потужність витяжки (0-100%)
  uint8_t mode;                // Режим роботи (0=AUTO, 1=MANUAL, 2=FORCE, 3=EMERGENCY)
};

// Агрегований запис (для SPIFFS - середнє за 5 хвилин)
struct AggregatedRecord {
  unsigned long timestamp;      // Unix timestamp початку інтервалу
  float avgTempCarrier;        // Середня температура теплоносія
  float avgTempRoom;           // Середня температура кімнати
  float avgTempBME;            // Середня температура BME280
  float avgHumidity;           // Середня вологість
  float minTempCarrier;        // Мінімальна температура теплоносія
  float maxTempCarrier;        // Максимальна температура теплоносія
  float minTempRoom;           // Мінімальна температура кімнаті
  float maxTempRoom;           // Максимальна температура кімнаті
  uint8_t avgPumpPower;        // Середня потужність насоса
  uint8_t avgFanPower;         // Середня потужність вентилятора
  uint8_t avgExtractorPower;   // Середня потужність витяжки
  uint8_t mode;                // Домінуючий режим роботи
};

// Статистика логування
struct LoggerStats {
  unsigned long totalRecordsRAM;      // Загальна кількість записів у RAM
  unsigned long totalRecordsSPIFFS;   // Загальна кількість записів у SPIFFS
  unsigned long lastLogTimeRAM;       // Час останнього запису в RAM
  unsigned long lastLogTimeSPIFFS;    // Час останнього запису в SPIFFS
  size_t currentFileSize;             // Розмір поточного файлу
  uint8_t archiveFilesCount;          // Кількість архівних файлів
  unsigned long spiffsUsedBytes;      // Використано байт у SPIFFS
  unsigned long spiffsTotalBytes;     // Загальний обсяг SPIFFS
};

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ
// ============================================================================

extern LoggerStats loggerStats;
extern DataRecord ramBuffer[1440];     // Буфер на 24 години (1440 хвилин)
extern uint16_t ramBufferIndex;        // Поточний індекс у буфері

// ============================================================================
// ФУНКЦІЇ
// ============================================================================

// Ініціалізація системи логування
bool initDataLogger();

// Запис даних
void logDataToRAM();                              // Запис поточних даних в RAM
void logDataToSPIFFS();                           // Запис агрегованих даних в SPIFFS
void aggregateAndSave();                          // Агрегувати дані з RAM і зберегти в SPIFFS

// Читання даних
bool readRAMData(DataRecord* buffer, uint16_t* count);                        // Читання всіх даних з RAM
bool readRAMDataChunk(DataRecord* buffer, uint16_t offset, uint16_t count);   // Читання частини даних з RAM (chunked)
bool readSPIFFSData(const char* startDate, const char* endDate,
                    String& jsonData);                                        // Читання даних з SPIFFS за період
bool readSPIFFSDataCSV(const char* startDate, const char* endDate,
                       String& csvData);                                      // Читання у форматі CSV

// Управління файлами
void rotateLogFiles();                            // Ротація файлів (архівування старих)
void cleanOldLogs();                              // Видалення старих логів
bool formatSPIFFS();                              // Форматування SPIFFS (видалити всі дані)

// Експорт даних
String exportToCSV(const char* startDate, const char* endDate);   // Експорт даних у CSV
String exportToJSON(const char* startDate, const char* endDate);  // Експорт даних у JSON

// Статистика
LoggerStats getLoggerStats();                     // Отримати статистику логування
void printLoggerInfo();                           // Вивести інформацію в Serial

// Таск для логування
void dataLoggerTask(void *parameter);             // FreeRTOS таск для періодичного запису

// Утиліти
unsigned long getCurrentTimestamp();              // Отримати поточний Unix timestamp
String timestampToString(unsigned long timestamp); // Перетворити timestamp у рядок дати
unsigned long stringToTimestamp(const char* dateStr); // Перетворити рядок дати у timestamp

#endif // DATA_LOGGER_H
