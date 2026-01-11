# Anti-Patterns to Avoid

## Hardcoding

### ❌ Never Hardcode:

**WiFi Credentials**
```cpp
// ❌ Bad
const char* ssid = "MyNetwork";
const char* password = "MyPassword123";

// ✅ Good
String ssid = loadFromPreferences("wifi_ssid");
String password = loadFromPreferences("wifi_pass");
```

**Temperature Thresholds**
```cpp
// ❌ Bad
if (temp > 25.0) { activateCooling(); }

// ✅ Good
if (temp > userConfiguredMaxTemp) { activateCooling(); }
```

**GPIO Pins (unless hardware-specific)**
```cpp
// ❌ Bad
#define PUMP_PIN 23

// ✅ Good (if changeable)
int pumpPin = prefs.getInt("pump_pin", 23);
```

**URLs & API Keys**
```cpp
// ❌ Bad
const char* serverUrl = "https://myserver.com/api";

// ✅ Good
String serverUrl = prefs.getString("server_url", DEFAULT_URL);
```

---

## Assumptions

### ❌ Never Assume:

**User Technical Knowledge**
```cpp
// ❌ Bad
Serial.println("ERR_SENSOR_I2C_NACK");

// ✅ Good
Serial.println("Помилка: датчик не відповідає. Перевірте підключення.");
showWebError("Датчик BME280 не знайдено. Перевірте проводи.");
```

**User Can Read Code**
```cpp
// ❌ Bad (no explanation)
bool result = initSensor();

// ✅ Good (web interface)
if (!initSensor()) {
    webInterface.showError(
        "Датчик не ініціалізовано",
        "Можливі причини: неправильне підключення, пошкоджений датчик",
        "Перевірте проводи та перезавантажте систему"
    );
}
```

**Hardware Configuration**
```cpp
// ❌ Bad
bme.begin(0x76); // Assumes address

// ✅ Good
if (!bme.begin(0x76)) {
    if (!bme.begin(0x77)) {
        Serial.println("BME280 не знайдено за адресами 0x76 та 0x77");
    }
}
```

**Timezone or Language**
```cpp
// ❌ Bad
Serial.println("System started at 14:30");

// ✅ Good
Serial.println("Систему запущено о " + getLocalTime());
// + налаштування timezone в конфігурації
```

---

## Over-Engineering

### ❌ Avoid:

**Features "Just in Case"**
```cpp
// ❌ Bad - надто складно для простої системи
class AbstractSensorFactory {
    virtual ISensor* createSensor() = 0;
};

// ✅ Good - KISS principle
struct Sensor {
    String name;
    float (*readFunc)();
};
```

**Complex Abstractions**
```cpp
// ❌ Bad - заплутано для користувача
config.setParameter(PARAM_THERMAL_CONTROL_UPPER_BOUND, 25.0);

// ✅ Good - зрозуміло
config.maxTemperature = 25.0;
```

**Premature Optimization**
```cpp
// ❌ Bad - оптимізація без вимірювань
uint8_t temp_x10 = temperature * 10; // save memory!

// ✅ Good - зрозумілість важливіша
float temperature = readSensor();
```

---

## Poor UX

### ❌ Never:

**Cryptic Error Codes**
```cpp
// ❌ Bad
Serial.println("ERR_0x42");

// ✅ Good
Serial.println("Помилка 0x42: Датчик температури не відповідає");
Serial.println("Рішення: Перевірте I2C підключення (SDA/SCL)");
```

**Require Serial for Normal Operation**
```cpp
// ❌ Bad - система не працює без Serial Monitor
void loop() {
    Serial.println("Enter command:");
    String cmd = Serial.readString();
    processCommand(cmd);
}

// ✅ Good - Serial тільки для debug
void loop() {
    // Нормальна робота через web interface
    processWebRequests();
    
    // Debug info якщо Serial доступний
    if (Serial) {
        logDebugInfo();
    }
}
```

**Hidden Critical Settings**
```cpp
// ❌ Bad - налаштування в коді
#define EMERGENCY_TEMP 15.0

// ✅ Good - в web interface
<input type="number" id="emergencyTemp" 
       label="Аварійна температура (°C)"
       tooltip="Система увімкне обігрів на максимум"/>
```

**Lose User Data Without Confirmation**
```cpp
// ❌ Bad
void factoryReset() {
    prefs.clear();
    ESP.restart();
}

// ✅ Good
void factoryReset() {
    webInterface.showConfirmDialog(
        "Увага!",
        "Це видалить ВСІ налаштування. Продовжити?",
        onConfirm: []() {
            createBackup();
            prefs.clear();
            ESP.restart();
        }
    );
}
```

---

## Code Smells

### ❌ Avoid:

**Magic Numbers**
```cpp
// ❌ Bad
if (analogRead(34) > 2048) { ... }

// ✅ Good
const int SOIL_MOISTURE_PIN = 34;
const int MOISTURE_THRESHOLD = 2048;
if (analogRead(SOIL_MOISTURE_PIN) > MOISTURE_THRESHOLD) { ... }
```

**Deep Nesting**
```cpp
// ❌ Bad
void updateSystem() {
    if (wifiConnected) {
        if (sensorsReady) {
            if (temperature > minTemp) {
                if (humidity < maxHumidity) {
                    // actual logic buried here
                }
            }
        }
    }
}

// ✅ Good - early returns
void updateSystem() {
    if (!wifiConnected) return;
    if (!sensorsReady) return;
    if (temperature <= minTemp) return;
    if (humidity >= maxHumidity) return;
    
    // actual logic clearly visible
}
```

**God Objects**
```cpp
// ❌ Bad
class SystemManager {
    void readSensors();
    void controlActuators();
    void serveWeb();
    void syncGoogleSheets();
    void handleOTA();
    void manageLogs();
    // ... 50+ methods
}

// ✅ Good - separation of concerns
class SensorManager { ... }
class ActuatorManager { ... }
class WebServer { ... }
class DataSync { ... }
```

**Copy-Paste Code**
```cpp
// ❌ Bad
void updatePump() {
    int value = calculatePumpPower();
    ledcWrite(PUMP_CHANNEL, value);
    Serial.printf("Pump: %d\n", value);
}

void updateFan() {
    int value = calculateFanPower();
    ledcWrite(FAN_CHANNEL, value);
    Serial.printf("Fan: %d\n", value);
}

// ✅ Good
void updateActuator(int channel, int value, const char* name) {
    ledcWrite(channel, value);
    Serial.printf("%s: %d\n", name, value);
}
```

---

## Memory Issues

### ❌ Avoid:

**String Concatenation in Loops**
```cpp
// ❌ Bad - фрагментація heap
String buildJSON() {
    String json = "{";
    for (int i = 0; i < 100; i++) {
        json += "\"sensor" + String(i) + "\":" + String(values[i]) + ",";
    }
    return json;
}

// ✅ Good - pre-allocate
String buildJSON() {
    String json;
    json.reserve(500);  // Оцінюємо розмір
    json = "{";
    // ...
}
```

**Unbounded Buffers**
```cpp
// ❌ Bad
String logBuffer;
void log(String msg) {
    logBuffer += msg + "\n";  // Росте безмежно
}

// ✅ Good
#define MAX_LOG_LINES 100
CircularBuffer<String, MAX_LOG_LINES> logBuffer;
```

---

## Performance

### ❌ Avoid:

**Blocking Operations in Loop**
```cpp
// ❌ Bad
void loop() {
    delay(1000);  // Блокує все
    readSensors();
}

// ✅ Good
void loop() {
    static unsigned long lastRead = 0;
    if (millis() - lastRead >= 1000) {
        lastRead = millis();
        readSensors();
    }
}
```

**Synchronous Network Calls**
```cpp
// ❌ Bad
void syncData() {
    HTTPClient http;
    http.begin(serverUrl);
    http.POST(data);  // Блокує до завершення
}

// ✅ Good - async
void syncData() {
    AsyncHTTPClient http;
    http.onComplete(handleResponse);
    http.POST(data);
}
```

---

## Security

### ❌ Never:

**Expose Credentials in Web Interface**
```cpp
// ❌ Bad
server.on("/api/config", [](){ 
    String json = "{\"wifi_password\":\"" + wifiPassword + "\"}";
    request->send(200, "application/json", json);
});

// ✅ Good
server.on("/api/config", [](){ 
    String json = "{\"wifi_configured\":" + String(wifiConfigured) + "}";
    // Пароль НЕ передаємо
});
```

**SQL Injection Equivalent (JSON)**
```cpp
// ❌ Bad
String query = "{\"temperature\":" + userInput + "}";

// ✅ Good - validate first
float temp = userInput.toFloat();
if (temp >= MIN_TEMP && temp <= MAX_TEMP) {
    String query = "{\"temperature\":" + String(temp) + "}";
}
```

---

## Summary

**Remember:**
- User experience > Technical elegance
- Simple & working > Complex & perfect
- Clear errors > Silent failures
- Configuration > Hardcoding
- Documentation > Clever code

**Golden Rule:**  
If non-technical user can't use it → it's not production-ready.
