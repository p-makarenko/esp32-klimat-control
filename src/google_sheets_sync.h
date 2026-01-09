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
#define GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/AKfycbwJSvYd1i_82sUEDHumz90wctO1KGyqmDGWuDNuStFR0oysu4NPzMbHBk5Fsefk1fhJ/exec"

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
  unsigned long lastSentTimestamp;        // Timestamp останнього відправленого запису
  uint16_t totalRecordsSent;              // Всього записів відправлено за сесію
  uint16_t failedSyncs;                   // Кількість невдалих синхронізацій
  bool syncInProgress;                    // Прапорець що синхронізація триває
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

#endif // GOOGLE_SHEETS_SYNC_H
