# Code Conventions - ESP32 Klimat Control

## File Naming

**Implementation:** `.cpp`
**Headers:** `.h` or `.hpp`
**Pattern:** `component_name.*`

Examples:
- sensor_manager.cpp / sensor_manager.h
- web_interface.cpp / web_interface.h

## Variable Naming

### General Rules
- **Clear and descriptive** in Ukrainian or English
- **Consistent language** within component
- **No abbreviations** unless widely known (PWM, PID, GPIO)

### Naming Patterns

**Global constants:**
```cpp
const int PUMP_PWM_CHANNEL = 0;
const float DEFAULT_TARGET_TEMP = 22.0;
const unsigned long SENSOR_READ_INTERVAL = 5000;
```

**Global variables:**
```cpp
extern float targetTemperature;      // Цільова температура
extern bool isSystemActive;          // Чи активна система
extern unsigned long lastReadTime;   // Час останнього читання
```

**Local variables:**
```cpp
float currentTemp = sensors.getTemperature();
int pwmValue = calculatePWM(error);
bool sensorValid = validateReading(value);
```

**Function parameters:**
```cpp
void setPWM(int channel, int dutyCycle);
float calculatePID(float setpoint, float processValue);
bool connectToWiFi(const char* ssid, const char* password);
```

## Function Design

### Single Responsibility
Each function performs ONE clear task:

```cpp
// ❌ BAD - does too much
void updateSystem() {
    readSensors();
    calculateLogic();
    applyActuators();
    updateWeb();
    syncGoogleSheets();
}

// ✅ GOOD - separated concerns
void updateSensorReadings();
void processClimateLogic();
void applyActuatorControl();
void refreshWebInterface();
void syncDataToCloud();
```

### Validation Pattern

Always validate inputs:

```cpp
bool setTargetTemperature(float temp) {
    // Validate range
    if (temp < MIN_TEMP || temp > MAX_TEMP) {
        Serial.printf("Помилка: температура %f поза межами [%f, %f]\n", 
                     temp, MIN_TEMP, MAX_TEMP);
        return false;
    }
    
    // Apply valid value
    targetTemperature = temp;
    saveConfig();
    return true;
}
```

### Error Handling

```cpp
float readTemperature() {
    float temp = bme.readTemperature();
    
    // Check for invalid reading
    if (isnan(temp)) {
        Serial.println("Помилка: датчик BME280 повернув NaN");
        return lastValidTemperature;  // Fallback
    }
    
    // Update last valid value
    lastValidTemperature = temp;
    return temp;
}
```

## Code Comments

### Language: Ukrainian

```cpp
// Ініціалізація PWM каналів для виконавчих механізмів
void initPWMChannels() {
    // Налаштування каналу насоса (8 біт, 5 кГц)
    ledcSetup(PUMP_PWM_CHANNEL, 5000, 8);
    ledcAttachPin(PUMP_PIN, PUMP_PWM_CHANNEL);
    
    // Налаштування каналу вентилятора
    ledcSetup(FAN_PWM_CHANNEL, 5000, 8);
    ledcAttachPin(FAN_PIN, FAN_PWM_CHANNEL);
}
```

### When to Comment

**Always comment:**
- Complex algorithms
- Non-obvious logic
- Magic numbers (after converting to constants)
- Hardware-specific workarounds
- Safety-critical sections

**Never comment:**
- Obvious code
- Redundant explanations

```cpp
// ❌ BAD - obvious
int sum = a + b;  // Додаємо a та b

// ✅ GOOD - explains WHY
// Використовуємо середнє з 5 вимірів для фільтрації шуму DHT22
float avgTemp = calculateAverage(tempReadings, 5);
```

### Function Documentation

```cpp
/**
 * Розраховує вихід PID регулятора
 * 
 * @param setpoint Цільове значення температури (°C)
 * @param processValue Поточне значення температури (°C)
 * @param dt Час з останнього виклику (мс)
 * @return Значення керування [0-255] для PWM
 */
int calculatePID(float setpoint, float processValue, unsigned long dt);
```

## Code Organization

### File Structure

```cpp
// 1. Includes
#include <Arduino.h>
#include "config.h"
#include "global_declarations.h"

// 2. Local constants
const int BUFFER_SIZE = 64;

// 3. Local variables (static if possible)
static float calibrationOffset = 0.0;

// 4. Forward declarations (if needed)
void helperFunction();

// 5. Public functions
void publicAPI() {
    // ...
}

// 6. Private/helper functions
static void helperFunction() {
    // ...
}
```

### Header Guards

```cpp
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

// Header content

#endif // SENSOR_MANAGER_H
```

## Common Patterns

### Initialization Pattern

```cpp
bool SensorManager::init() {
    Serial.println("Ініціалізація менеджера сенсорів...");
    
    // Initialize I2C
    if (!Wire.begin(SDA_PIN, SCL_PIN)) {
        Serial.println("Помилка: не вдалося ініціалізувати I2C");
        return false;
    }
    
    // Initialize BME280
    if (!bme.begin(BME280_ADDRESS)) {
        Serial.println("Помилка: BME280 не знайдено");
        return false;
    }
    
    Serial.println("Сенсори ініціалізовано успішно");
    return true;
}
```

### Manager Pattern

```cpp
class SensorManager {
public:
    void init();           // Ініціалізація
    void update();         // Оновлення даних
    float getValue();      // Отримання значення
    bool isValid();        // Перевірка валідності
    
private:
    float lastValue;
    bool valid;
    void readSensor();
    bool validateReading(float value);
};
```

### State Machine Pattern

```cpp
enum SystemMode {
    MODE_AUTO,
    MODE_MANUAL,
    MODE_FORCE,
    MODE_EMERGENCY
};

void handleMode(SystemMode mode) {
    switch (mode) {
        case MODE_AUTO:
            runAutoLogic();
            break;
        case MODE_MANUAL:
            applyManualSettings();
            break;
        case MODE_FORCE:
            forceHeating();
            break;
        case MODE_EMERGENCY:
            emergencyProtection();
            break;
    }
}
```

## Code Quality Rules

### DO:
- Use const for values that don't change
- Prefer static for file-local functions/variables
- Check return values from functions
- Free allocated memory
- Use millis() instead of delay()
- Validate all user inputs

### DON'T:
- Use delay() in critical sections
- Ignore compiler warnings
- Leave dead/commented code
- Use magic numbers (create constants)
- Block WiFi/web server tasks
- Access globals without protection in ISRs

## Performance Considerations

### Memory
```cpp
// ❌ BAD - wastes RAM
String message = "Temperature: " + String(temp) + "°C";

// ✅ GOOD - uses less RAM
char message[32];
snprintf(message, sizeof(message), "Temperature: %.1f°C", temp);
```

### Timing
```cpp
// ❌ BAD - blocking
delay(1000);

// ✅ GOOD - non-blocking
static unsigned long lastTime = 0;
if (millis() - lastTime >= 1000) {
    lastTime = millis();
    doPeriodicTask();
}
```

## Example: Complete Function

```cpp
/**
 * Оновлює стан насоса на основі розрахованого PWM значення
 * Включає плавне нарощування та обмеження потужності
 * 
 * @param targetPWM Цільове значення PWM [0-255]
 */
void updatePump(int targetPWM) {
    // Валідація входу
    targetPWM = constrain(targetPWM, 0, 255);
    
    // Обмеження максимальної потужності
    if (targetPWM > MAX_PUMP_PWM) {
        targetPWM = MAX_PUMP_PWM;
        Serial.printf("Обмеження: PWM насоса скорочено до %d\n", MAX_PUMP_PWM);
    }
    
    // Плавне нарощування (захист від стрибків струму)
    static int currentPWM = 0;
    const int RAMP_STEP = 5;
    
    if (abs(targetPWM - currentPWM) > RAMP_STEP) {
        currentPWM += (targetPWM > currentPWM) ? RAMP_STEP : -RAMP_STEP;
    } else {
        currentPWM = targetPWM;
    }
    
    // Застосування до апаратури
    ledcWrite(PUMP_PWM_CHANNEL, currentPWM);
    
    // Логування значних змін
    static int lastLoggedPWM = -1;
    if (abs(currentPWM - lastLoggedPWM) >= 20) {
        Serial.printf("Насос: PWM = %d (ціль: %d)\n", currentPWM, targetPWM);
        lastLoggedPWM = currentPWM;
    }
}
```

This example demonstrates:
- Clear documentation
- Input validation
- Safety limits
- Smooth ramping
- Appropriate logging
- Ukrainian comments
- Descriptive variable names
