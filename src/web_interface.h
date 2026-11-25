#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "system_core.h"
#include "advanced_climate_logic.h"
#include "utility_functions.h"
#include "global_declarations.h"
#include "data_logger.h"

// ============================================================================
// ВЕБ-ІНТЕРФЕЙС
// ============================================================================

// Ініціалізація Wi-Fi та веб-сервера
void initWiFi();

// Основні сторінки
void handleRoot();
void handleStatus();
void handleControlPage();
void handleSettingsPage();
void handleSaveSettings();
void handleWiFiPage();
void handleHistoryPage();
void handleHistoryData();
void handleHistoryStats();
void handleHistoryExport();
void handleHistoryClearLogs();
void handleHelpPage();
void handleDebugPage();
void handleTabletPage();

// Servo calibration
void handleServoPage();
void handleServoAPI();

// Налаштування Wi-Fi (додаємо!)
void handleWiFiSettingsPage();
void handleSaveWiFiSettings();
void handleSaveNetworkSettings();
void handleScanWiFi();
void handleConnectWiFi();

// Автентифікація
bool checkAuth();

// Обробка команд із веб-інтерфейсу
void handleWebCommand();
String processWebCommand(const String& cmd);

// Допоміжні функції для Wi-Fi
String wifiStrengthToHTML(int rssi);
String encryptionTypeToString(wifi_auth_mode_t type);

// Завдання веб-сервера та безпечний перезапуск після відновлення Wi-Fi
void webTask(void *parameter);
void requestWebServerRestart();

// ============================================================================
// ЕНЕРГОКОНТРОЛЕР - forward declarations
// ============================================================================
void handleEnergyPage();
void handleEnergyAPI();
void handleEnergyHistory();
void handleEnergyHistoryStats();

// ============================================================================
// ОСВІТЛЕННЯ - forward declarations
// ============================================================================
void handleLightPage();
void handleLightSave();
void handleLightManual();
void handleLightOff();
void handleLightToggle();
void handleLightPreset();

// ============================================================================
// ФІТОЛАМПА TC421 (окрема сторінка /tc421) - forward declarations
// ============================================================================
void handleTC421Page();
void handleTC421Discover();
void handleTC421Target();
void handleTC421ScheduleSave();
void handleTC421Preset();
void handleTC421ResetProfile();
void handleTC421Manual();
void handleTC421ApiManual();
void handleTC421ApiStatus();
void handleTC421ApiAuto();
void handleTC421Off();
void handleTC421Internal();
void handleTC421SyncTime();

// ============================================================================
// ЕНЕРГО- ТА СВІТЛОВИЙ АУДИТ (/audit) - forward declarations
// ============================================================================
void handleAuditPage();
void handleAuditPhyphoxProxy();
void handleAuditProfileGet();
void handleAuditProfilePost();
void handleAuditDataGet();
void handleAuditDataPost();

// ============================================================================
// OTA ТА BACKUP - forward declarations
// ============================================================================
void handleOTAPage();
void handleOTAStatus();
void handleOTAPasswordChange();
void handleBackupCreate();
void handleBackupRestore();
void handleBackupStatus();
void handleConfigExport();
void handleConfigImport();
void handleFactoryReset();
void handleOTAUploadResult();
void handleOTAUpload();

#endif