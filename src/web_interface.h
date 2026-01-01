#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "system_core.h"
#include "advanced_climate_logic.h"
#include "learning_system.h"
#include "utility_functions.h"

// ============================================================================
// ВЕБ-ИНТЕРФЕЙС
// ============================================================================

// Инициализация Wi-Fi и веб-сервера
void initWiFi();

// Основные страницы
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

// Настройки Wi-Fi (добавляем!)
void handleWiFiSettingsPage();
void handleSaveWiFiSettings();
void handleScanWiFi();
void handleConnectWiFi();

// Обработка команд из веба
void handleWebCommand();
String processWebCommand(const String& cmd);

// Вспомогательные функции для Wi-Fi
String wifiStrengthToHTML(int rssi);
String encryptionTypeToString(wifi_auth_mode_t type);

// Задача веб-сервера
void webTask(void *parameter);

#endif