// config_manager.cpp
// Управління конфігурацією: backup, restore, export/import, factory reset, міграція

#include "config_manager.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "global_declarations.h"
#include <esp_ota_ops.h>

// Зовнішні змінні
extern Preferences preferences;
extern SystemConfig config;
extern SemaphoreHandle_t configMutex;

// Прапорець ініціалізації
static bool configManagerInitialized = false;

// Прапорець OTA в процесі (для запобігання конфліктів flash)
static bool otaInProgress = false;

// ============================================================================
// OTA СТАТУС (для запобігання конфліктів)
// ============================================================================

void setOTAInProgress(bool inProgress) {
    otaInProgress = inProgress;
    if (inProgress) {
        Serial.println("  ⚠️  OTA активна - Preferences zapisy заборонені");
    } else {
        Serial.println("  ✅ OTA завершена - Preferences запис дозволений");
    }
}

bool isOTAInProgress() {
    return otaInProgress;
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

bool initConfigManager() {
    Serial.println("Ініціалізація Config Manager...");

    // Ініціалізація SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("  SPIFFS mount failed, formatting...");
        if (!SPIFFS.format()) {
            Serial.println("  SPIFFS format failed!");
            return false;
        }
        if (!SPIFFS.begin()) {
            Serial.println("  SPIFFS still failed after format");
            return false;
        }
    }

    // Виводимо інформацію про SPIFFS
    Serial.printf("  SPIFFS: %u / %u bytes (%.1f%% вільно)\n",
                  SPIFFS.usedBytes(), SPIFFS.totalBytes(),
                  100.0 - (SPIFFS.usedBytes() * 100.0 / SPIFFS.totalBytes()));

    // Якщо SPIFFS total менше 100KB - щось не так з partition, форматуємо
    if (SPIFFS.totalBytes() < 100000) {
        Serial.println("  ⚠️  SPIFFS занадто малий - можливо стара partition table");
        Serial.println("  Спроба форматування...");
        SPIFFS.format();
        SPIFFS.begin(true);
        Serial.printf("  Після форматування: %u bytes total\n", SPIFFS.totalBytes());
    }

    // Перевірка цілісності backup файлу
    if (SPIFFS.exists(BACKUP_JSON_PATH)) {
        File file = SPIFFS.open(BACKUP_JSON_PATH, FILE_READ);
        if (file) {
            size_t size = file.size();
            file.close();

            if (size == 0 || size > 50000) {
                // Підозрілий розмір - видалити (зменшили ліміт до 50KB)
                SPIFFS.remove(BACKUP_JSON_PATH);
                Serial.println("  Пошкоджений backup файл видалено");
            } else {
                Serial.printf("  Backup файл знайдено: %u bytes\n", size);
            }
        }
    } else {
        Serial.println("  Backup файл відсутній (це нормально)");
    }

    configManagerInitialized = true;
    Serial.println("  Config Manager готовий");
    return true;
}

// ============================================================================
// CRC32 ОБЧИСЛЕННЯ
// ============================================================================

// CRC32 lookup table
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7a9c, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdede96c5, 0x5791a7de, 0x20969d48,
    0xbdf90b5b, 0xca0ecd6b, 0xabd13d59, 0xdcd60dcf, 0x43d5e0c9, 0x34d2d05f,
    0xa3655cde, 0xd4627448, 0x4d6b04f2, 0x3a6c3464, 0xa4080dc7, 0xd30fad51,
    0x4a0654eb, 0x3d01447d
};

uint32_t calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t calculateCRC32(const String& str) {
    return calculateCRC32((const uint8_t*)str.c_str(), str.length());
}

// ============================================================================
// BACKUP ОПЕРАЦІЇ
// ============================================================================

bool createBackup(const char* description) {
    Serial.printf("Створення backup: %s\n", description);

    // Захоплюємо mutex
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("  Timeout очікування configMutex");
        return false;
    }

    // Зупиняємо критичні операції
    pauseCriticalTasks();

    // Створюємо JSON документ
    JsonDocument doc;

    // Метадані
    doc["version"]["major"] = CONFIG_VERSION_MAJOR;
    doc["version"]["minor"] = CONFIG_VERSION_MINOR;
    doc["timestamp"] = millis();
    doc["description"] = description;

    // === CLIMATE NAMESPACE ===
    Preferences prefs;
    prefs.begin(NS_CLIMATE, true);

    JsonObject climate = doc["climate"].to<JsonObject>();
    climate["tempMin"] = prefs.getFloat("tempMin", TEMP_MIN_THRESHOLD);
    climate["tempMax"] = prefs.getFloat("tempMax", TEMP_MAX_THRESHOLD);
    climate["tempVentMin"] = prefs.getFloat("tempVentMin", TEMP_VENT_MIN);
    climate["tempVentMax"] = prefs.getFloat("tempVentMax", TEMP_VENT_MAX);
    climate["heatingEnabled"] = prefs.getBool("heatingEnabled", true);
    climate["humidifierEnabled"] = prefs.getBool("humidifierEnabled", true);
    climate["statusPeriod"] = prefs.getULong("statusPeriod", STATUS_PRINT_INTERVAL);
    climate["pumpMinPercent"] = prefs.getUChar("pumpMinPercent", PUMP_MIN_DEFAULT);
    climate["pumpMaxPercent"] = prefs.getUChar("pumpMaxPercent", PUMP_MAX_DEFAULT);
    climate["fanMinPercent"] = prefs.getUChar("fanMinPercent", FAN_MIN_DEFAULT);
    climate["fanMaxPercent"] = prefs.getUChar("fanMaxPercent", FAN_MAX_DEFAULT);
    climate["extractorMinPercent"] = prefs.getUChar("extractorMinPer", EXTRACTOR_MIN_DEFAULT);
    climate["extractorMaxPercent"] = prefs.getUChar("extractorMaxPer", EXTRACTOR_MAX_DEFAULT);
    climate["servoClosedAngle"] = prefs.getInt("servoClosedAngle", SERVO_CLOSED_ANGLE);
    climate["servoOpenAngle"] = prefs.getInt("servoOpenAngle", SERVO_OPEN_ANGLE);
    climate["servoSpeed"] = prefs.getInt("servoSpeed", 15);
    climate["autoStatusEnabled"] = prefs.getBool("autoStatusEnabled", true);
    climate["useAuth"] = prefs.getBool("useAuth", false);
    climate["authLogin"] = prefs.getString("authLogin", "admin");
    climate["authPassword"] = prefs.getString("authPassword", "admin");
    climate["otaPassword"] = prefs.getString("otaPassword", OTA_DEFAULT_PASSWORD);
    climate["tempCriticalLow"] = prefs.getFloat("tempCriticalLow", TEMP_CRITICAL_LOW);
    climate["tempEmergencyLow"] = prefs.getFloat("tempEmergencyLow", TEMP_EMERGENCY_LOW);

    // Humidity config
    JsonObject humidity = climate["humidity"].to<JsonObject>();
    humidity["minHumidity"] = prefs.getFloat("humMinHum", HUM_MIN_DEFAULT);
    humidity["maxHumidity"] = prefs.getFloat("humMaxHum", HUM_MAX_DEFAULT);
    humidity["tempCoefficient"] = prefs.getFloat("humTempCoeff", HUM_TEMP_COEFF);
    humidity["enabled"] = prefs.getBool("humEnabled", true);
    humidity["hysteresis"] = prefs.getUChar("humHysteresis", 5);
    humidity["adaptiveMode"] = prefs.getBool("humAdaptive", false);

    // Extractor timer
    JsonObject extractor = climate["extractorTimer"].to<JsonObject>();
    extractor["enabled"] = prefs.getBool("extTimerEnabled", false);
    extractor["onMinutes"] = prefs.getUShort("extTimerOnMin", EXTRACTOR_TIMER_DEFAULT_ON);
    extractor["offMinutes"] = prefs.getUShort("extTimerOffMin", EXTRACTOR_TIMER_DEFAULT_OFF);
    extractor["powerPercent"] = prefs.getUChar("extTimerPower", EXTRACTOR_TIMER_DEFAULT_POWER);

    prefs.end();

    // === WIFI NAMESPACE ===
    prefs.begin(NS_WIFI, true);

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["useStaticIP"] = prefs.getBool("useStaticIP", false);
    wifi["staticIP"] = prefs.getString("staticIP", "");
    wifi["gateway"] = prefs.getString("gateway", "");
    wifi["subnet"] = prefs.getString("subnet", "255.255.255.0");
    wifi["dns"] = prefs.getString("dns", "8.8.8.8");
    wifi["netCount"] = prefs.getInt("netCount", 0);

    // Збережені мережі
    int netCount = prefs.getInt("netCount", 0);
    JsonArray networks = wifi["networks"].to<JsonArray>();
    for (int i = 0; i < netCount && i < 5; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = prefs.getString(("ssid" + String(i)).c_str(), "");
        net["password"] = prefs.getString(("pass" + String(i)).c_str(), "");
    }

    prefs.end();

    // === DATA_LOGGER NAMESPACE ===
    prefs.begin(NS_DATA_LOGGER, true);

    JsonObject dataLogger = doc["data_logger"].to<JsonObject>();
    dataLogger["enabled"] = prefs.getBool("enabled", true);
    dataLogger["interval"] = prefs.getULong("interval", 60000);
    dataLogger["maxRecords"] = prefs.getInt("maxRecords", HISTORY_BUFFER_SIZE);

    prefs.end();

    // === SHEETS_SYNC NAMESPACE ===
    prefs.begin(NS_SHEETS_SYNC, true);

    JsonObject sheetsSync = doc["sheets_sync"].to<JsonObject>();
    sheetsSync["enabled"] = prefs.getBool("enabled", false);
    sheetsSync["scriptUrl"] = prefs.getString("scriptUrl", "");
    sheetsSync["syncInterval"] = prefs.getULong("syncInterval", 300000);
    sheetsSync["batchSize"] = prefs.getInt("batchSize", 30);

    prefs.end();

    // === LEARNING NAMESPACE ===
    prefs.begin(NS_LEARNING, true);

    JsonObject learning = doc["learning"].to<JsonObject>();
    learning["enabled"] = prefs.getBool("enabled", false);
    learning["count"] = prefs.getInt("count", 0);

    prefs.end();

    // Серіалізуємо JSON (без checksum)
    String jsonString;
    serializeJson(doc, jsonString);

    // Обчислюємо CRC32
    uint32_t crc = calculateCRC32(jsonString);
    doc["checksum"] = crc;

    // Серіалізуємо з checksum
    String finalJson;
    serializeJson(doc, finalJson);

    // Записуємо у SPIFFS ТІЛЬКИ якщо це не OTA
    // (під час OTA запис в SPIFFS конфліктує з flash писанням)
    bool spiffsFailed = false;
    File file = SPIFFS.open(BACKUP_JSON_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("  ⚠️  Помилка відкриття SPIFFS для backup (можуть бути конфлікти з OTA)");
        spiffsFailed = true;
    } else {
        size_t written = file.print(finalJson);
        file.close();

        if (written != finalJson.length()) {
            Serial.println("  ⚠️  Помилка запису SPIFFS backup");
            SPIFFS.remove(BACKUP_JSON_PATH);
            spiffsFailed = true;
        }
    }

    resumeCriticalTasks();
    xSemaphoreGive(configMutex);

    // Backup вважається успішним навіть якщо SPIFFS неудачно (дані в NVS збережені)
    Serial.printf("  Backup в NVS успішно (%d bytes, CRC: 0x%08X)\n", finalJson.length(), crc);
    if (spiffsFailed) {
        Serial.println("  💡 SPIFFS запис пропущено (можливо OTA активна)");
    }
    return true;
}

bool restoreBackup() {
    Serial.println("Відновлення з backup...");

    // Перевіряємо наявність файлу
    if (!SPIFFS.exists(BACKUP_JSON_PATH)) {
        Serial.println("  Backup файл не знайдено");
        return false;
    }

    // Захоплюємо mutex
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("  Timeout очікування configMutex");
        return false;
    }

    // Зупиняємо все
    pauseCriticalTasks();

    // Читаємо файл
    File file = SPIFFS.open(BACKUP_JSON_PATH, FILE_READ);
    if (!file) {
        Serial.println("  Помилка відкриття backup файлу");
        resumeCriticalTasks();
        xSemaphoreGive(configMutex);
        return false;
    }

    String jsonString = file.readString();
    file.close();

    // Парсимо JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Serial.printf("  Помилка парсингу JSON: %s\n", error.c_str());
        resumeCriticalTasks();
        xSemaphoreGive(configMutex);
        return false;
    }

    // Перевіряємо CRC
    uint32_t storedCRC = doc["checksum"];
    doc.remove("checksum");

    String jsonWithoutCRC;
    serializeJson(doc, jsonWithoutCRC);
    uint32_t calculatedCRC = calculateCRC32(jsonWithoutCRC);

    if (calculatedCRC != storedCRC) {
        Serial.printf("  CRC mismatch: stored=0x%08X, calculated=0x%08X\n", storedCRC, calculatedCRC);
        Serial.println("  Backup пошкоджений!");
        resumeCriticalTasks();
        xSemaphoreGive(configMutex);
        return false;
    }

    // Перевіряємо версію
    uint8_t major = doc["version"]["major"];
    uint8_t minor = doc["version"]["minor"];

    if (major > CONFIG_VERSION_MAJOR) {
        Serial.printf("  Backup з новішої версії (v%d.%d) - неможливо відновити\n", major, minor);
        resumeCriticalTasks();
        xSemaphoreGive(configMutex);
        return false;
    }

    Serial.printf("  Backup версія: v%d.%d, CRC: OK\n", major, minor);

    // === ВІДНОВЛЕННЯ CLIMATE NAMESPACE ===
    Preferences prefs;
    prefs.begin(NS_CLIMATE, false);
    prefs.clear();

    JsonObject climate = doc["climate"];
    prefs.putFloat("tempMin", climate["tempMin"] | TEMP_MIN_THRESHOLD);
    prefs.putFloat("tempMax", climate["tempMax"] | TEMP_MAX_THRESHOLD);
    prefs.putFloat("tempVentMin", climate["tempVentMin"] | TEMP_VENT_MIN);
    prefs.putFloat("tempVentMax", climate["tempVentMax"] | TEMP_VENT_MAX);
    prefs.putBool("heatingEnabled", climate["heatingEnabled"] | true);
    prefs.putBool("humidifierEnabled", climate["humidifierEnabled"] | true);
    prefs.putULong("statusPeriod", climate["statusPeriod"] | STATUS_PRINT_INTERVAL);
    prefs.putUChar("pumpMinPercent", climate["pumpMinPercent"] | PUMP_MIN_DEFAULT);
    prefs.putUChar("pumpMaxPercent", climate["pumpMaxPercent"] | PUMP_MAX_DEFAULT);
    prefs.putUChar("fanMinPercent", climate["fanMinPercent"] | FAN_MIN_DEFAULT);
    prefs.putUChar("fanMaxPercent", climate["fanMaxPercent"] | FAN_MAX_DEFAULT);
    prefs.putUChar("extractorMinPer", climate["extractorMinPercent"] | EXTRACTOR_MIN_DEFAULT);
    prefs.putUChar("extractorMaxPer", climate["extractorMaxPercent"] | EXTRACTOR_MAX_DEFAULT);
    prefs.putInt("servoClosedAngle", climate["servoClosedAngle"] | SERVO_CLOSED_ANGLE);
    prefs.putInt("servoOpenAngle", climate["servoOpenAngle"] | SERVO_OPEN_ANGLE);
    prefs.putInt("servoSpeed", climate["servoSpeed"] | 15);
    prefs.putBool("autoStatusEnabled", climate["autoStatusEnabled"] | true);
    prefs.putBool("useAuth", climate["useAuth"] | false);
    prefs.putString("authLogin", climate["authLogin"] | "admin");
    prefs.putString("authPassword", climate["authPassword"] | "admin");
    prefs.putString("otaPassword", climate["otaPassword"] | OTA_DEFAULT_PASSWORD);
    prefs.putFloat("tempCriticalLow", climate["tempCriticalLow"] | TEMP_CRITICAL_LOW);
    prefs.putFloat("tempEmergencyLow", climate["tempEmergencyLow"] | TEMP_EMERGENCY_LOW);

    // Humidity config
    JsonObject humidity = climate["humidity"];
    prefs.putFloat("humMinHum", humidity["minHumidity"] | HUM_MIN_DEFAULT);
    prefs.putFloat("humMaxHum", humidity["maxHumidity"] | HUM_MAX_DEFAULT);
    prefs.putFloat("humTempCoeff", humidity["tempCoefficient"] | HUM_TEMP_COEFF);
    prefs.putBool("humEnabled", humidity["enabled"] | true);
    prefs.putUChar("humHysteresis", humidity["hysteresis"] | 5);
    prefs.putBool("humAdaptive", humidity["adaptiveMode"] | false);

    // Extractor timer
    JsonObject extractor = climate["extractorTimer"];
    prefs.putBool("extTimerEnabled", extractor["enabled"] | false);
    prefs.putUShort("extTimerOnMin", extractor["onMinutes"] | EXTRACTOR_TIMER_DEFAULT_ON);
    prefs.putUShort("extTimerOffMin", extractor["offMinutes"] | EXTRACTOR_TIMER_DEFAULT_OFF);
    prefs.putUChar("extTimerPower", extractor["powerPercent"] | EXTRACTOR_TIMER_DEFAULT_POWER);

    // Оновлюємо версію конфігурації
    prefs.putUChar("configMajor", CONFIG_VERSION_MAJOR);
    prefs.putUChar("configMinor", CONFIG_VERSION_MINOR);

    prefs.end();

    // === ВІДНОВЛЕННЯ WIFI NAMESPACE ===
    prefs.begin(NS_WIFI, false);
    prefs.clear();

    JsonObject wifi = doc["wifi"];
    prefs.putBool("useStaticIP", wifi["useStaticIP"] | false);
    prefs.putString("staticIP", wifi["staticIP"] | "");
    prefs.putString("gateway", wifi["gateway"] | "");
    prefs.putString("subnet", wifi["subnet"] | "255.255.255.0");
    prefs.putString("dns", wifi["dns"] | "8.8.8.8");

    JsonArray networks = wifi["networks"];
    int netCount = networks.size();
    prefs.putInt("netCount", netCount);

    for (int i = 0; i < netCount && i < 5; i++) {
        prefs.putString(("ssid" + String(i)).c_str(), networks[i]["ssid"] | "");
        prefs.putString(("pass" + String(i)).c_str(), networks[i]["password"] | "");
    }

    prefs.end();

    // === ВІДНОВЛЕННЯ DATA_LOGGER NAMESPACE ===
    prefs.begin(NS_DATA_LOGGER, false);
    prefs.clear();

    JsonObject dataLogger = doc["data_logger"];
    prefs.putBool("enabled", dataLogger["enabled"] | true);
    prefs.putULong("interval", dataLogger["interval"] | 60000);
    prefs.putInt("maxRecords", dataLogger["maxRecords"] | HISTORY_BUFFER_SIZE);

    prefs.end();

    // === ВІДНОВЛЕННЯ SHEETS_SYNC NAMESPACE ===
    prefs.begin(NS_SHEETS_SYNC, false);
    prefs.clear();

    JsonObject sheetsSync = doc["sheets_sync"];
    prefs.putBool("enabled", sheetsSync["enabled"] | false);
    prefs.putString("scriptUrl", sheetsSync["scriptUrl"] | "");
    prefs.putULong("syncInterval", sheetsSync["syncInterval"] | 300000);
    prefs.putInt("batchSize", sheetsSync["batchSize"] | 30);

    prefs.end();

    // === ВІДНОВЛЕННЯ LEARNING NAMESPACE ===
    prefs.begin(NS_LEARNING, false);
    prefs.clear();

    JsonObject learning = doc["learning"];
    prefs.putBool("enabled", learning["enabled"] | false);
    prefs.putInt("count", learning["count"] | 0);

    prefs.end();

    resumeCriticalTasks();
    xSemaphoreGive(configMutex);

    Serial.println("  Конфігурація успішно відновлена!");
    Serial.println("  Перезавантаження системи...");

    delay(2000);
    ESP.restart();

    return true;  // Не досягне через restart
}

BackupStatus getBackupStatus() {
    BackupStatus status = {false, 0, 0, false, ""};

    if (!SPIFFS.exists(BACKUP_JSON_PATH)) {
        return status;
    }

    File file = SPIFFS.open(BACKUP_JSON_PATH, FILE_READ);
    if (!file) {
        return status;
    }

    status.exists = true;
    status.size = file.size();

    String jsonString = file.readString();
    file.close();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (!error) {
        status.timestamp = doc["timestamp"];
        status.description = doc["description"].as<String>();

        // Перевіряємо CRC
        uint32_t storedCRC = doc["checksum"];
        doc.remove("checksum");

        String jsonWithoutCRC;
        serializeJson(doc, jsonWithoutCRC);
        uint32_t calculatedCRC = calculateCRC32(jsonWithoutCRC);

        status.valid = (calculatedCRC == storedCRC);
    }

    return status;
}

// ============================================================================
// EXPORT/IMPORT JSON
// ============================================================================

String exportConfigJSON() {
    // Використовуємо ту ж логіку що і createBackup, але повертаємо String
    JsonDocument doc;

    doc["version"]["major"] = CONFIG_VERSION_MAJOR;
    doc["version"]["minor"] = CONFIG_VERSION_MINOR;
    doc["timestamp"] = millis();
    doc["description"] = "Export";

    // Climate namespace
    Preferences prefs;
    prefs.begin(NS_CLIMATE, true);

    JsonObject climate = doc["climate"].to<JsonObject>();
    climate["tempMin"] = prefs.getFloat("tempMin", TEMP_MIN_THRESHOLD);
    climate["tempMax"] = prefs.getFloat("tempMax", TEMP_MAX_THRESHOLD);
    climate["tempVentMin"] = prefs.getFloat("tempVentMin", TEMP_VENT_MIN);
    climate["tempVentMax"] = prefs.getFloat("tempVentMax", TEMP_VENT_MAX);
    climate["heatingEnabled"] = prefs.getBool("heatingEnabled", true);
    climate["humidifierEnabled"] = prefs.getBool("humidifierEnabled", true);
    climate["pumpMinPercent"] = prefs.getUChar("pumpMinPercent", PUMP_MIN_DEFAULT);
    climate["pumpMaxPercent"] = prefs.getUChar("pumpMaxPercent", PUMP_MAX_DEFAULT);
    climate["fanMinPercent"] = prefs.getUChar("fanMinPercent", FAN_MIN_DEFAULT);
    climate["fanMaxPercent"] = prefs.getUChar("fanMaxPercent", FAN_MAX_DEFAULT);
    climate["servoClosedAngle"] = prefs.getInt("servoClosedAngle", SERVO_CLOSED_ANGLE);
    climate["servoOpenAngle"] = prefs.getInt("servoOpenAngle", SERVO_OPEN_ANGLE);
    climate["useAuth"] = prefs.getBool("useAuth", false);
    climate["authLogin"] = prefs.getString("authLogin", "admin");
    climate["authPassword"] = prefs.getString("authPassword", "admin");

    prefs.end();

    // WiFi namespace
    prefs.begin(NS_WIFI, true);

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["useStaticIP"] = prefs.getBool("useStaticIP", false);
    wifi["staticIP"] = prefs.getString("staticIP", "");
    wifi["gateway"] = prefs.getString("gateway", "");

    int netCount = prefs.getInt("netCount", 0);
    JsonArray networks = wifi["networks"].to<JsonArray>();
    for (int i = 0; i < netCount && i < 5; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = prefs.getString(("ssid" + String(i)).c_str(), "");
        // Не експортуємо паролі для безпеки
        net["password"] = "********";
    }

    prefs.end();

    String output;
    serializeJsonPretty(doc, output);
    return output;
}

bool importConfigJSON(const String& json) {
    Serial.println("Імпорт конфігурації з JSON...");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.printf("  Помилка парсингу JSON: %s\n", error.c_str());
        return false;
    }

    // Захоплюємо mutex
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        Serial.println("  Timeout очікування configMutex");
        return false;
    }

    pauseCriticalTasks();

    // Імпортуємо тільки climate налаштування (без паролів WiFi)
    Preferences prefs;
    prefs.begin(NS_CLIMATE, false);

    JsonObject climate = doc["climate"];
    if (!climate.isNull()) {
        if (!climate["tempMin"].isNull()) prefs.putFloat("tempMin", climate["tempMin"]);
        if (!climate["tempMax"].isNull()) prefs.putFloat("tempMax", climate["tempMax"]);
        if (!climate["tempVentMin"].isNull()) prefs.putFloat("tempVentMin", climate["tempVentMin"]);
        if (!climate["tempVentMax"].isNull()) prefs.putFloat("tempVentMax", climate["tempVentMax"]);
        if (!climate["heatingEnabled"].isNull()) prefs.putBool("heatingEnabled", climate["heatingEnabled"]);
        if (!climate["humidifierEnabled"].isNull()) prefs.putBool("humidifierEnabled", climate["humidifierEnabled"]);
        if (!climate["pumpMinPercent"].isNull()) prefs.putUChar("pumpMinPercent", climate["pumpMinPercent"]);
        if (!climate["pumpMaxPercent"].isNull()) prefs.putUChar("pumpMaxPercent", climate["pumpMaxPercent"]);
        if (!climate["fanMinPercent"].isNull()) prefs.putUChar("fanMinPercent", climate["fanMinPercent"]);
        if (!climate["fanMaxPercent"].isNull()) prefs.putUChar("fanMaxPercent", climate["fanMaxPercent"]);
        if (!climate["servoClosedAngle"].isNull()) prefs.putInt("servoClosedAngle", climate["servoClosedAngle"]);
        if (!climate["servoOpenAngle"].isNull()) prefs.putInt("servoOpenAngle", climate["servoOpenAngle"]);
    }

    prefs.end();

    resumeCriticalTasks();
    xSemaphoreGive(configMutex);

    Serial.println("  Імпорт завершено");
    return true;
}

// ============================================================================
// FACTORY RESET
// ============================================================================

void factoryResetComplete() {
    Serial.println("\n⚠️⚠️⚠️  ПОВНИЙ FACTORY RESET  ⚠️⚠️⚠️");
    Serial.println("Очищення ВСІХ налаштувань...");

    // Очищуємо всі namespace
    const char* namespaces[] = {
        NS_CLIMATE, NS_WIFI, NS_DATA_LOGGER, NS_SHEETS_SYNC, NS_LEARNING
    };

    Preferences prefs;
    for (const char* ns : namespaces) {
        prefs.begin(ns, false);
        prefs.clear();
        prefs.end();
        Serial.printf("  Очищено namespace: %s\n", ns);
    }

    // Очищуємо SPIFFS
    if (SPIFFS.begin()) {
        if (SPIFFS.exists(BACKUP_JSON_PATH)) {
            SPIFFS.remove(BACKUP_JSON_PATH);
            Serial.println("  Видалено backup.json");
        }
        if (SPIFFS.exists(EXPORT_JSON_PATH)) {
            SPIFFS.remove(EXPORT_JSON_PATH);
            Serial.println("  Видалено export.json");
        }
    }

    Serial.println("\n✅  Factory reset завершено");
    Serial.println("🔄  Перезавантаження...");

    delay(2000);
    ESP.restart();
}

// ============================================================================
// ВАЛІДАЦІЯ ТА ПЕРЕВІРКА
// ============================================================================

bool validateConfigIntegrity() {
    // Перевіряємо чи всі критичні ключі існують
    Preferences prefs;
    prefs.begin(NS_CLIMATE, true);

    bool valid = true;

    // Перевірка температурних налаштувань
    if (!prefs.isKey("tempMin") || !prefs.isKey("tempMax")) {
        Serial.println("  Відсутні температурні налаштування");
        valid = false;
    }

    float tempMin = prefs.getFloat("tempMin", 0);
    float tempMax = prefs.getFloat("tempMax", 0);

    if (tempMin >= tempMax || tempMin < 10 || tempMax > 40) {
        Serial.println("  Некоректні температурні діапазони");
        valid = false;
    }

    prefs.end();

    return valid;
}

uint32_t calculateConfigCRC() {
    String configStr = exportConfigJSON();
    return calculateCRC32(configStr);
}

// ============================================================================
// МІГРАЦІЯ ВЕРСІЙ
// ============================================================================

void checkAndMigrateConfig() {
    Preferences prefs;
    prefs.begin(NS_CLIMATE, true);

    uint8_t configMajor = prefs.getUChar("configMajor", 4);  // Default v4 (попередня версія)
    uint8_t configMinor = prefs.getUChar("configMinor", 8);

    prefs.end();

    if (configMajor < CONFIG_VERSION_MAJOR) {
        Serial.printf("\n=== МІГРАЦІЯ КОНФІГУРАЦІЇ v%d.%d -> v%d.%d ===\n",
                     configMajor, configMinor,
                     CONFIG_VERSION_MAJOR, CONFIG_VERSION_MINOR);

        // Створюємо backup перед міграцією
        createBackup("Before migration to v5.0");

        // Виконуємо міграцію
        migrateConfigVersion(configMajor, configMinor);

        // Оновлюємо версію
        prefs.begin(NS_CLIMATE, false);
        prefs.putUChar("configMajor", CONFIG_VERSION_MAJOR);
        prefs.putUChar("configMinor", CONFIG_VERSION_MINOR);
        prefs.end();

        Serial.println("=== МІГРАЦІЯ ЗАВЕРШЕНА ===\n");
    }
}

void migrateConfigVersion(uint8_t fromMajor, uint8_t fromMinor) {
    Serial.printf("Виконання міграції з v%d.%d...\n", fromMajor, fromMinor);

    Preferences prefs;

    // Міграція з v4.x до v5.0
    if (fromMajor == 4 && CONFIG_VERSION_MAJOR == 5) {
        prefs.begin(NS_CLIMATE, false);

        // Додаємо нові ключі v5.0 якщо їх немає
        if (!prefs.isKey("otaPassword")) {
            prefs.putString("otaPassword", OTA_DEFAULT_PASSWORD);
            Serial.println("  Додано: otaPassword");
        }

        if (!prefs.isKey("tempCriticalLow")) {
            prefs.putFloat("tempCriticalLow", TEMP_CRITICAL_LOW);
            Serial.println("  Додано: tempCriticalLow");
        }

        if (!prefs.isKey("tempEmergencyLow")) {
            prefs.putFloat("tempEmergencyLow", TEMP_EMERGENCY_LOW);
            Serial.println("  Додано: tempEmergencyLow");
        }

        prefs.end();
    }

    Serial.println("  Міграція завершена успішно");
}

// ============================================================================
// OTA ПАРОЛЬ
// ============================================================================

String getOTAPassword() {
    Preferences prefs;
    prefs.begin(NS_CLIMATE, true);
    String password = prefs.getString("otaPassword", OTA_DEFAULT_PASSWORD);
    prefs.end();
    return password;
}

bool setOTAPassword(const String& newPassword) {
    if (newPassword.length() < 4 || newPassword.length() > 32) {
        return false;
    }

    Preferences prefs;
    prefs.begin(NS_CLIMATE, false);
    prefs.putString("otaPassword", newPassword);
    prefs.end();

    Serial.println("OTA пароль оновлено");
    return true;
}

// ============================================================================
// КРИТИЧНІ ОПЕРАЦІЇ
// ============================================================================

void pauseCriticalTasks() {
    // Зупиняємо актуатори для безпеки
    setHeatingPower(0, 0, 0);

    // Призупиняємо FreeRTOS задачі (крім web task)
    suspendAllTasks();

    Serial.println("  Критичні операції призупинено");
}

void resumeCriticalTasks() {
    // Відновлюємо FreeRTOS задачі
    resumeAllTasks();

    Serial.println("  Критичні операції відновлено");
}

// ============================================================================
// ПЕРЕВІРКА ПЕРШОГО BOOT ПІСЛЯ OTA
// ============================================================================

void checkFirstBootAfterOTA() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.println("\n=== ПЕРШИЙ BOOT ПІСЛЯ OTA ===");
            Serial.println("Перевірка критичних систем...");

            bool systemsOK = true;

            // Перевірка датчиків температури
            if (!initTemperatureSensors()) {
                Serial.println("  ❌ Датчики температури: ПОМИЛКА");
                systemsOK = false;
            } else {
                Serial.println("  ✅ Датчики температури: OK");
            }

            // Перевірка BME280
            if (!initBME280()) {
                Serial.println("  ❌ BME280: ПОМИЛКА");
                systemsOK = false;
            } else {
                Serial.println("  ✅ BME280: OK");
            }

            // Перевірка конфігурації
            if (!validateConfigIntegrity()) {
                Serial.println("  ⚠️  Конфігурація: потребує відновлення");
                // Спробуємо відновити з backup
                if (SPIFFS.exists(BACKUP_JSON_PATH)) {
                    Serial.println("  Відновлення з backup...");
                    // Не викликаємо restoreBackup() бо воно зробить restart
                }
            } else {
                Serial.println("  ✅ Конфігурація: OK");
            }

            if (systemsOK) {
                // Підтверджуємо нову версію
                esp_ota_mark_app_valid_cancel_rollback();
                Serial.println("\n✅ Нова прошивка підтверджена!");
                Serial.println("=== ПЕРЕВІРКА ЗАВЕРШЕНА ===\n");
            } else {
                // Критична помилка - rollback
                Serial.println("\n❌ КРИТИЧНА ПОМИЛКА!");
                Serial.println("Система автоматично повернеться на попередню версію...");
                delay(5000);
                ESP.restart();
            }
        }
    }
}
