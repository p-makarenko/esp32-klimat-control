# ESP32 Klimat Control - Технічний Звіт

> **Версія:** v5.0-D58
> **Платформа:** ESP32-S3, PlatformIO, Arduino Framework
> **Мова інтерфейсу та коду:** Українська
> **Рядків коду:** ~14,748
> **Розмір прошивки:** ~1,508 KB

---

## 1. Структура Каталогів

```
klimat_kontrol/
├── .claude/                     # Налаштування Claude AI
│   ├── settings.json
│   └── settings.local.json
├── .vscode/                     # VS Code конфігурація
│   ├── c_cpp_properties.json
│   ├── extensions.json
│   ├── launch.json
│   ├── settings.json
│   └── tasks.json
├── docs/                        # Документація
│   ├── .ai-instructions/        # AI інструкції
│   │   ├── architecture.md
│   │   ├── code-conventions.md
│   │   ├── production-ready.md
│   │   ├── skeptical-architect.md
│   │   ├── anti-patterns.md
│   │   └── ...
│   ├── setup/                   # Посібники налаштування
│   │   ├── GOOGLE_SHEETS_SYNC_GUIDE.md
│   │   ├── NETWORK_SETUP.md
│   │   └── ...
│   └── research/                # Дослідження
├── include/                     # Додаткові заголовки
│   ├── advanced_climate_logic.h
│   ├── global_declarations.h
│   └── sensor_manager.h
├── lib/                         # Зовнішні бібліотеки (placeholder)
├── logs/                        # Runtime логи
├── src/                         # ВИХІДНИЙ КОД (44 файли)
│   ├── main.cpp                 # Точка входу
│   ├── config.h                 # Конфігурація пінів та констант
│   ├── system_core.*            # Ядро системи, структури даних
│   ├── global_declarations.*    # Глобальні змінні
│   ├── sensor_manager.*         # Датчики (DS18B20, BME280)
│   ├── actuator_manager.*       # Виконавчі пристрої (насос, вентилятор, серво)
│   ├── advanced_climate_logic.* # Розширена логіка клімату
│   ├── learning_system.*        # Система адаптивного навчання
│   ├── data_logger.*            # Логування в RAM та SPIFFS
│   ├── data_storage.*           # Збереження конфігурації
│   ├── config_manager.*         # Backup, restore, factory reset
│   ├── google_sheets_sync.*     # Синхронізація з Google Sheets
│   ├── energy_monitor.*         # Моніторинг енергоспоживання (PZEM-004T)
│   ├── ota_manager.*            # OTA оновлення прошивки
│   ├── utility_functions.*      # Допоміжні функції
│   ├── web_interface.*          # Веб-сервер (головний модуль)
│   ├── web_common.h             # Спільні веб-компоненти
│   ├── web_pages_main.cpp       # Головна сторінка
│   ├── web_pages_control.cpp    # Сторінка управління
│   ├── web_pages_settings.cpp   # Налаштування
│   ├── web_pages_history.cpp    # Історія даних
│   ├── web_pages_wifi.cpp       # WiFi налаштування
│   ├── web_pages_servo.cpp      # Калібрування серво
│   ├── web_pages_console.cpp    # Веб-консоль
│   ├── websocket_console.*      # WebSocket консоль
│   └── co2_sensor.*             # CO2 датчик (підготовлено)
├── test/                        # Тести (placeholder)
├── пристрої/                    # Документація обладнання
├── platformio.ini               # PlatformIO конфігурація
├── partitions_ota.csv           # Таблиця розділів OTA
├── CLAUDE.md                    # AI інструкції
└── README.md                    # Документація проєкту
```

---

## 2. Залежності (platformio.ini)

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.partitions = partitions_ota.csv

lib_deps =
    bblanchon/ArduinoJson           # JSON парсинг
    madhephaestus/ESP32Servo        # Серво управління
    paulstoffregen/OneWire          # 1-Wire протокол
    milesburton/DallasTemperature   # DS18B20 датчики
    adafruit/Adafruit BME280 Library # BME280 датчик
    mandulaj/PZEM-004T-v30          # Лічильник енергії
    links2004/WebSockets@^2.4.1     # WebSocket підтримка
```

### Таблиця розділів (partitions_ota.csv)

| Розділ  | Тип    | Розмір   | Призначення              |
|---------|--------|----------|--------------------------|
| nvs     | data   | 20 KB    | Preferences (NVS)        |
| otadata | data   | 8 KB     | OTA метадані             |
| app0    | app    | 1920 KB  | Основна прошивка (OTA 0) |
| app1    | app    | 1920 KB  | Резервна прошивка (OTA 1)|
| spiffs  | data   | 192 KB   | Файлова система (логи)   |

---

## 3. Опис Ключових Модулів

### 3.1 Ядро Системи

#### `main.cpp` - Точка входу
- Ініціалізація всіх підсистем у `setup()`
- Головний цикл `loop()`: OTA обробка, WiFi реконект, Serial команди, автостатус
- Функції: `emergencyStop()`, `runSystemTest()`, `resetToFactoryDefaults()`

#### `system_core.h/cpp` - Центральні структури та FreeRTOS
```cpp
// Ключові структури:
struct SensorData {         // Дані датчиків
    float tempCarrier;      // Температура теплоносія
    float tempRoom;         // Температура кімнати
    float tempBME;          // BME280 температура
    float humidity;         // Вологість
    float pressure;         // Тиск
    bool carrierValid, roomValid, bmeValid;
};

struct SystemConfig {       // Конфігурація системи
    float tempMin, tempMax; // Порогові температури
    bool heatingEnabled;    // Обігрів увімкнено
    bool coolingMode;       // Режим охолодження (літо)
    HumidityConfig humidityConfig;
    ExtractorTimer extractorTimer;
    // ... та багато інших параметрів
};

struct HeatingState {       // Стан опалення
    bool active;
    uint8_t pumpPower, fanPower, extractorPower;
    bool forceMode, emergencyMode, manualMode;
    bool adaptive_heating_active;
    // Історія для тренду
    float tempHistory[TEMP_HISTORY_SIZE];
};

struct PowerOutageState {   // Моніторинг аварій теплоносія
    bool detected;
    uint8_t recoveryStage;  // Етап відновлення (0-3)
    bool emergencyHeatingActive;
};
```

#### `config.h` - Константи конфігурації
```cpp
// Піни
#define ONE_WIRE_PIN      15    // DS18B20
#define I2C_SDA           1     // BME280
#define I2C_SCL           2
#define PUMP_PWM_PIN      21    // Насос
#define FAN_PWM_PIN       38    // Вентилятор
#define EXTRACTOR_PIN     12    // Витяжка
#define SERVO_PIN         14    // Серво заслінка
#define HUMIDIFIER_PIN    6     // Зволожувач

// Порогові значення
#define TEMP_MIN_THRESHOLD     24.0f
#define TEMP_MAX_THRESHOLD     26.0f
#define TEMP_CRITICAL_LOW      20.0f   // Форсаж
#define TEMP_EMERGENCY_LOW     18.0f   // Аварія

// Системні
#define HISTORY_BUFFER_SIZE    720     // 12 годин по 1 хв
#define PWM_FREQ               25000   // 25 кГц (безшумно)
```

### 3.2 Датчики

#### `sensor_manager.h/cpp`
- `initTemperatureSensors()` - DS18B20 x2 (теплоносій + кімната)
- `initBME280()` - I2C датчик (температура, вологість, тиск)
- `sensorTask()` - FreeRTOS таск зчитування кожні 2 сек

### 3.3 Виконавчі Пристрої

#### `actuator_manager.h/cpp`
```cpp
// PWM управління
void setPumpPercent(uint8_t percent);
void pumpStartupRamp(uint8_t targetPercent);  // Плавний розгін
void setFanPercent(uint8_t percent);
void setExtractorPercent(uint8_t percent);

// Серво (заслінка вентиляції)
void moveServoSmooth(int targetAngle);  // 18° відкрито, 166° закрито

// FreeRTOS таски
void heatingTask(void* parameter);
void ventilationTask(void* parameter);
```

### 3.4 Клімат-Логіка

#### `advanced_climate_logic.h/cpp`
```cpp
// Режими роботи
void smartHeatingControl();      // Автоматичне опалення
void smartCoolingControl();      // Режим охолодження (літо)

// Аварійний моніторинг
float analyzeTempTrend();        // Аналіз тренду °C/хв
void monitorPowerOutage();       // Виявлення відключення теплоносія
void cascadeEmergencyHeating();  // Каскадне аварійне опалення

// Адаптація
void checkAdaptiveThresholds();  // Динамічне зниження порогів
void checkFanMaxPowerEfficiency(); // Ефективність вентилятора
bool isHeatingSeasonActive();    // Сезонне відключення

// Режими: AUTO → FORCE (t<20°C) → EMERGENCY (t<18°C)
```

### 3.5 Система Навчання

#### `learning_system.h/cpp`
```cpp
struct LearningEntry {
    float roomError;        // Відхилення від цільової температури
    uint8_t fanPercent;     // Налаштування вентилятора
    uint8_t pumpPercent;    // Налаштування насоса
    uint8_t uses;           // Кількість використань
    uint32_t lastUsed;      // Час останнього використання
};

// Максимум 100 записів навчання
bool findOptimalSettings(float tempError, uint8_t &fanOut, uint8_t &pumpOut);
void recordLearning(float tempError, uint8_t fan, uint8_t pump, bool success);
```

### 3.6 Логування Даних

#### `data_logger.h/cpp`
```cpp
struct DataRecord {
    unsigned long timestamp;    // Unix timestamp
    uint32_t sequenceNumber;    // Унікальний ID
    float tempCarrier, tempRoom, tempBME, humidity;
    uint8_t pumpPower, fanPower, extractorPower;
    uint8_t mode;  // 0=AUTO, 1=MANUAL, 2=FORCE, 3=EMERGENCY
};

// Двохрівнева система:
// 1. RAM буфер: 720 записів (12 год), кожна хвилина
// 2. SPIFFS: агреговані дані (5 хв), ротація файлів
```

### 3.7 Хмарна Синхронізація

#### `google_sheets_sync.h/cpp`
```cpp
#define SYNC_INTERVAL_MS 1800000      // 30 хвилин
#define SYNC_BATCH_SIZE 30            // Записів за пакет

bool syncToGoogleSheets();            // Ручна синхронізація
void autoSyncTask();                  // Автоматична (викликається в loop)
bool sendBatchToSheets(records, count); // Відправка пакету

// Зберігає lastSentSequence в NVS для уникнення дублів
```

### 3.8 Веб-Інтерфейс

#### `web_interface.h/cpp` + `web_pages_*.cpp`
```cpp
// Сторінки:
/                  - handleRoot()          // Головна (статус)
/control           - handleControlPage()   // Управління
/settings          - handleSettingsPage()  // Налаштування
/history           - handleHistoryPage()   // Історія + графіки
/wifi              - handleWiFiPage()      // WiFi налаштування
/servo             - handleServoPage()     // Калібрування серво
/ota               - handleOTAPage()       // OTA оновлення
/energy            - handleEnergyPage()    // Енергомонітор
/console           - handleConsolePage()   // WebSocket консоль
/learning          - handleLearningPage()  // Система навчання

// API endpoints:
/api/status        - JSON статус
/api/history       - JSON історія
/api/command       - Виконання команд
```

### 3.9 OTA та Backup

#### `ota_manager.h/cpp`
```cpp
bool initOTA();                    // ArduinoOTA ініціалізація
void handleOTA();                  // Виклик в loop()
bool updateFromURL(url, md5);      // Оновлення з URL
bool backupBeforeOTA();            // Backup перед оновленням
```

#### `config_manager.h/cpp`
```cpp
bool createBackup(description);    // Backup в /backup.json
bool restoreBackup();              // Відновлення з backup
String exportConfigJSON();         // Експорт конфігурації
bool importConfigJSON(json);       // Імпорт конфігурації
void factoryResetComplete();       // Повний скид
void checkAndMigrateConfig();      // Міграція версій конфігурації
```

### 3.10 Енергомоніторинг

#### `energy_monitor.h/cpp`
```cpp
struct EnergyMeasurements {
    float voltage;      // V
    float current;      // A
    float power;        // W
    float energy;       // kWh
    float frequency;    // Hz
    float powerFactor;  // PF
};

// Використовує PZEM-004T-v30 лічильник
void updateEnergyData();
EnergyMeasurements getEnergyMeasurements();
```

---

## 4. Data Flow (Потік Даних)

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SENSORS LAYER                                 │
├─────────────────────────────────────────────────────────────────────┤
│  DS18B20 (x2)      BME280           PZEM-004T         Vent Switch   │
│  ├─ Теплоносій     ├─ Температура   ├─ Напруга        ├─ Стан       │
│  └─ Кімната        ├─ Вологість     ├─ Струм          └─ GPIO 13    │
│                    └─ Тиск          └─ Потужність                   │
└────────────────────────────┬────────────────────────────────────────┘
                             │ sensorTask() кожні 2 сек
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     CORE DATA STRUCTURES                             │
├─────────────────────────────────────────────────────────────────────┤
│  SensorData          SystemConfig        HeatingState               │
│  ├─ tempCarrier      ├─ tempMin/Max      ├─ active                  │
│  ├─ tempRoom         ├─ heatingEnabled   ├─ pumpPower               │
│  ├─ humidity         ├─ coolingMode      ├─ fanPower                │
│  └─ pressure         └─ extractorTimer   └─ forceMode/emergencyMode │
└────────────────────────────┬────────────────────────────────────────┘
                             │
          ┌──────────────────┼──────────────────┐
          ▼                  ▼                  ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│  CLIMATE LOGIC  │  │   DATA LOGGING  │  │   WEB INTERFACE │
├─────────────────┤  ├─────────────────┤  ├─────────────────┤
│ smartHeating()  │  │ RAM Buffer      │  │ WebServer:80    │
│ smartCooling()  │  │ (720 записів)   │  │ ├─ /status API  │
│ monitorPower()  │  │       │         │  │ ├─ /control     │
│ analyzeTrend()  │  │       ▼         │  │ └─ /settings    │
│       │         │  │ SPIFFS Files    │  │       │         │
│       │         │  │ (агрегація 5хв) │  │       │         │
└───────┼─────────┘  └────────┼────────┘  └───────┼─────────┘
        │                     │                   │
        ▼                     ▼                   ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│    ACTUATORS    │  │  GOOGLE SHEETS  │  │  USER COMMANDS  │
├─────────────────┤  ├─────────────────┤  ├─────────────────┤
│ Pump (PWM 21)   │  │ autoSyncTask()  │  │ Serial Monitor  │
│ Fan (PWM 38)    │  │ кожні 30 хв     │  │ Web Console     │
│ Extractor (12)  │  │ batch по 30     │  │ WebSocket       │
│ Servo (14)      │  │ записів         │  │                 │
│ Humidifier (6)  │  │                 │  │                 │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

### Режими Роботи

```
┌─────────────────────────────────────────────────────────────────────┐
│                      OPERATION MODES                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   t > 26°C          24°C < t < 26°C       t < 24°C                  │
│   ┌─────┐           ┌─────────┐           ┌─────┐                   │
│   │COOL │  ───────► │  AUTO   │ ◄───────  │HEAT │                   │
│   └─────┘           └─────────┘           └─────┘                   │
│                          │                    │                      │
│                          │                    │                      │
│                          │              t < 20°C                     │
│                          │                    │                      │
│                          │                    ▼                      │
│                          │              ┌─────────┐                  │
│                          │              │ FORCE   │ (100% pump+fan)  │
│                          │              └────┬────┘                  │
│                          │                   │                       │
│                          │              t < 18°C                     │
│                          │                   │                       │
│                          │                   ▼                       │
│                          │              ┌─────────┐                  │
│                          └─────────────►│EMERGENCY│ (каскадне       │
│                                         └─────────┘  відновлення)    │
│                                                                      │
│   MANUAL MODE: обходить всю автоматику                              │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Ключові Конфігураційні Файли

### 5.1 platformio.ini (повний вміст)

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.partitions = partitions_ota.csv
upload_port = COM5
monitor_speed = 115200
monitor_encoding = utf-8
monitor_filters = esp32_exception_decoder, log2file
monitor_echo = yes
extra_scripts = pre:count_lines.py
build_type = debug
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -g
    -ggdb
    -Og

lib_deps =
    bblanchon/ArduinoJson
    madhephaestus/ESP32Servo
    paulstoffregen/OneWire
    milesburton/DallasTemperature
    adafruit/Adafruit BME280 Library
    mandulaj/PZEM-004T-v30
    links2004/WebSockets@^2.4.1
```

### 5.2 partitions_ota.csv (повний вміст)

```csv
# ESP32-S3 16MB Partition Table for OTA Updates
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,   0x5000
otadata,  data, ota,     0xe000,   0x2000
app0,     app,  ota_0,   0x10000,  0x1E0000
app1,     app,  ota_1,   0x1F0000, 0x1E0000
spiffs,   data, spiffs,  0x3D0000, 0x30000
```

### 5.3 config.h (ключові константи)

```cpp
// ============= ПІНИ =============
#define ONE_WIRE_PIN      15    // DS18B20 датчики температури
#define I2C_SDA           1     // BME280 SDA
#define I2C_SCL           2     // BME280 SCL
#define PUMP_PWM_PIN      21    // Насос (PWM)
#define FAN_PWM_PIN       38    // Вентилятор обігріву (PWM)
#define EXTRACTOR_PIN     12    // Витяжка (PWM)
#define SERVO_PIN         14    // Сервопривод вентиляції
#define VENT_SWITCH_PIN   13    // Механічний вимикач вентиляції
#define HUMIDIFIER_PIN    6     // Зволожувач (цифровий)

// ============= PWM =============
#define PWM_FREQ          25000  // 25 кГц для безшумної роботи
#define PWM_RESOLUTION    8      // 8-біт (0-255)

// ============= ТЕМПЕРАТУРА =============
#define TEMP_MIN_THRESHOLD     24.0f   // Нижній поріг
#define TEMP_MAX_THRESHOLD     26.0f   // Верхній поріг
#define TEMP_CRITICAL_LOW      20.0f   // Форсаж
#define TEMP_EMERGENCY_LOW     18.0f   // Аварія
#define TEMP_CARRIER_MIN       35.0f   // Мін температура теплоносія

// ============= ВОЛОГІСТЬ =============
#define HUM_MIN_DEFAULT        65.0f
#define HUM_MAX_DEFAULT        70.0f

// ============= СЕРВО =============
#define SERVO_CLOSED_ANGLE     166     // Закрито
#define SERVO_OPEN_ANGLE       18      // Відкрито

// ============= СИСТЕМНІ =============
#define SENSOR_READ_INTERVAL   2000    // Читання датчиків (мс)
#define HISTORY_BUFFER_SIZE    720     // 12 годин даних
#define NTP_UPDATE_INTERVAL    3600000 // Оновлення часу (1 год)

// ============= WIFI =============
#define WIFI_SSID "Redmi Note 14"
#define WIFI_PASSWORD "12345678"
#define KNOWN_NETWORKS_COUNT 2
```

---

## 6. FreeRTOS Задачі

| Задача               | Пріоритет | Stack | Інтервал   | Функція                    |
|----------------------|-----------|-------|------------|----------------------------|
| sensorTask           | 3         | 4096  | 2 сек      | Зчитування датчиків        |
| heatingTask          | 2         | 4096  | 1 сек      | Управління опаленням       |
| ventilationTask      | 2         | 4096  | 1 сек      | Управління вентиляцією     |
| webTask              | 1         | 8192  | постійно   | Обробка веб-запитів        |
| timeTask             | 1         | 4096  | 1 год      | Синхронізація NTP          |
| advancedLogicTask    | 2         | 4096  | 1 сек      | Клімат-логіка              |
| dataLoggerTask       | 1         | 4096  | 1 хв       | Логування даних            |

---

## 7. API Endpoints

### REST API

| Endpoint              | Метод | Опис                          |
|-----------------------|-------|-------------------------------|
| `/`                   | GET   | Головна сторінка              |
| `/api/status`         | GET   | JSON статус системи           |
| `/api/history`        | GET   | JSON історія (RAM buffer)     |
| `/api/history/stats`  | GET   | Статистика історії            |
| `/api/command`        | POST  | Виконання команди             |
| `/api/energy`         | GET   | Дані енергомоніторингу        |
| `/api/learning`       | GET   | Дані системи навчання         |
| `/api/servo`          | POST  | Управління серво              |
| `/ota/status`         | GET   | Статус OTA                    |
| `/backup/create`      | POST  | Створити backup               |
| `/backup/restore`     | POST  | Відновити backup              |
| `/config/export`      | GET   | Експорт конфігурації (JSON)   |
| `/config/import`      | POST  | Імпорт конфігурації           |

### Serial Команди

```
s, status     - Статус системи
m, menu       - Повне меню
a50           - Насос 50%
b80           - Вентилятор 80%
c30           - Витяжка 30%
tmin 22       - Мін температура 22°C
tmax 25       - Макс температура 25°C
hmin 60       - Мін вологість 60%
hmax 70       - Макс вологість 70%
test          - Повний тест системи
stop          - Аварійна зупинка
start         - Автоматичний режим
reset         - Factory reset
sheets-sync   - Синхронізація з Google Sheets
reboot        - Перезавантаження
```

---

## 8. Мережеві Можливості

- **WiFi Station** - підключення до існуючої мережі
- **WiFi AP** - точка доступу при втраті зв'язку
- **mDNS** - `http://klimat.local` (Mac/iOS/Linux)
- **NetBIOS** - `http://klimat` (Windows)
- **NTP** - синхронізація часу (pool.ntp.org)
- **OTA** - оновлення прошивки по WiFi
- **WebSocket** - консоль реального часу

---

## 9. Збереження Даних

### NVS (Preferences)

| Namespace     | Ключі                                              |
|---------------|----------------------------------------------------|
| climate       | tempMin, tempMax, heatingEnabled, manualModeLocked |
| wifi          | ssid0-4, pass0-4, netCount                         |
| data_logger   | lastSeq, ramIndex                                  |
| sheets_sync   | lastSentSeq, lastSync                              |
| learning      | entries (JSON), enabled                            |

### SPIFFS

| Файл           | Призначення                   |
|----------------|-------------------------------|
| /current.csv   | Поточний лог (агреговані дані)|
| /archive_*.csv | Архівні логи                  |
| /backup.json   | Backup конфігурації           |
| /export.json   | Експорт конфігурації          |
| /config.json   | Метадані логування            |

---

## 10. Версійність та Міграція

```cpp
#define VERSION "v5.0-D58"
#define CONFIG_VERSION_MAJOR 5
#define CONFIG_VERSION_MINOR 0

// При оновленні:
void checkAndMigrateConfig();  // Перевірка та міграція
void migrateConfigVersion(fromMajor, fromMinor);  // Міграція
```

---

## 11. Аварійні Сценарії

1. **FORCE Mode** (t < 20°C): Насос 100%, Вентилятор 100%
2. **EMERGENCY Mode** (t < 18°C): Каскадне відновлення з моніторингом
3. **Power Outage Detection**: Аналіз тренду температури теплоносія
4. **Watchdog**: Автоматичний перезапуск при зависанні
5. **OTA Failure**: Автоматичний відкат на попередню прошивку

---

## 12. Рекомендації для AI-Моделі

При роботі з цим проєктом:

1. **Мова**: Весь код та коментарі українською
2. **Стиль**: C++17, Arduino Framework
3. **Критичні файли**: `global_declarations.h`, `config.h`, `system_core.h`
4. **Не запускати**: `pio run` без явного запиту
5. **RAM обмеження**: Уважно з буферами (ESP32-S3 має 320KB RAM)
6. **SPIFFS обмеження**: 192KB, ротація файлів обов'язкова
7. **FreeRTOS**: Використовувати mutex для глобальних структур

---

*Згенеровано: 2026-02-03*
*Версія проєкту: v5.0-D58*
