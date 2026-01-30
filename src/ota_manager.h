// ota_manager.h
// OTA оновлення: ArduinoOTA + Web Upload + Remote URL
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFi.h>

// ============================================================================
// КОНСТАНТИ
// ============================================================================
#define OTA_TIMEOUT_MS        600000   // 10 хвилин максимум на OTA
#define OTA_WIFI_CHECK_MS     5000     // Перевірка WiFi кожні 5 секунд
#define OTA_MAX_FIRMWARE_SIZE 1900000  // Максимальний розмір прошивки (1.9MB)

// ============================================================================
// СТРУКТУРИ
// ============================================================================

// Статус OTA операції
struct OTAStatus {
    bool inProgress;
    uint8_t progressPercent;
    String currentVersion;
    String targetVersion;
    unsigned long startTime;
    String errorMessage;
    bool success;
};

// ============================================================================
// ПРОТОТИПИ ФУНКЦІЙ
// ============================================================================

// Ініціалізація
bool initOTA();

// Обробка OTA (викликати в loop)
void handleOTA();

// Оновлення з URL
bool updateFromURL(const char* url, const char* expectedMD5 = nullptr);

// Статус
OTAStatus getOTAStatus();
void resetOTAStatus();

// Backup перед OTA
bool backupBeforeOTA();

// Зміна паролю
bool changeOTAPassword(const String& newPassword);

// Перевірка чи OTA активний
bool isOTAInProgress();

#endif // OTA_MANAGER_H
