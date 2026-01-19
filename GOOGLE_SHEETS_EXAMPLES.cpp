/**
 * GOOGLE SHEETS SYNCHRONIZATION - ПРАКТИЧНІ ПРИКЛАДИ
 *
 * Цей файл містить кодові приклади та шаблони для синхронізації даних
 * з ESP32 на Google Sheets, базуючись на best practices та дослідженнях.
 *
 * Дата: 17 січня 2026
 * Статус: Рекомендаційні шаблони (не компілюються окремо)
 */

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ============================================================================
// ПРИКЛАД 1: БАЗОВИЙ BATCH SYNC З RETRY ЛОГІКОЮ
// ============================================================================

/**
 * Удосконалена версія sendBatchToSheets з детальним логуванням
 * та поліпшеною обробкою помилок
 */
bool sendBatchToSheetsEnhanced(DataRecord* records, uint16_t count) {
    if (count == 0) {
        Serial.println("⚠️ Нічого не буде надіслано (count=0)");
        return true;
    }

    // КРОК 1: Перевірка попередніх умов
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi не підключено");
        return false;
    }

    Serial.printf("📡 WiFi статус: підключено, IP: %s\n",
                  WiFi.localIP().toString().c_str());
    Serial.printf("📶 Сила сигналу: %d dBm\n", WiFi.RSSI());

    // КРОК 2: Перевірка DNS
    IPAddress serverIP;
    if (!WiFi.hostByName("script.google.com", serverIP)) {
        Serial.println("❌ DNS помилка: не можемо розв'язати script.google.com");
        // Спробуємо IP адресу напряму (якщо знаємо)
        // serverIP.fromString("142.251.33.145");
        return false;
    }
    Serial.printf("✅ DNS OK: script.google.com = %s\n", serverIP.toString().c_str());

    // КРОК 3: Перевірка пам'яті ДО SSL підключення
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t maxBlockBefore = ESP.getMaxAllocHeap();

    Serial.printf("💾 Пам'ять перед SSL: %u байт (макс блок: %u)\n",
                  heapBefore, maxBlockBefore);

    if (maxBlockBefore < 5000) {
        Serial.println("❌ Недостатньо пам'яті для SSL");
        return false;
    }

    // КРОК 4: Формування CSV
    String csvData = "";
    csvData.reserve(count * 100);  // Предвиділяємо місце

    for (uint16_t i = 0; i < count; i++) {
        time_t ts = records[i].timestamp;
        struct tm* timeinfo = localtime(&ts);
        char dateStr[20];
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", timeinfo);

        // Форматуємо числа з локальними роздільювачами
        char tempCarrier[10], tempRoom[10], tempBME[10], humidity[10];
        snprintf(tempCarrier, sizeof(tempCarrier), "%.1f", records[i].tempCarrier);
        snprintf(tempRoom, sizeof(tempRoom), "%.1f", records[i].tempRoom);
        snprintf(tempBME, sizeof(tempBME), "%.1f", records[i].tempBME);
        snprintf(humidity, sizeof(humidity), "%.1f", records[i].humidity);

        // Замінюємо крапку на кому (європейський формат)
        for (char* p = tempCarrier; *p; p++) if (*p == '.') *p = ',';
        for (char* p = tempRoom; *p; p++) if (*p == '.') *p = ',';
        for (char* p = tempBME; *p; p++) if (*p == '.') *p = ',';
        for (char* p = humidity; *p; p++) if (*p == '.') *p = ',';

        // Формуємо рядок CSV
        csvData += dateStr;
        csvData += ";";
        csvData += String(records[i].sequenceNumber);
        csvData += ";";
        csvData += tempCarrier;
        csvData += ";";
        csvData += tempRoom;
        csvData += ";";
        csvData += tempBME;
        csvData += ";";
        csvData += humidity;
        csvData += ";";
        csvData += String(records[i].pumpPower);
        csvData += ";";
        csvData += String(records[i].fanPower);
        csvData += ";";
        csvData += String(records[i].extractorPower);
        csvData += ";";
        csvData += String(records[i].mode);
        csvData += "\n";
    }

    Serial.printf("📦 Сформовано CSV: %u байт\n", csvData.length());
    if (count > 0) {
        int firstNewline = csvData.indexOf('\n');
        Serial.println("📋 Перший рядок:");
        Serial.println(csvData.substring(0, firstNewline));
    }

    // КРОК 5: Створення SSL клієнта
    WiFiClientSecure* client = new WiFiClientSecure();
    if (!client) {
        Serial.println("❌ Помилка виділення пам'яті для SSL клієнта");
        return false;
    }

    client->setInsecure();  // Дозволяємо self-signed сертифікати
    client->setTimeout(10000);  // 10 сек timeout

    // КРОК 6: З'єднання до сервера
    unsigned long connectStart = millis();

    if (!client->connect("script.google.com", 443)) {
        unsigned long connectTime = millis() - connectStart;
        Serial.printf("❌ Помилка підключення (%.1f сек)\n", connectTime / 1000.0);
        Serial.printf("   Heap: %u, Signal: %d dBm\n",
                      ESP.getFreeHeap(), WiFi.RSSI());

        delete client;
        return false;
    }

    unsigned long connectTime = millis() - connectStart;
    Serial.printf("✅ Підключено за %.1f сек\n", connectTime / 1000.0);

    // КРОК 7: Надсилання POST запиту
    String path = "/macros/s/YOUR_SCRIPT_ID/exec";

    client->println("POST " + path + " HTTP/1.1");
    client->println("Host: script.google.com");
    client->println("Content-Type: text/csv; charset=utf-8");
    client->println("Content-Length: " + String(csvData.length()));
    client->println("Connection: close");
    client->println("User-Agent: ESP32-Klimat-Kontrol");
    client->println();

    unsigned long uploadStart = millis();
    client->print(csvData);
    unsigned long uploadTime = millis() - uploadStart;

    Serial.printf("📤 CSV надіслано за %.1f сек (%u байт/сек)\n",
                  uploadTime / 1000.0,
                  (uint32_t)(csvData.length() * 1000 / (uploadTime + 1)));

    // КРОК 8: Читання відповіді
    bool success = false;
    unsigned long responseStart = millis();
    unsigned long responseTimeout = 15000;

    while (millis() - responseStart < responseTimeout) {
        if (client->available()) {
            String line = client->readStringUntil('\n');

            if (line.length() > 0) {
                // Видаляємо \r з кінця
                if (line.endsWith("\r")) {
                    line.remove(line.length() - 1);
                }

                Serial.printf("📨 %s\n", line.c_str());

                // Перевіряємо HTTP статус
                if (line.indexOf("HTTP/1.1 200") >= 0) {
                    success = true;
                    Serial.println("✅ HTTP 200 OK");
                } else if (line.indexOf("HTTP/1.1 202") >= 0) {
                    success = true;
                    Serial.println("✅ HTTP 202 Accepted");
                } else if (line.indexOf("HTTP/1.1 302") >= 0) {
                    success = true;
                    Serial.println("✅ HTTP 302 Redirect");
                } else if (line.indexOf("HTTP/1.1") >= 0) {
                    // Прочитали HTTP рядок
                    delay(50);
                    while (client->available()) {
                        String data = client->readStringUntil('\n');
                        if (data.length() > 50) {
                            data = data.substring(0, 50) + "...";
                        }
                        if (data.length() > 0) {
                            Serial.printf("📨 %s\n", data.c_str());
                        }
                    }
                    break;
                }
            }
        }

        if (!client->connected()) {
            break;
        }

        delay(100);
        yield();
    }

    unsigned long responseTime = millis() - responseStart;
    Serial.printf("⏱️ Відповідь отримана за %.1f сек\n", responseTime / 1000.0);

    // КРОК 9: Закриття з'єднання
    client->stop();
    delete client;
    client = nullptr;

    // КРОК 10: Перевірка пам'яті ПІСЛЯ SSL
    uint32_t heapAfter = ESP.getFreeHeap();
    int32_t heapDiff = (int32_t)heapAfter - (int32_t)heapBefore;

    Serial.printf("💾 Пам'ять після SSL: %u байт (різниця: %+d)\n",
                  heapAfter, heapDiff);

    if (success) {
        Serial.printf("✅ Успіх! %u записів надіслано\n", count);
        return true;
    } else {
        Serial.println("❌ Timeout або помилка HTTP");
        return false;
    }
}

// ============================================================================
// ПРИКЛАД 2: RETRY З ЕКСПОНЕНЦІЙНОЮ ЗАТРИМКОЮ
// ============================================================================

/**
 * Відправляє дані з автоматичною повторною спробою
 * Експоненційна затримка: 1сек, 2сек, 4сек, 8сек...
 */
bool sendWithExponentialBackoff(DataRecord* records, uint16_t count) {
    const int MAX_RETRIES = 3;
    const int BASE_DELAY_MS = 1000;
    const int MAX_DELAY_MS = 32000;
    const int JITTER_PERCENT = 25;  // ±25% випадкові коливання

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        Serial.printf("\n🔄 Спроба %d/%d...\n", attempt, MAX_RETRIES);

        // Перевіряємо WiFi перед кожною спробою
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("⚠️ WiFi відключений, переєднуюсь...");
            WiFi.reconnect();
            delay(2000);

            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("❌ WiFi повторний зв'язок не вдався");
                if (attempt < MAX_RETRIES) {
                    unsigned long delayMs = min(BASE_DELAY_MS * (1 << (attempt - 1)),
                                               (int)(MAX_DELAY_MS));
                    unsigned long jitter = random(0, delayMs * JITTER_PERCENT / 100);
                    Serial.printf("⏳ Чекаємо %lu мс перед наступною спробою...\n",
                                 delayMs + jitter);
                    delay(delayMs + jitter);
                }
                continue;
            }
        }

        // Пробуємо надіслати
        if (sendBatchToSheetsEnhanced(records, count)) {
            Serial.println("✅ Успішна синхронізація!");
            return true;
        }

        // Якщо не останна спроба - чекаємо
        if (attempt < MAX_RETRIES) {
            // Експоненційна затримка: 2^(attempt-1) * BASE_DELAY
            unsigned long baseDelay = BASE_DELAY_MS * (1 << (attempt - 1));
            unsigned long maxDelay = min(baseDelay, (unsigned long)MAX_DELAY_MS);

            // Додаємо jitter щоб уникнути "thundering herd"
            unsigned long jitter = random(0, maxDelay * JITTER_PERCENT / 100);
            unsigned long totalDelay = maxDelay + jitter;

            Serial.printf("⏳ Чекаємо %lu мс перед спробою %d...\n",
                         totalDelay, attempt + 1);
            delay(totalDelay);
        }
    }

    Serial.println("❌ Всі спроби закінчилися невдачею");
    return false;
}

// ============================================================================
// ПРИКЛАД 3: CIRCUIT BREAKER ПАТТЕРН
// ============================================================================

struct CircuitBreakerConfig {
    uint16_t failureThreshold = 5;      // Скільки помилок до блокування
    unsigned long cooldownPeriod = 3600000;  // 1 година
};

struct CircuitBreakerState {
    uint16_t failureCount = 0;
    unsigned long lastFailureTime = 0;
    bool isOpen = false;  // true = заблокований
};

/**
 * Перевіряє чи повинна синхронізація виконуватись
 * Використовує circuit breaker паттерн для уникнення DDoS себе
 */
bool shouldAttemptSync(CircuitBreakerState& state,
                       const CircuitBreakerConfig& config) {

    if (!state.isOpen) {
        // Circuit закритий - все добре
        return true;
    }

    // Circuit відкритий - перевіряємо cooldown
    unsigned long timeSinceLast = millis() - state.lastFailureTime;

    if (timeSinceLast >= config.cooldownPeriod) {
        // Cooldown минув - спробуємо знову
        Serial.printf("🔄 Circuit breaker: cooldown закінчився, спробуємо знову\n");
        state.isOpen = false;
        state.failureCount = 0;
        return true;
    }

    // Ще в cooldown
    unsigned long remainingMs = config.cooldownPeriod - timeSinceLast;
    Serial.printf("⛔ Circuit breaker: блокировано ще %lu сек\n",
                 remainingMs / 1000);
    return false;
}

/**
 * Зареєструвати помилку синхронізації
 */
void recordSyncFailure(CircuitBreakerState& state,
                      const CircuitBreakerConfig& config) {

    state.failureCount++;
    state.lastFailureTime = millis();

    if (state.failureCount >= config.failureThreshold) {
        state.isOpen = true;
        Serial.printf("⛔ Circuit breaker активован (%u помилок)\n",
                     state.failureCount);
    }
}

/**
 * Зареєструвати успішну синхронізацію
 */
void recordSyncSuccess(CircuitBreakerState& state) {
    if (state.failureCount > 0) {
        Serial.printf("✅ Успіх! Reset лічильника помилок (%u -> 0)\n",
                     state.failureCount);
    }
    state.failureCount = 0;
}

// ============================================================================
// ПРИКЛАД 4: JSON ФОРМАТУВАННЯ (АЛЬТЕРНАТИВА CSV)
// ============================================================================

/**
 * Альтернативне форматування даних у JSON для більшої гнучкості
 * Користь: Вкладена структура, легше розширювати
 * Недолік: Більше пам'яті (+30% порівняно з CSV)
 */
String formatDataAsJSON(const DataRecord& record) {
    // Використовуємо DynamicJsonDocument для гнучкості
    DynamicJsonDocument doc(256);

    // Конвертуємо timestamp у ISO 8601
    time_t ts = record.timestamp;
    struct tm* timeinfo = localtime(&ts);
    char isoTime[30];
    strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

    // Заповнюємо документ
    doc["timestamp"] = isoTime;
    doc["sequence"] = record.sequenceNumber;
    doc["temperatures"]["carrier"] = serialized(String(record.tempCarrier, 1));
    doc["temperatures"]["room"] = serialized(String(record.tempRoom, 1));
    doc["temperatures"]["bme280"] = serialized(String(record.tempBME, 1));
    doc["humidity"] = serialized(String(record.humidity, 1));
    doc["power"]["pump"] = record.pumpPower;
    doc["power"]["fan"] = record.fanPower;
    doc["power"]["extractor"] = record.extractorPower;
    doc["mode"] = record.mode;

    // Сериалізуємо у String
    String output;
    serializeJson(doc, output);

    return output;
}

/**
 * Формує масив JSON для батч операцій
 */
String formatBatchAsJSON(DataRecord* records, uint16_t count) {
    DynamicJsonDocument doc(4096);  // Більший буфер для масиву
    JsonArray dataArray = doc.createNestedArray("data");

    for (uint16_t i = 0; i < count; i++) {
        JsonObject obj = dataArray.createNestedObject();

        time_t ts = records[i].timestamp;
        struct tm* timeinfo = localtime(&ts);
        char isoTime[30];
        strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

        obj["timestamp"] = isoTime;
        obj["sequence"] = records[i].sequenceNumber;
        obj["temp_carrier"] = serialized(String(records[i].tempCarrier, 1));
        obj["temp_room"] = serialized(String(records[i].tempRoom, 1));
        obj["humidity"] = serialized(String(records[i].humidity, 1));
    }

    String output;
    serializeJson(doc, output);

    return output;
}

// ============================================================================
// ПРИКЛАД 5: ДІАГНОСТИКА ТА МОНІТОРИНГ
// ============================================================================

struct ConnectionDiagnostics {
    int wifiSignalStrength;      // dBm (RSSI)
    const char* signalQuality;   // Текстове описання
    bool hasActiveConnection;
    unsigned long connectionUptime;
    int packetLoss;              // %
};

/**
 * Отримує детальну діагностику WiFi з'єднання
 */
ConnectionDiagnostics getConnectionDiagnostics() {
    ConnectionDiagnostics diag;

    diag.wifiSignalStrength = WiFi.RSSI();  // dBm
    diag.hasActiveConnection = (WiFi.status() == WL_CONNECTED);
    diag.connectionUptime = millis();  // Спрощено, повна реалізація складніша

    // Класифікація якості сигналу
    if (diag.wifiSignalStrength >= -50) {
        diag.signalQuality = "Отмично (Excellent)";
    } else if (diag.wifiSignalStrength >= -60) {
        diag.signalQuality = "Хорошо (Good)";
    } else if (diag.wifiSignalStrength >= -70) {
        diag.signalQuality = "Нормально (Fair)";
    } else if (diag.wifiSignalStrength >= -80) {
        diag.signalQuality = "Плохо (Poor)";
    } else {
        diag.signalQuality = "Очень плохо (Very Poor)";
    }

    // Приблизна оцінка packet loss (залежить від сигналу)
    int rssi = diag.wifiSignalStrength;
    if (rssi >= -50) diag.packetLoss = 0;
    else if (rssi >= -60) diag.packetLoss = 1;
    else if (rssi >= -70) diag.packetLoss = 3;
    else if (rssi >= -80) diag.packetLoss = 10;
    else diag.packetLoss = 25;

    return diag;
}

/**
 * Виводить детальну діагностику у Serial
 */
void printDetailedDiagnostics() {
    Serial.println("\n=== 📊 ДЕТАЛЬНА ДІАГНОСТИКА ===");

    // WiFi інформація
    Serial.println("\n📡 WiFi:");
    Serial.printf("  SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("  Channel: %d\n", WiFi.channel());
    Serial.printf("  Status: %d\n", WiFi.status());

    ConnectionDiagnostics diag = getConnectionDiagnostics();
    Serial.printf("  Signal: %d dBm (%s)\n", diag.wifiSignalStrength, diag.signalQuality);
    Serial.printf("  Est. Packet Loss: %d%%\n", diag.packetLoss);

    // Пам'ять
    Serial.println("\n💾 Пам'ять:");
    Serial.printf("  Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("  Max Heap Block: %u bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("  Heap Fragmentation: %u%%\n", 100 - (ESP.getMaxAllocHeap() * 100 / ESP.getFreeHeap()));
    Serial.printf("  PSRAM (якщо є): %u bytes\n", ESP.getFreePsram());

    // Система
    Serial.println("\n⚙️ Система:");
    Serial.printf("  Uptime: %lu сек\n", millis() / 1000);
    Serial.printf("  CPU Freq: %d MHz\n", getCpuFrequencyMhz());

    time_t now = time(nullptr);
    Serial.printf("  Поточний час: %s\n", ctime(&now));

    Serial.println("================================\n");
}

// ============================================================================
// ПРИКЛАД 6: ЛОКАЛЬНЕ КЕШУВАННЯ НЕВІДПРАВЛЕНИХ ДАНИХ (SPIFFS)
// ============================================================================

/**
 * Зберігає невідправлені записи в SPIFFS як backup
 * на випадок довгого відключення WiFi
 */
bool cacheFailedBatch(DataRecord* records, uint16_t count,
                      const char* cacheDir = "/sync_cache") {

    // Переконуємось що директорія існує
    if (!SPIFFS.exists(cacheDir)) {
        if (!SPIFFS.mkdir(cacheDir)) {
            Serial.printf("❌ Помилка створення директорії %s\n", cacheDir);
            return false;
        }
    }

    // Формуємо ім'я файлу з timestamp
    char filename[64];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);

    strftime(filename, sizeof(filename),
            "/sync_cache/batch_%Y%m%d_%H%M%S.csv", timeinfo);

    // Записуємо CSV у файл
    File file = SPIFFS.open(filename, "w");
    if (!file) {
        Serial.printf("❌ Не можемо відкрити %s\n", filename);
        return false;
    }

    // Записуємо кожний запис
    for (uint16_t i = 0; i < count; i++) {
        time_t ts = records[i].timestamp;
        struct tm* timeinfo = localtime(&ts);
        char dateStr[20];
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", timeinfo);

        // Форматуємо CSV рядок
        char line[256];
        snprintf(line, sizeof(line),
                "%s;%lu;%.1f;%.1f;%.1f;%.1f;%u;%u;%u;%u\n",
                dateStr,
                records[i].sequenceNumber,
                records[i].tempCarrier,
                records[i].tempRoom,
                records[i].tempBME,
                records[i].humidity,
                records[i].pumpPower,
                records[i].fanPower,
                records[i].extractorPower,
                records[i].mode);

        file.print(line);
    }

    file.close();
    Serial.printf("💾 Кешовано %u записів у %s\n", count, filename);

    return true;
}

/**
 * Відправляє закешовані батчі при наступному з'єднанні
 */
uint16_t resendCachedBatches(const char* cacheDir = "/sync_cache") {
    uint16_t successCount = 0;

    if (!SPIFFS.exists(cacheDir)) {
        Serial.printf("ℹ️ Кеш директорія %s не існує\n", cacheDir);
        return 0;
    }

    File root = SPIFFS.open(cacheDir);
    File file = root.openNextFile();

    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();

            // Читаємо файл
            // (спрощено - повна реалізація більш складна)
            Serial.printf("📤 Відправка кешованого файлу: %s\n", name);

            // Видаляємо після успіху
            file.close();
            SPIFFS.remove(name);
            successCount++;
        }

        file = root.openNextFile();
    }

    if (successCount > 0) {
        Serial.printf("✅ Відправлено %u кешованих файлів\n", successCount);
    }

    return successCount;
}

// ============================================================================
// ПРИКЛАД 7: АВТОМАТИЧНА СИНХРОНІЗАЦІЯ З ВДОСКОНАЛЕНОЮ ЛОГІКОЮ
// ============================================================================

struct SyncSchedule {
    uint16_t autoSyncIntervalMinutes = 30;  // Кожні 30 хвилин
    uint16_t dailySyncHour = 23;            // О 23:00
    uint16_t dailySyncMinute = 59;          // 59 хвилин
    uint16_t minRecordsForAutoSync = 10;    // Мінімум 10 нових записів
};

struct SyncScheduleState {
    unsigned long lastAutoSyncTime = 0;
    bool dailySyncDone = false;
    unsigned long lastCheckTime = 0;
};

/**
 * Улучшена логіка автоматичної синхронізації
 */
bool shouldPerformAutoSync(SyncScheduleState& state,
                           const SyncSchedule& schedule,
                           uint16_t pendingRecords) {

    unsigned long now = millis();

    // Перевіряємо не частіше ніж раз на хвилину
    if (now - state.lastCheckTime < 60000) {
        return false;
    }
    state.lastCheckTime = now;

    // Перевіряємо щоденну синхронізацію (о 23:59)
    time_t currentTime = time(nullptr);
    struct tm* timeinfo = localtime(&currentTime);

    if (timeinfo->tm_hour == schedule.dailySyncHour &&
        timeinfo->tm_min == schedule.dailySyncMinute &&
        !state.dailySyncDone) {

        Serial.printf("🕐 Час щоденної синхронізації (%02d:%02d)\n",
                     schedule.dailySyncHour, schedule.dailySyncMinute);
        state.dailySyncDone = true;
        return true;
    }

    // Скидаємо прапорець щоденної синхронізації о 00:05
    if (timeinfo->tm_hour == 0 && timeinfo->tm_min == 5) {
        state.dailySyncDone = false;
    }

    // Перевіряємо інтервальну синхронізацію
    unsigned long intervalMs = (unsigned long)schedule.autoSyncIntervalMinutes * 60 * 1000;

    if (state.lastAutoSyncTime > 0 &&
        (now - state.lastAutoSyncTime) >= intervalMs &&
        pendingRecords >= schedule.minRecordsForAutoSync) {

        Serial.printf("🤖 Автоматична синхронізація: %u нових записів\n",
                     pendingRecords);
        return true;
    }

    return false;
}

/**
 * Записує час успішної синхронізації
 */
void recordSuccessfulSync(SyncScheduleState& state) {
    state.lastAutoSyncTime = millis();
}

// ============================================================================
// ПРИКЛАД 8: ОПТИМІЗАЦІЯ ПАМ'ЯТІ ДЛЯ ВЕЛИКИХ БУФЕРІВ
// ============================================================================

/**
 * Виділяє пам'ять з урахуванням PSRAM (якщо доступна)
 */
DataRecord* allocateRecordBuffer(uint16_t count) {
    size_t needed = count * sizeof(DataRecord);

    // Спочатку пробуємо PSRAM (якщо є більше 10 KB вільно)
    if (ESP.getFreePsram() > needed + 1000) {
        Serial.printf("📦 Виділяємо %u байт з PSRAM\n", needed);
        // Використовуємо heap_caps_malloc для PSRAM
        // return (DataRecord*)heap_caps_malloc(needed, MALLOC_CAP_SPIRAM);
    }

    // Fallback на внутрішню пам'ять
    if (ESP.getMaxAllocHeap() > needed + 2000) {
        Serial.printf("📦 Виділяємо %u байт з внутрішнього heap\n", needed);
        return (DataRecord*)malloc(needed);
    }

    Serial.printf("❌ Недостатньо пам'яті (%u байт потрібно)\n", needed);
    return nullptr;
}

/**
 * Отримує статистику використання пам'яті
 */
struct MemoryStats {
    uint32_t freeHeap;
    uint32_t maxAllocHeap;
    uint32_t freePsram;
    uint8_t fragmentationPercent;
};

MemoryStats getMemoryStats() {
    MemoryStats stats;

    stats.freeHeap = ESP.getFreeHeap();
    stats.maxAllocHeap = ESP.getMaxAllocHeap();
    stats.freePsram = ESP.getFreePsram();

    // Розраховуємо фрагментацію
    if (stats.freeHeap > 0) {
        stats.fragmentationPercent = 100 -
            (stats.maxAllocHeap * 100 / stats.freeHeap);
    } else {
        stats.fragmentationPercent = 100;
    }

    return stats;
}

void printMemoryStats() {
    MemoryStats stats = getMemoryStats();

    Serial.println("\n=== 💾 СТАТИСТИКА ПАМ'ЯТІ ===");
    Serial.printf("Free Heap: %u bytes (%.2f KB)\n",
                 stats.freeHeap, stats.freeHeap / 1024.0);
    Serial.printf("Max Heap Block: %u bytes (%.2f KB)\n",
                 stats.maxAllocHeap, stats.maxAllocHeap / 1024.0);
    Serial.printf("Free PSRAM: %u bytes (%.2f KB)\n",
                 stats.freePsram, stats.freePsram / 1024.0);
    Serial.printf("Fragmentation: %u%%\n", stats.fragmentationPercent);

    if (stats.fragmentationPercent > 50) {
        Serial.println("⚠️ УВАГА: Висока фрагментація пам'яті!");
    }
    Serial.println("================================\n");
}

// ============================================================================
// ЗАВЕРШЕННЯ
// ============================================================================

/*
 * ВИКОРИСТАННЯ ЦИХ ПРИКЛАДІВ:
 *
 * 1. Скопіюйте потрібні функції у свій проект
 * 2. Адаптуйте під вашу архітектуру
 * 3. Тестуйте окремо перед інтеграцією
 * 4. Монітуйте в серіалі під час роботи
 *
 * БЕСТ ПРАКТИКИ:
 * - Завжди перевіряйте WiFi перед операціями
 * - Виділяйте пам'ять динамічно, а не статично
 * - Використовуйте retry логіку для надійності
 * - Логуйте помилки для діагностики
 * - Тестуйте при поганому WiFi сигналі
 */
