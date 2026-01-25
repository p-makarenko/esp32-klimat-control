# Architecture Details - ESP32 Klimat Control

## Component Structure

### Core System (system_core.*)
- Main loop() orchestration
- Mode management: AUTO, MANUAL, FORCE, EMERGENCY
- Watchdog timer integration
- Multi-WiFi with auto-reconnect
- System state machine

### Sensor Manager (sensor_manager.*)
**BME280 (I2C):**
- Temperature, humidity, pressure
- Validation: reject NaN values
- Use lastValidValue for stability

**DS18B20 (OneWire):**
- Additional temperature probe
- Error detection: -127 indicates failure
- Averaging for noise reduction

**Error handling pattern:**
```cpp
if (isnan(temperature)) {
    Serial.println("Помилка: отримано NaN з датчика BME280");
    return lastValidValue;
}
```

### Climate Logic (advanced_climate_logic.*, learning_system.*)
- PID regulator with tuning
- Adaptive learning algorithms
- Hysteresis management
- Trend analysis

### Actuator Manager (actuator_manager.*)
**PWM Control:**
- Pump, fan, exhaust: ledcWrite()
- Servo control for dampers
- Smooth ramping for longevity
- Power limiting

**Channels:**
```cpp
const int PUMP_PWM_CHANNEL = 0;
const int FAN_PWM_CHANNEL = 1;
// etc.
```

### Web Interface (web_interface.*)
**Server:** ESPAsyncWebServer
**Endpoints:**
- GET /api/data - current readings
- POST /api/control - manual control
- GET /api/config - settings
- POST /api/config - update settings

**UI:** HTML/CSS/JS served from SPIFFS
**Language:** All strings in Ukrainian

### Configuration (config.*)
**Storage:** EEPROM/Preferences
**Settings:**
- Temperature thresholds
- PID coefficients  
- Network credentials
- GPIO assignments
- Timing intervals

### Global Declarations (global_declarations.h)
**Purpose:** Central instance/state declarations
**Rule:** Add globals ONLY if essential
**Pattern:**
```cpp
extern float targetTemperature;
extern bool isHeatingActive;
extern SensorManager sensors;
```

## Hardware Details

### ESP32-S3 GPIO Mapping
**See config.h for current assignments**

General pattern:
- I2C: GPIO 21 (SDA), GPIO 22 (SCL)
- OneWire: GPIO configurable
- PWM outputs: High-current capable pins
- Analog inputs: ADC1 pins preferred

**NEVER change pin assignment** except in:
1. config.h
2. global_declarations.h

## Network Architecture

### WiFi Management
- Multiple network support (SSID + password arrays)
- Auto-connect on boot
- Auto-reconnect on disconnect
- Fallback sequence through configured networks

### Access Methods
1. **Windows:** http://klimat (NetBIOS)
2. **Mac/Linux:** http://klimat.local (mDNS)
3. **Fallback:** Direct IP address

### API Architecture
- RESTful endpoints in web_interface.cpp
- JSON responses for data exchange
- AJAX polling from web UI
- WebSocket for real-time updates (if implemented)

## Safety Architecture

### Emergency Modes
**FORCE (<20°C):**
- Bypass normal logic
- Maximum heating output
- Frequent sensor reads

**EMERGENCY (<18°C):**
- Critical cold protection
- All available heating
- Alert notifications

**Implementation:** DO NOT disable unless absolutely necessary

## Memory Management (Critical for Google Sheets Sync)

### Heap Calculation Strategy
**ALWAYS follow this order:**

1. **Read constants from config.h:**
   - `HISTORY_BUFFER_SIZE` (typically 1440 records = 24 hours)
   - Check other buffer-related defines

2. **Calculate DataRecord size:**
   ```cpp
   sizeof(DataRecord) ≈ 50 bytes (including padding/alignment)
   ```

3. **Calculate total requirement:**
   - Buffer: `HISTORY_BUFFER_SIZE * sizeof(DataRecord)` ≈ 70KB
   - SSL overhead: 20-25KB (depends on allocation method)
   - CSV String buffer: `SYNC_BATCH_SIZE * 70` bytes
   - **Total: ~95-100KB minimum**

4. **Check before allocation:**
   ```cpp
   uint32_t required = (HISTORY_BUFFER_SIZE * sizeof(DataRecord)) + 25000;
   if (ESP.getFreeHeap() < required) {
       // Handle insufficient memory
   }
   ```

### Optimization Techniques

**SSL Client:**
- ✅ Stack allocation: `WiFiClientSecure sslClient;` (saves ~16KB heap)
- ❌ Heap allocation: `new WiFiClientSecure()` (wastes heap)

**Buffer Management:**
- ✅ In-place filtering (reuse allocated buffer)
- ❌ Double malloc (allRecords + newRecords)

**String Optimization:**
- ✅ Pre-reserve: `csvData.reserve(count * 70);`
- ✅ snprintf to fixed buffer, then append
- ❌ String concatenation in loop

**Batch Size:**
- Smaller `SYNC_BATCH_SIZE` = less CSV buffer memory
- Larger = more network efficiency
- Balance: 30 records ≈ 2KB CSV buffer

### Heap Fragmentation
ESP32 heap fragments over time from:
- WebServer allocations
- WiFi stack
- String operations
- Multiple malloc/free cycles

**Strategy:**
- Allocate large buffers early
- Free immediately after use
- Consider `heap_caps_get_largest_free_block()` for actual available contiguous memory

### Common Pitfalls
1. **Not checking HISTORY_BUFFER_SIZE** - assumes fixed 1000
2. **SSL on heap** - wastes 16KB unnecessarily
3. **Double malloc** - temporary + filtered arrays
4. **String += in loop** - causes fragmentation
5. **Ignoring alignment** - sizeof may be larger than sum of fields

### Watchdog Protection
- Hardware timer monitors main loop
- Must execute quickly
- Reset if loop hangs
- Prevents system freeze

## Data Flow

```
Sensors → Manager → Validation → Logic → Actuators
                         ↓
                    Web Interface
                         ↓
                   Configuration
                         ↓
                   Google Sheets
```

## File Organization

```
src/
├── main.cpp                    # Entry point
├── system_core.*              # Main orchestration
├── sensor_manager.*           # All sensor logic
├── actuator_manager.*         # All actuator control
├── advanced_climate_logic.*   # PID & algorithms
├── learning_system.*          # Adaptive features
├── web_interface.*            # HTTP server & API
├── config.*                   # Settings management
└── global_declarations.h      # Shared instances

data/
└── web/                       # HTML/CSS/JS files

docs/
└── .ai-instructions/          # AI agent rules
```

## Processing Loop Pattern

Typical flow in main loop:
1. **Read** sensors via SensorManager
2. **Validate** readings (NaN check, range check)
3. **Process** through climate logic
4. **Apply** via ActuatorManager
5. **Update** web interface data
6. **Log** if necessary

**Rule:** Never call setup() or loop() directly for components
**Pattern:** Always integrate via managers

## Error Handling Philosophy

1. **Validate everything** from sensors
2. **Graceful degradation** on sensor failure
3. **Use last valid value** rather than panic
4. **Log errors** but continue operation
5. **Alert user** via web interface
6. **Safe state** on critical failure

## Testing Strategy

For new features:
1. Unit test individual functions
2. Integration test with managers
3. Safety test (failure modes)
4. Performance test (timing, memory)
5. User acceptance test (web UI)
