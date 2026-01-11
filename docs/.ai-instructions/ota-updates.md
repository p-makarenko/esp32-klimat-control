# OTA Updates - Implementation Guide

## Critical Feature

OTA (Over-The-Air) updates are **REQUIRED** for production-ready systems.
Users must be able to update firmware **without physical access** to the device.

## Why OTA is Essential

1. **Transferability** - Devices can be sold/deployed remotely
2. **Support** - Fix bugs without site visits
3. **Features** - Deploy new capabilities to existing installations
4. **Security** - Patch vulnerabilities quickly
5. **Cost** - Eliminate travel/downtime for updates

---

## Update Methods (All Required)

### Method 1: Web Interface Upload

**User flow:**
1. Download new firmware.bin from developer
2. Navigate to Settings → Firmware Update
3. Click "Choose File" → select firmware.bin
4. Click "Upload & Install"
5. Progress bar shows upload (0-100%)
6. System verifies and flashes firmware
7. Automatic reboot
8. Success/failure notification

**Implementation:**
```cpp
// In web_interface.cpp
server.on("/update", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
        // Handler after upload completes
        AsyncWebServerResponse *response = request->beginResponse(
            Update.hasError() ? 500 : 200, 
            "text/plain", 
            Update.hasError() ? "FAILED" : "OK"
        );
        response->addHeader("Connection", "close");
        request->send(response);
        ESP.restart();
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, 
       uint8_t *data, size_t len, bool final) {
        // Upload handler
        if (!index) {
            Serial.printf("Початок OTA: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        }
        
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
        
        if (final) {
            if (Update.end(true)) {
                Serial.printf("OTA завершено: %u байт\n", index + len);
            } else {
                Update.printError(Serial);
            }
        }
    }
);
```

**UI (HTML/JS):**
```html
<form method='POST' action='/update' enctype='multipart/form-data' id='updateForm'>
    <input type='file' name='update' accept='.bin' required>
    <button type='submit'>Завантажити оновлення</button>
</form>
<div id='progress' style='display:none'>
    <progress id='progressBar' value='0' max='100'></progress>
    <span id='progressText'>0%</span>
</div>

<script>
document.getElementById('updateForm').onsubmit = async (e) => {
    e.preventDefault();
    const formData = new FormData(e.target);
    const xhr = new XMLHttpRequest();
    
    xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
            const percent = (e.loaded / e.total) * 100;
            document.getElementById('progressBar').value = percent;
            document.getElementById('progressText').innerText = Math.round(percent) + '%';
        }
    };
    
    xhr.onload = () => {
        if (xhr.status === 200) {
            alert('Оновлення успішне! Система перезавантажується...');
        } else {
            alert('Помилка оновлення: ' + xhr.responseText);
        }
    };
    
    document.getElementById('progress').style.display = 'block';
    xhr.open('POST', '/update');
    xhr.send(formData);
};
</script>
```

---

### Method 2: Remote URL Update

**User flow:**
1. Developer publishes firmware.bin at public URL
2. User navigates to Settings → Remote Update
3. Enter URL: https://example.com/klimat_v4.7.bin
4. Click "Download & Install"
5. System downloads firmware
6. Verifies checksum (if provided)
7. Flashes and reboots

**Implementation:**
```cpp
#include <HTTPClient.h>

bool updateFromURL(const char* url) {
    HTTPClient http;
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("Помилка завантаження: %d\n", httpCode);
        return false;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("Невідомий розмір файлу");
        return false;
    }
    
    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        Serial.println("Недостатньо місця для OTA");
        return false;
    }
    
    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    
    if (written != contentLength) {
        Serial.printf("Записано лише %d з %d байт\n", written, contentLength);
        return false;
    }
    
    if (!Update.end()) {
        Serial.println("Помилка завершення OTA");
        return false;
    }
    
    if (!Update.isFinished()) {
        Serial.println("OTA не завершено");
        return false;
    }
    
    Serial.println("OTA успішно завершено");
    http.end();
    return true;
}

// In web API
server.on("/api/update/url", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("url", true)) {
        request->send(400, "application/json", "{\"error\":\"URL required\"}");
        return;
    }
    
    String url = request->getParam("url", true)->value();
    request->send(200, "application/json", "{\"status\":\"downloading\"}");
    
    // Run update in separate task to allow response to complete
    xTaskCreate([](void* param) {
        const char* url = (const char*)param;
        if (updateFromURL(url)) {
            delay(1000);
            ESP.restart();
        }
        free((void*)url);
        vTaskDelete(NULL);
    }, "OTA_Task", 8192, strdup(url.c_str()), 1, NULL);
});
```

---

### Method 3: Auto-Update Check (Optional)

**Requirements:**
- User consent required (opt-in)
- Never force updates
- Show changelog before updating
- Configurable check interval

**Implementation:**
```cpp
struct UpdateInfo {
    String version;
    String url;
    String changelog;
    bool critical;  // Security patch, etc.
};

UpdateInfo checkForUpdates() {
    HTTPClient http;
    http.begin("https://api.example.com/klimat/latest");
    
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        // Parse JSON response
        // Return update info
    }
    
    return {"", "", "", false};  // No update
}

// In main loop (if enabled)
static unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 24 * 60 * 60 * 1000;  // 24h

void loop() {
    if (config.autoUpdateCheck && millis() - lastCheck > CHECK_INTERVAL) {
        UpdateInfo info = checkForUpdates();
        if (info.version != CURRENT_VERSION) {
            notifyUpdateAvailable(info);
        }
        lastCheck = millis();
    }
    // rest of loop...
}
```

---

## Safety Features

### 1. Firmware Verification

**MD5 Checksum:**
```cpp
// Generate MD5 for your firmware
// Linux: md5sum firmware.bin
// Include in JSON metadata

bool verifyFirmware(const char* expectedMD5) {
    String calculatedMD5 = Update.md5String();
    return calculatedMD5.equals(expectedMD5);
}
```

**Usage:**
```cpp
if (Update.end(true)) {
    if (verifyFirmware(providedMD5)) {
        Serial.println("Firmware verified ✓");
        ESP.restart();
    } else {
        Serial.println("MD5 mismatch! Update aborted.");
        Update.abort();
    }
}
```

### 2. Configuration Backup

```cpp
void backupConfig() {
    // Save current config to backup namespace
    Preferences backup;
    backup.begin("config_backup", false);
    
    // Copy all current settings
    backup.putFloat("targetTemp", config.targetTemp);
    backup.putFloat("minTemp", config.minTemp);
    // ... all settings
    
    backup.end();
    Serial.println("Конфігурація збережена");
}

void restoreConfig() {
    Preferences backup;
    backup.begin("config_backup", true);
    
    if (backup.isKey("targetTemp")) {
        config.targetTemp = backup.getFloat("targetTemp");
        // ... restore all settings
        Serial.println("Конфігурація відновлена");
    }
    
    backup.end();
}
```

**Call before update:**
```cpp
server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    backupConfig();  // Always backup before OTA
    // ... proceed with update
});
```

### 3. Rollback on Failure

ESP32 supports dual partitions (OTA_0, OTA_1):

```cpp
#include <esp_ota_ops.h>

void checkFirstBoot() {
    static bool firstBoot = true;
    if (!firstBoot) return;
    
    firstBoot = false;
    
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    
    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            // New firmware booted successfully
            esp_ota_mark_app_valid_cancel_rollback();
            Serial.println("Нова версія підтверджена ✓");
        }
    }
}

void setup() {
    Serial.begin(115200);
    checkFirstBoot();
    
    // If system crashes during init, watchdog triggers rollback
    // ... rest of setup
}
```

**platformio.ini configuration:**
```ini
[env:esp32s3]
board_build.partitions = min_spiffs.csv  # or custom partition table

# Custom partition example (partitions.csv):
# nvs,      data, nvs,     0x9000,  0x5000,
# otadata,  data, ota,     0xe000,  0x2000,
# app0,     app,  ota_0,   0x10000, 0x1E0000,
# app1,     app,  ota_1,   0x1F0000,0x1E0000,
# spiffs,   data, spiffs,  0x3D0000,0x30000,
```

### 4. Power Loss Protection

**Warning before update:**
```javascript
// In web UI
function confirmUpdate() {
    return confirm(
        "⚠️ ВАЖЛИВО:\n\n" +
        "1. Переконайтеся, що ESP32 підключений до стабільного живлення\n" +
        "2. НЕ вимикайте живлення під час оновлення\n" +
        "3. Процес займе 1-2 хвилини\n" +
        "4. Система автоматично перезавантажиться\n\n" +
        "Продовжити оновлення?"
    );
}
```

**Delay if power unstable:**
```cpp
float getBatteryVoltage() {
    // Read from ADC if battery-powered
    return analogRead(BATTERY_PIN) * (3.3 / 4095.0) * 2.0;
}

bool isPowerStable() {
    if (isBatteryPowered) {
        float voltage = getBatteryVoltage();
        return voltage > MIN_SAFE_VOLTAGE;
    }
    return true;  // Assume stable if mains-powered
}

server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isPowerStable()) {
        request->send(503, "text/plain", 
                     "Низький заряд батареї. Підключіть живлення.");
        return;
    }
    // ... proceed
});
```

### 5. LED/Status Indicators

```cpp
void otaProgress(size_t progress, size_t total) {
    static int lastPercent = -1;
    int percent = (progress * 100) / total;
    
    if (percent != lastPercent) {
        lastPercent = percent;
        
        // Blink LED to show progress
        digitalWrite(LED_PIN, percent % 2);
        
        // Update web UI via WebSocket (if implemented)
        // ws.textAll(String(percent));
        
        Serial.printf("OTA Progress: %d%%\n", percent);
    }
}

void setupOTA() {
    Update.onProgress(otaProgress);
}
```

---

## User Experience

### Update History Log

```cpp
struct UpdateRecord {
    String version;
    String timestamp;
    bool success;
    String notes;
};

void logUpdate(String version, bool success, String notes = "") {
    Preferences prefs;
    prefs.begin("update_log", false);
    
    int count = prefs.getInt("count", 0);
    
    String key = "update_" + String(count);
    String record = version + "|" + 
                   String(millis()) + "|" + 
                   (success ? "OK" : "FAIL") + "|" + 
                   notes;
    
    prefs.putString(key.c_str(), record);
    prefs.putInt("count", count + 1);
    prefs.end();
}

// API endpoint to retrieve history
server.on("/api/update/history", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Read and format update history
    // Return JSON array
});
```

### Changelog Display

**JSON format:**
```json
{
  "version": "4.7-D45",
  "url": "https://example.com/firmware_v4.7.bin",
  "md5": "a1b2c3d4...",
  "changelog": [
    "✨ Додано: Автоматичне налаштування PID",
    "🐛 Виправлено: Збій WiFi після 24h роботи",
    "⚡ Покращено: Швидкість читання сенсорів +20%",
    "🔒 Безпека: Patch для CVE-2024-1234"
  ],
  "critical": false,
  "minVersion": "4.0"
}
```

**Display in UI:**
```html
<div class="changelog">
    <h3>Доступна нова версія: 4.7-D45</h3>
    <h4>Зміни:</h4>
    <ul>
        <li>✨ Додано: Автоматичне налаштування PID</li>
        <li>🐛 Виправлено: Збій WiFi після 24h роботи</li>
        <!-- ... -->
    </ul>
    <button onclick="startUpdate()">Оновити зараз</button>
    <button onclick="remindLater()">Нагадати пізніше</button>
</div>
```

---

## Security

### 1. Password Protection

```cpp
const char* OTA_USERNAME = "admin";
const char* OTA_PASSWORD = "secure_password";  // From config

server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(OTA_USERNAME, OTA_PASSWORD)) {
        return request->requestAuthentication();
    }
    // ... proceed with update
});
```

### 2. HTTPS for Remote Updates

```cpp
// Use WiFiClientSecure for HTTPS
#include <WiFiClientSecure.h>

WiFiClientSecure secureClient;
secureClient.setCACert(root_ca);  // SSL certificate

HTTPClient https;
https.begin(secureClient, url);
```

### 3. Firmware Signing (Advanced)

```cpp
// Requires secure boot enabled in ESP32
// Firmware must be signed with private key
// ESP32 verifies signature before flashing
// See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html
```

### 4. Rate Limiting

```cpp
// Prevent brute-force attacks
static unsigned long lastUpdateAttempt = 0;
const unsigned long UPDATE_COOLDOWN = 60000;  // 1 minute

server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (millis() - lastUpdateAttempt < UPDATE_COOLDOWN) {
        request->send(429, "text/plain", 
                     "Забагато спроб. Почекайте 1 хвилину.");
        return;
    }
    lastUpdateAttempt = millis();
    // ... proceed
});
```

---

## Testing Checklist

Before releasing OTA feature:

- [ ] Test web upload with valid firmware
- [ ] Test web upload with invalid file (should reject)
- [ ] Test remote URL update with good WiFi
- [ ] Test remote URL update with poor WiFi (timeout handling)
- [ ] Verify config preservation across update
- [ ] Test rollback on intentionally broken firmware
- [ ] Test power loss during update (if possible)
- [ ] Verify MD5 checksum validation
- [ ] Test update history logging
- [ ] Test password protection
- [ ] Verify LED indicators during update
- [ ] Test with multiple consecutive updates
- [ ] Verify partition table configuration
- [ ] Test "check for updates" button
- [ ] Mobile-friendly UI testing

---

## Configuration Migration

**Handling version changes:**

```cpp
void migrateConfig() {
    Preferences prefs;
    prefs.begin("klimat_config", false);
    
    String currentVersion = prefs.getString("config_version", "0.0");
    
    if (currentVersion == "0.0") {
        // Migrate from v1 to v2 format
        // Example: rename keys, convert units, add defaults
        float oldTemp = prefs.getFloat("temp", 22.0);
        prefs.putFloat("targetTemp", oldTemp);  // New key name
        prefs.putString("config_version", "2.0");
        Serial.println("Config migrated: v1 → v2");
    }
    
    if (currentVersion == "2.0" && FIRMWARE_VERSION == "3.0") {
        // Migrate v2 to v3
        // ...
        prefs.putString("config_version", "3.0");
    }
    
    prefs.end();
}

void setup() {
    Serial.begin(115200);
    migrateConfig();  // Always run on boot
    // ...
}
```

---

## Documentation for Users

**README section:**

```markdown
## Оновлення прошивки

### Метод 1: Через веб-інтерфейс

1. Завантажте новий файл firmware.bin
2. Відкрийте http://klimat.local
3. Перейдіть: Налаштування → Оновлення
4. Оберіть файл та натисніть "Завантажити"
5. Дочекайтеся завершення (1-2 хв)
6. Система перезавантажиться автоматично

### Метод 2: Віддалене оновлення

1. Введіть URL прошивки
2. Натисніть "Завантажити з інтернету"
3. Дочекайтеся завершення

### ⚠️ Важливо

- НЕ вимикайте живлення під час оновлення
- Переконайтеся в стабільному WiFi з'єднанні
- Збережіть резервну копію налаштувань (експорт)

### Відновлення після невдалого оновлення

Якщо система не завантажується:
1. Вона автоматично повернеться до попередньої версії
2. Якщо ні - перепрошийте через USB (див. інструкцію)
```

This comprehensive OTA implementation ensures reliable, secure, and user-friendly firmware updates for production deployments.
