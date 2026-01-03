#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "system_core.h"
#include "advanced_climate_logic.h"
#include "learning_system.h"
#include "utility_functions.h"

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
void handleTimePage();
void handleWiFiPage();
void handleHistoryPage();
void handleDebugPage();
void handleLearningPage();
void handleLearningAPI();

// Servo calibration
void handleServoPage();
void handleServoAPI();

// Налаштування Wi-Fi (додаємо!)
void handleWiFiSettingsPage();
void handleSaveWiFiSettings();
void handleScanWiFi();
void handleConnectWiFi();

// Обробка команд із веб-інтерфейсу
void handleWebCommand();
String processWebCommand(const String& cmd);

// Допоміжні функції для Wi-Fi
String wifiStrengthToHTML(int rssi);
String encryptionTypeToString(wifi_auth_mode_t type);

// Завдання веб-сервера
void webTask(void *parameter);

#endif