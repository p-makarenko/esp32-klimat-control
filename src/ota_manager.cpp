// ota_manager.cpp
// OTA оновлення: ArduinoOTA + Web Upload + Remote URL

#include "ota_manager.h"
#include "config_manager.h"
#include "actuator_manager.h"
#include "global_declarations.h"
#include <esp_ota_ops.h>

// Глобальний статус OTA
static OTAStatus otaStatus = {
    .inProgress = false,
    .progressPercent = 0,
    .currentVersion = VERSION,
    .targetVersion = "",
    .startTime = 0,
    .errorMessage = "",
    .success = false
};

// Прапорець ініціалізації
static bool otaInitialized = false;

// Час останньої перевірки WiFi під час OTA
static unsigned long lastWiFiCheckDuringOTA = 0;

// LED пін для індикації (використовуємо вбудований якщо є)
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================================

static void setOTALed(bool on) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
}

static void blinkOTALed() {
    static bool state = false;
    state = !state;
    setOTALed(state);
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ OTA
// ============================================================================

bool initOTA() {
    if (otaInitialized) {
        return true;
    }

    Serial.println("Ініціалізація OTA...");

    // Встановлюємо hostname
    ArduinoOTA.setHostname(OTA_HOSTNAME);

    // Встановлюємо пароль з конфігурації
    String otaPassword = getOTAPassword();
    ArduinoOTA.setPassword(otaPassword.c_str());

    // Callback на початок OTA
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "firmware";
        } else {
            type = "filesystem";
        }

        Serial.println("\n╔════════════════════════════════════════════════════════╗");
        Serial.println("║           🔄 OTA ОНОВЛЕННЯ РОЗПОЧАТО                   ║");
        Serial.println("╠════════════════════════════════════════════════════════╣");
        Serial.printf("║  Тип:     %-41s║\n", type.c_str());
        Serial.printf("║  Версія:  %-41s║\n", VERSION);
        Serial.println("╚════════════════════════════════════════════════════════╝");

        // Оновлюємо статус
        otaStatus.inProgress = true;
        otaStatus.progressPercent = 0;
        otaStatus.startTime = millis();
        otaStatus.errorMessage = "";
        otaStatus.success = false;

        // Позначаємо що OTA активна (для запобігання конфліктів)
        setOTAInProgress(true);

        // Створюємо backup перед оновленням
        Serial.println("\n📦 Створення backup конфігурації...");
        if (!backupBeforeOTA()) {
            Serial.println("⚠️  Попередження: Backup не створено!");
        } else {
            Serial.println("✅ Backup в NVS готово");
        }

        // Зупиняємо критичні операції
        Serial.println("\n🛑 Зупинка критичних операцій...");
        pauseCriticalTasks();

        // Ініціалізуємо LED
        setOTALed(true);
    });

    // Callback на прогрес
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int lastPercent = 0;
        unsigned int percent = (progress * 100) / total;

        // Оновлюємо статус
        otaStatus.progressPercent = percent;

        // Виводимо прогрес кожні 5%
        if (percent != lastPercent && percent % 5 == 0) {
            lastPercent = percent;
            Serial.printf("OTA Progress: %u%%\r", percent);

            // Моргаємо LED
            blinkOTALed();
        }

        // Перевіряємо WiFi кожні 5 секунд
        unsigned long now = millis();
        if (now - lastWiFiCheckDuringOTA >= OTA_WIFI_CHECK_MS) {
            lastWiFiCheckDuringOTA = now;

            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("\n❌ WiFi відключено під час OTA!");
                // ArduinoOTA не має методу abort, тому просто логуємо
                otaStatus.errorMessage = "WiFi disconnected";
            }

            // Перевірка timeout
            if (now - otaStatus.startTime > OTA_TIMEOUT_MS) {
                Serial.println("\n❌ OTA timeout!");
                otaStatus.errorMessage = "Timeout";
            }
        }
    });

    // Callback на завершення
    ArduinoOTA.onEnd([]() {
        Serial.println("\n");
        Serial.println("╔════════════════════════════════════════════════════════╗");
        Serial.println("║           ✅ OTA ОНОВЛЕННЯ ЗАВЕРШЕНО                   ║");
        Serial.println("╠════════════════════════════════════════════════════════╣");
        Serial.println("║  Система перезавантажується...                         ║");
        Serial.println("╚════════════════════════════════════════════════════════╝");

        otaStatus.success = true;
        otaStatus.progressPercent = 100;
        setOTALed(true);

        // Позначаємо що OTA завершена
        setOTAInProgress(false);
    });

    // Callback на помилку
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("\n❌ OTA Помилка [%u]: ", error);

        String errorMsg;
        switch (error) {
            case OTA_AUTH_ERROR:
                errorMsg = "Помилка автентифікації";
                break;
            case OTA_BEGIN_ERROR:
                errorMsg = "Помилка початку";
                break;
            case OTA_CONNECT_ERROR:
                errorMsg = "Помилка з'єднання";
                break;
            case OTA_RECEIVE_ERROR:
                errorMsg = "Помилка отримання";
                break;
            case OTA_END_ERROR:
                errorMsg = "Помилка завершення";
                break;
            default:
                errorMsg = "Невідома помилка";
        }

        Serial.println(errorMsg);

        otaStatus.errorMessage = errorMsg;
        otaStatus.inProgress = false;
        otaStatus.success = false;

        // Вимикаємо LED
        setOTALed(false);

        // Позначаємо що OTA завершена (з помилкою)
        setOTAInProgress(false);

        // Відновлюємо критичні операції
        resumeCriticalTasks();
    });

    // Запускаємо OTA
    ArduinoOTA.begin();

    otaInitialized = true;

    Serial.println("  OTA готовий");
    Serial.printf("  Hostname: %s\n", OTA_HOSTNAME);
    Serial.printf("  Пароль: %s\n", "***");
    Serial.println("  Arduino IDE: Tools -> Port -> Network -> klimat");

    return true;
}

// ============================================================================
// ОБРОБКА OTA
// ============================================================================

void handleOTA() {
    if (!otaInitialized) {
        return;
    }

    ArduinoOTA.handle();
}

// ============================================================================
// ОНОВЛЕННЯ З URL
// ============================================================================

bool updateFromURL(const char* url, const char* expectedMD5) {
    Serial.printf("\n📥 Завантаження прошивки з: %s\n", url);

    // Оновлюємо статус
    otaStatus.inProgress = true;
    otaStatus.progressPercent = 0;
    otaStatus.startTime = millis();
    otaStatus.errorMessage = "";
    otaStatus.success = false;
    otaStatus.targetVersion = url;

    // Перевіряємо WiFi
    if (WiFi.status() != WL_CONNECTED) {
        otaStatus.errorMessage = "WiFi not connected";
        otaStatus.inProgress = false;
        Serial.println("❌ WiFi не підключено");
        return false;
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);  // 30 секунд timeout

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("❌ HTTP помилка: %d\n", httpCode);
        otaStatus.errorMessage = "HTTP error: " + String(httpCode);
        otaStatus.inProgress = false;
        http.end();
        return false;
    }

    int contentLength = http.getSize();

    if (contentLength <= 0) {
        Serial.println("❌ Невідомий розмір файлу");
        otaStatus.errorMessage = "Unknown file size";
        otaStatus.inProgress = false;
        http.end();
        return false;
    }

    if (contentLength > OTA_MAX_FIRMWARE_SIZE) {
        Serial.printf("❌ Файл занадто великий: %d bytes (max: %d)\n", contentLength, OTA_MAX_FIRMWARE_SIZE);
        otaStatus.errorMessage = "File too large";
        otaStatus.inProgress = false;
        http.end();
        return false;
    }

    Serial.printf("📊 Розмір прошивки: %d bytes\n", contentLength);

    // Створюємо backup
    Serial.println("\n📦 Створення backup...");
    if (!backupBeforeOTA()) {
        Serial.println("⚠️  Попередження: Backup не створено");
    }

    // Зупиняємо критичні операції
    Serial.println("🛑 Зупинка актуаторів...");
    pauseCriticalTasks();

    // Початок OTA
    if (!Update.begin(contentLength)) {
        Serial.println("❌ Недостатньо місця для OTA");
        otaStatus.errorMessage = "Not enough space";
        otaStatus.inProgress = false;
        resumeCriticalTasks();
        http.end();
        return false;
    }

    Serial.println("\n📥 Завантаження та запис...");

    // Завантаження та запис
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buffer[512];
    unsigned long lastPrint = 0;
    unsigned long lastWiFiCheck = 0;

    while (http.connected() && written < (size_t)contentLength) {
        size_t available = stream->available();

        if (available) {
            int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));

            if (Update.write(buffer, bytesRead) != (size_t)bytesRead) {
                Serial.println("\n❌ Помилка запису");
                otaStatus.errorMessage = "Write error";
                Update.abort();
                http.end();
                resumeCriticalTasks();
                otaStatus.inProgress = false;
                return false;
            }

            written += bytesRead;

            // Оновлюємо прогрес
            unsigned int percent = (written * 100) / contentLength;
            otaStatus.progressPercent = percent;

            // Виводимо прогрес кожну секунду
            unsigned long now = millis();
            if (now - lastPrint > 1000) {
                lastPrint = now;
                Serial.printf("Progress: %u%% (%u/%u bytes)\r", percent, written, contentLength);
                blinkOTALed();
            }

            // Перевіряємо WiFi кожні 5 секунд
            if (now - lastWiFiCheck > OTA_WIFI_CHECK_MS) {
                lastWiFiCheck = now;

                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("\n❌ WiFi відключено!");
                    otaStatus.errorMessage = "WiFi disconnected";
                    Update.abort();
                    http.end();
                    resumeCriticalTasks();
                    otaStatus.inProgress = false;
                    return false;
                }

                // Перевірка timeout
                if (now - otaStatus.startTime > OTA_TIMEOUT_MS) {
                    Serial.println("\n❌ Timeout!");
                    otaStatus.errorMessage = "Timeout";
                    Update.abort();
                    http.end();
                    resumeCriticalTasks();
                    otaStatus.inProgress = false;
                    return false;
                }
            }
        }

        delay(1);
    }

    http.end();

    if (written != (size_t)contentLength) {
        Serial.printf("\n❌ Неповне завантаження: %u/%u bytes\n", written, contentLength);
        otaStatus.errorMessage = "Incomplete download";
        Update.abort();
        resumeCriticalTasks();
        otaStatus.inProgress = false;
        return false;
    }

    Serial.println("\n\n✅ Завантаження завершено");

    // Завершення OTA
    if (!Update.end()) {
        Serial.println("❌ Помилка завершення OTA");
        otaStatus.errorMessage = "OTA end failed";
        resumeCriticalTasks();
        otaStatus.inProgress = false;
        return false;
    }

    // Перевірка MD5
    if (expectedMD5 != nullptr && strlen(expectedMD5) > 0) {
        String actualMD5 = Update.md5String();
        if (!actualMD5.equalsIgnoreCase(expectedMD5)) {
            Serial.println("⚠️  MD5 не співпадає!");
            Serial.printf("  Очікуваний: %s\n", expectedMD5);
            Serial.printf("  Отриманий:  %s\n", actualMD5.c_str());
            otaStatus.errorMessage = "MD5 mismatch";
            // Не abort - прошивка вже записана
        } else {
            Serial.println("✅ MD5 перевірено");
        }
    }

    otaStatus.success = true;
    otaStatus.progressPercent = 100;

    Serial.println("\n╔════════════════════════════════════════════════════════╗");
    Serial.println("║           ✅ OTA ОНОВЛЕННЯ УСПІШНЕ                     ║");
    Serial.println("╠════════════════════════════════════════════════════════╣");
    Serial.println("║  Перезавантаження через 3 секунди...                   ║");
    Serial.println("╚════════════════════════════════════════════════════════╝");

    delay(3000);
    ESP.restart();

    return true;  // Не досягне через restart
}

// ============================================================================
// СТАТУС
// ============================================================================

OTAStatus getOTAStatus() {
    return otaStatus;
}

void resetOTAStatus() {
    otaStatus.inProgress = false;
    otaStatus.progressPercent = 0;
    otaStatus.currentVersion = VERSION;
    otaStatus.targetVersion = "";
    otaStatus.startTime = 0;
    otaStatus.errorMessage = "";
    otaStatus.success = false;
}

// ============================================================================
// BACKUP ПЕРЕД OTA
// ============================================================================

bool backupBeforeOTA() {
    return createBackup("Auto backup before OTA");
}

// ============================================================================
// ЗМІНА ПАРОЛЮ
// ============================================================================

bool changeOTAPassword(const String& newPassword) {
    if (setOTAPassword(newPassword)) {
        // Оновлюємо ArduinoOTA пароль
        ArduinoOTA.setPassword(newPassword.c_str());
        return true;
    }
    return false;
}
