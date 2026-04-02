// config_manager.h
// Управління конфігурацією: backup, restore, export/import, factory reset, міграція
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// ============================================================================
// КОНСТАНТИ ВЕРСІОНУВАННЯ
// ============================================================================
#define CONFIG_VERSION_MAJOR 5
#define CONFIG_VERSION_MINOR 0

// ============================================================================
// КОНСТАНТИ ЗБЕРІГАННЯ
// ============================================================================
#define BACKUP_JSON_PATH      "/backup.json"
#define EXPORT_JSON_PATH      "/export.json"
#define CONFIG_NAMESPACE_COUNT 5

// Namespace імена для конфігурації
#define NS_CLIMATE      "climate"
#define NS_WIFI         "wifi"
#define NS_DATA_LOGGER  "data_logger"
#define NS_SHEETS_SYNC  "sheets_sync"

// ============================================================================
// OTA НАЛАШТУВАННЯ
// ============================================================================
#define OTA_DEFAULT_PASSWORD  "klimat2025"
#define OTA_HOSTNAME          "klimat"

// ============================================================================
// СТРУКТУРИ
// ============================================================================

// Версія конфігурації
struct ConfigVersion {
    uint8_t major;
    uint8_t minor;
    uint32_t checksum;
};

// Метадані backup
struct BackupMetadata {
    ConfigVersion version;
    unsigned long timestamp;
    uint32_t totalSize;
    char description[64];
};

// Статус backup операції
struct BackupStatus {
    bool exists;
    unsigned long timestamp;
    uint32_t size;
    bool valid;
    String description;
};

// ============================================================================
// ПРОТОТИПИ ФУНКЦІЙ
// ============================================================================

// Ініціалізація
bool initConfigManager();

// Backup операції
bool createBackup(const char* description = "Manual backup");
bool restoreBackup();
BackupStatus getBackupStatus();

// Export/Import JSON
String exportConfigJSON();
bool importConfigJSON(const String& json);

// Factory Reset
void factoryResetComplete();

// Валідація та перевірка
bool validateConfigIntegrity();
uint32_t calculateConfigCRC();

// Міграція версій
void checkAndMigrateConfig();
void migrateConfigVersion(uint8_t fromMajor, uint8_t fromMinor);

// OTA пароль
String getOTAPassword();
bool setOTAPassword(const String& newPassword);

// Утиліти
uint32_t calculateCRC32(const uint8_t* data, size_t length);
uint32_t calculateCRC32(const String& str);

// Критичні операції (для OTA)
void pauseCriticalTasks();
void resumeCriticalTasks();

// Перевірка першого завантаження після OTA
void checkFirstBootAfterOTA();

// OTA статус (для запобігання конфліктів)
void setOTAInProgress(bool inProgress);
bool isOTAInProgress();

#endif // CONFIG_MANAGER_H
