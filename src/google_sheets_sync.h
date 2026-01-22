#ifndef GOOGLE_SHEETS_SYNC_H
#define GOOGLE_SHEETS_SYNC_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "data_logger.h"

// ============================================================================
// НАЛАШТУВАННЯ GOOGLE SHEETS
// ============================================================================

// URL Google Apps Script Web App (оновлено для GET запитів)
#define GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/AKfycbzNjLPxfI2AbmTR4rlBTIWrU835YNNmU88qQ2IzltJWOQEomUMZF669IrPmaZS3xw1H/exec"

// Налаштування синхронізації
#define SYNC_INTERVAL_MS 1800000          // 30 хвилин
#define SYNC_MIN_NEW_RECORDS 10           // Мінімум нових записів для автосинхронізації
#define SYNC_DAILY_HOUR 23                // Година щоденної синхронізації (23:00)
#define SYNC_DAILY_MINUTE 59              // Хвилина щоденної синхронізації (23:59)
#define SYNC_BATCH_SIZE 50                // Розмір пакету для відправки (записів)

// ============================================================================
// СТРУКТУРИ ДАНИХ
// ============================================================================

struct SyncStats {
  unsigned long lastSyncTime;             // Останній час синхронізації (millis)
  unsigned long lastSentTimestamp;        // Timestamp останнього відправленого запису (legacy)
  uint32_t lastSentSequence;              // Sequence число останнього відправленого запису
  uint16_t totalRecordsSent;              // Всього записів відправлено за сесію
  uint16_t failedSyncs;                   // Кількість невдалих синхронізацій
  bool syncInProgress;                    // Прапорець що синхронізація править
};

// ============================================================================
// ФУНКЦІЇ
// ============================================================================

// Ініціалізація
bool initGoogleSheetsSync();              // Ініціалізувати систему синхронізації

// Синхронізація
bool syncToGoogleSheets();                // Ручна синхронізація
void autoSyncTask();                      // Автоматична синхронізація (викликати в loop)
bool sendBatchToSheets(DataRecord* records, uint16_t count); // Відправка пакету даних

// Статистика
SyncStats getSyncStats();                 // Отримати статистику синхронізації
void printSyncInfo();                     // Вивести інформацію про синхронізацію

// Утиліти
bool checkDailySyncTime();                // Перевірити чи час для щоденної синхронізації
unsigned long getLastSentTimestamp();     // Отримати останній відправлений timestamp з NVS
void saveLastSentTimestamp(unsigned long timestamp); // Зберегти timestamp в NVS
void setLastSentSequence(uint32_t seq);   // Оновити lastSentSequence вручну (виправлення дублів)
uint32_t getLastSentSequence();           // Отримати останній відправлений sequence

#endif // GOOGLE_SHEETS_SYNC_H
