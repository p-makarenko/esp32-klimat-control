# Production-Ready & Transferability

## Philosophy
System designed for transfer, sale, or sharing.
ALL features configurable without code changes.
Design for end-users, not developers.

## Configuration Requirements

### What MUST Be Configurable

#### Temperature Control
- All thresholds: target, min, max, emergency
- Hysteresis values
- Averaging periods
- BME280 offset correction

#### Timing
- Sensor read intervals
- Data logging frequency
- Google Sheets sync intervals
- Emergency detection timeouts
- Auto-recovery delays

#### Actuator Behavior
- PWM frequency and resolution
- Min/max power limits (pump, fan, extractor)
- Ramp-up/down rates
- Manual override durations

#### Data & Logging
- Data retention periods
- Sync destinations (enable/disable)
- Google Apps Script URL
- Log verbosity levels

#### Network
- Multiple WiFi credentials (SSID + password)
- Static IP vs DHCP
- mDNS hostname
- Web interface timeout

#### Hardware (if possible)
- GPIO pin assignments
- Sensor types and addresses
- Actuator characteristics

### Configuration Storage

Use Preferences (NVS), NOT EEPROM.

```cpp
#include <Preferences.h>

Preferences prefs;

void saveTemperatureConfig() {
    prefs.begin("klimat", false);
    prefs.putFloat("targetTemp", targetTemp);
    prefs.putFloat("minTemp", minTemp);
    prefs.putFloat("maxTemp", maxTemp);
    prefs.putFloat("emergencyTemp", emergencyTemp);
    prefs.end();
}

void loadTemperatureConfig() {
    prefs.begin("klimat", true);
    targetTemp = prefs.getFloat("targetTemp", 22.0);
    minTemp = prefs.getFloat("minTemp", 18.0);
    maxTemp = prefs.getFloat("maxTemp", 28.0);
    emergencyTemp = prefs.getFloat("emergencyTemp", 15.0);
    prefs.end();
}
```

### Default Values

Provide sensible defaults for all settings.

```cpp
// Температурні налаштування за замовчуванням
const float DEFAULT_TARGET_TEMP = 22.0;
const float DEFAULT_MIN_TEMP = 18.0;
const float DEFAULT_MAX_TEMP = 28.0;
const float DEFAULT_HYSTERESIS = 0.5;

// Інтервали за замовчуванням
const unsigned long DEFAULT_SENSOR_INTERVAL = 5000;  // 5 сек
const unsigned long DEFAULT_SYNC_INTERVAL = 300000;  // 5 хв
```

## First-Time Setup Wizard

Web-based wizard for initial configuration:

1. Welcome screen
2. WiFi configuration (multiple networks)
3. GPIO pin verification
4. Sensor calibration
5. Basic thresholds
6. Language selection (Ukrainian default)
7. Completion & system start

Implementation:
```cpp
bool isFirstBoot() {
    prefs.begin("klimat", true);
    bool configured = prefs.getBool("configured", false);
    prefs.end();
    return !configured;
}

void markAsConfigured() {
    prefs.begin("klimat", false);
    prefs.putBool("configured", true);
    prefs.end();
}
```

## Safety & Validation

### Input Validation
```cpp
bool setTargetTemperature(float temp) {
    // Перевірка діапазону
    if (temp < 15.0 || temp > 30.0) {
        Serial.println("ПОМИЛКА: температура поза безпечним діапазоном");
        return false;
    }
    
    // Попередження про нестандартні значення
    if (temp < 18.0 || temp > 26.0) {
        Serial.println("УВАГА: нестандартне значення температури");
    }
    
    targetTemperature = temp;
    saveConfig();
    return true;
}
```

### Dangerous Configuration Prevention
```cpp
// Запобігання небезпечним налаштуванням
if (emergencyTemp >= minTemp) {
    Serial.println("ПОМИЛКА: аварійна температура повинна бути нижче мінімальної");
    emergencyTemp = minTemp - 3.0;
}
```

## OTA Updates

### Implementation Requirements

```cpp
#include <ArduinoOTA.h>

void setupOTA() {
    // Налаштування hostname
    ArduinoOTA.setHostname("klimat");
    
    // Пароль для захисту
    ArduinoOTA.setPassword("secure_password"); // З конфігурації!
    
    // Callbacks для статусу
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) 
            ? "firmware" : "filesystem";
        Serial.println("Початок OTA: " + type);
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA завершено");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Прогрес: %u%%\r", (progress * 100) / total);
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Помилка[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Помилка автентифікації");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Помилка початку");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Помилка підключення");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Помилка отримання");
        else if (error == OTA_END_ERROR) Serial.println("Помилка завершення");
    });
    
    ArduinoOTA.begin();
}

void loop() {
    ArduinoOTA.handle();
    // ... інший код
}
```

### Web Interface Upload

Add endpoint for firmware upload:

```cpp
server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    // Response after update
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", 
        Update.hasError() ? "ПОМИЛКА" : "ОК"
    );
    response->addHeader("Connection", "close");
    request->send(response);
}, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    // Upload handler
    if (!index) {
        Serial.printf("Початок завантаження: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) {
            Serial.println("Оновлення успішне, перезавантаження...");
        } else {
            Update.printError(Serial);
        }
    }
});
```

### Configuration Backup Before Update

```cpp
void backupConfigBeforeOTA() {
    prefs.begin("backup", false);
    
    // Копіюємо всі налаштування
    prefs.putFloat("targetTemp", targetTemperature);
    prefs.putFloat("minTemp", minTemperature);
    // ... інші налаштування
    
    prefs.putBool("hasBackup", true);
    prefs.end();
}

void restoreConfigAfterOTA() {
    prefs.begin("backup", true);
    
    if (prefs.getBool("hasBackup", false)) {
        targetTemperature = prefs.getFloat("targetTemp", 22.0);
        minTemperature = prefs.getFloat("minTemp", 18.0);
        // ... інші налаштування
        
        Serial.println("Конфігурацію відновлено з backup");
    }
    
    prefs.end();
}
```

## Export/Import Settings

JSON format for configuration:

```cpp
String exportConfig() {
    String json = "{";
    json += "\"version\":\"4.6\",";
    json += "\"targetTemp\":" + String(targetTemp) + ",";
    json += "\"minTemp\":" + String(minTemp) + ",";
    json += "\"maxTemp\":" + String(maxTemp) + ",";
    // ... всі налаштування
    json += "}";
    return json;
}

bool importConfig(String json) {
    // Parse JSON
    // Validate values
    // Apply settings
    // Save to NVS
}
```

## Factory Reset

```cpp
void factoryReset() {
    Serial.println("Скидання до заводських налаштувань...");
    
    // Очистка всіх namespace
    prefs.begin("klimat", false);
    prefs.clear();
    prefs.end();
    
    prefs.begin("backup", false);
    prefs.clear();
    prefs.end();
    
    Serial.println("Налаштування скинуто. Перезавантаження...");
    delay(1000);
    ESP.restart();
}
```

## User Documentation

Create README_USER.md with:
- Installation guide with photos
- First-time setup walkthrough
- Web interface overview
- Common tasks (changing temperature, adding WiFi, etc.)
- Troubleshooting section
- FAQ

## Testing Checklist Before Release

- [ ] Fresh ESP32 installation works
- [ ] Setup wizard completes successfully
- [ ] Non-technical user can configure system
- [ ] All settings changeable via web
- [ ] OTA update works
- [ ] Configuration persists after reboot
- [ ] Factory reset works
- [ ] Export/import settings works
- [ ] Documentation is complete
- [ ] Recovery from misconfiguration possible

## Versioning

Display prominently in web interface:

```cpp
const char* FIRMWARE_VERSION = "v4.6-D32";
const char* BUILD_DATE = __DATE__;
const char* BUILD_TIME = __TIME__;

// В веб-інтерфейсі
String getVersionInfo() {
    return String(FIRMWARE_VERSION) + " (" + BUILD_DATE + " " + BUILD_TIME + ")";
}
```
