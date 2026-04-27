#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// КОНФІГУРАЦІЯ ПІНІВ
// ============================================================================
#define ONE_WIRE_PIN      16    // DS18B20 датчики температури
#define I2C_SDA           1     // BME280 SDA
#define I2C_SCL           2     // BME280 SCL
#define SCD30_I2C_SDA     4     // SCD30 SDA
#define SCD30_I2C_SCL     5     // SCD30 SCL
#define PUMP_PWM_PIN      21    // Насос (PWM)
#define FAN_PWM_PIN       38    // Вентилятор обігріву (PWM)
#define EXTRACTOR_PIN     12    // Витяжка (PWM)
#define SERVO_PIN         14    // Сервопривод вентиляції
#define VENT_SWITCH_PIN   13    // Механічний вимикач вентиляції (HIGH = відкрити)
#define HUMIDIFIER_PIN    6     // Зволожувач (цифровий)
#define PZEM_RX_PIN       18    // PZEM004T RX = U1RXD (TX PZEM → GPIO 18)
#define PZEM_TX_PIN       17    // PZEM004T TX = U1TXD (RX PZEM → GPIO 17)

// ============================================================================
// PWM НАЛАШТУВКИ
// ============================================================================
#define PWM_FREQ          25000  // 25 кГц для безшумної роботи
#define PWM_RESOLUTION    8      // 8-біт (0-255)
#define PUMP_CHANNEL      0
#define FAN_CHANNEL       1
#define EXTRACTOR_CHANNEL 2

// ============================================================================
// ТЕМПЕРАТУРНІ КОНСТАНТИ
// ============================================================================
#define TEMP_MIN_THRESHOLD     24.0f   // Нижній поріг температури
#define TEMP_MAX_THRESHOLD     26.0f   // Верхній поріг температури
#define TEMP_CARRIER_MIN       35.0f   // Мінімальна температура теплоносія
#define TEMP_CRITICAL_LOW      20.0f   // Критично низька температура
#define TEMP_EMERGENCY_LOW     18.0f   // Аварійна температура
#define TEMP_VENT_MIN          25.0f   // Мінімальна температура для вентиляції
#define TEMP_VENT_MAX          28.0f   // Максимальна температура для вентиляції

// Константи для виявлення відключення живлення (по напрузі PZEM)
#define POWER_OUTAGE_VOLTAGE_MIN  175.0f  // Мінімальна допустима напруга (В)
#define POWER_OUTAGE_VOLTAGE_MAX  255.0f  // Максимальна допустима напруга (В)
#define POWER_OUTAGE_CHECK_INTERVAL 2000  // Інтервал перевірки напруги (мс)

// ============================================================================
// ВОЛОГІСТЬ КОНСТАНТИ
// ============================================================================
#define HUM_MIN_DEFAULT        65.0f   // Базова мінімальна вологість
#define HUM_MAX_DEFAULT        70.0f   // Базова максимальна вологість
#define HUM_TEMP_COEFF         0.5f    // Коефіцієнт корекції %/C
#define HUMIDIFIER_MIN_INTERVAL 30000  // Мінімальний інтервал роботи зволожувача (мс)

// ============================================================================
// СЕРВО КОНСТАНТИ
// ============================================================================
#define SERVO_CLOSED_ANGLE     166     // Кут закритої вентиляції
#define SERVO_OPEN_ANGLE       18      // Кут відкритої вентиляції
#define SERVO_DETACH_DELAY     1000    // Затримка перед detach (мс)

// ============================================================================
// ВЕНТИЛЯТОР РОСЛИН
// ============================================================================
#define PLANT_FAN_PIN              11    // GPIO вентилятора рослин (PWM)
#define PLANT_FAN_CHANNEL          3     // PWM канал (0-2 зайняті)

// Таймер
#define PLANT_FAN_TIMER_DEFAULT_ON   20  // Час роботи за замовчуванням (хв)
#define PLANT_FAN_TIMER_DEFAULT_OFF  40  // Час паузи за замовчуванням (хв)
#define PLANT_FAN_TIMER_DEFAULT_POWER 50 // Потужність за замовчуванням (%)

// Режим природнього вітру
#define BREEZE_BASE_MIN      20   // Мінімальна базова швидкість (%)
#define BREEZE_BASE_MAX      60   // Максимальна базова швидкість (%)
#define BREEZE_GUST_BOOST    30   // Підсилення пориву відносно бази (%)
#define BREEZE_GUST_MIN_SEC  2    // Мінімальна тривалість пориву (сек)
#define BREEZE_GUST_MAX_SEC  8    // Максимальна тривалість пориву (сек)
#define BREEZE_CALM_MIN_SEC  5    // Мінімальне затишшя (сек)
#define BREEZE_CALM_MAX_SEC  20   // Максимальне затишшя (сек)

// ============================================================================
// ТАЙМЕР ВИТЯЖКИ КОНСТАНТИ
// ============================================================================
#define EXTRACTOR_TIMER_DEFAULT_ON   30    // Час роботи за замовчуванням (хв)
#define EXTRACTOR_TIMER_DEFAULT_OFF  30    // Час паузи за замовчуванням (хв)
#define EXTRACTOR_TIMER_DEFAULT_POWER 70   // Потужність за замовчуванням (%)
#define EXTRACTOR_TIMER_MIN_ON       1     // Мінімальний час роботи (хв)
#define EXTRACTOR_TIMER_MAX_ON       240   // Максимальний час роботи (хв)
#define EXTRACTOR_TIMER_MIN_OFF      0     // Мінімальний час паузи (хв)
#define EXTRACTOR_TIMER_MAX_OFF      240   // Максимальний час паузи (хв)
#define EXTRACTOR_TIMER_MIN_POWER    10    // Мінімальна потужність (%)
#define EXTRACTOR_TIMER_MAX_POWER    100   // Максимальна потужність (%)

// ============================================================================
// СИСТЕМНІ КОНСТАНТИ
// ============================================================================
#define SENSOR_READ_INTERVAL   2000    // Інтервал читання датчиків (мс)
#define STATUS_PRINT_INTERVAL  30000   // Період автовиведення статусу (мс)
#define TEMP_HISTORY_SIZE      300     // Історія температури (5 хв при 1с)
#define HEATING_CHECK_INTERVAL 300000  // Перевірка ефективності (5 хв)
#define HISTORY_BUFFER_SIZE    360     // Зберігаємо 6 годин даних (360 хвилин, оптимізовано для RAM при низькій вільній пам'яті)
#define NTP_UPDATE_INTERVAL    3600000 // Оновлення часу NTP кожну годину
#define EXTRACTOR_TIMER_CYCLE  3600000 // Цикл таймера витяжки (1 година)

// ============================================================================
// NTP НАЛАШТУВКИ
// ============================================================================
#define NTP_SERVER      "pool.ntp.org"
#define UTC_OFFSET_SEC  7200           // GMT+2 (Київ)
#define UTC_OFFSET_DST  3600           // Літній час +1 година
// POSIX timezone: EET-2EEST,M3.5.0/3,M10.5.0/4 (автоматичне переключення літній/зимовий)
#define TZ_INFO         "EET-2EEST,M3.5.0/3,M10.5.0/4"

// ============================================================================
// WI-FI НАЛАШТУВКИ
// ============================================================================
#define WIFI_SSID "Redmi Note 14"
#define WIFI_PASSWORD "12345678"
#define WIFI_CHANNEL       1
#define MAX_CLIENTS        4
// Структура для списку WiFi мереж
struct WiFiCredentials {
    const char* ssid;
    const char* password;
};

// Список відомих WiFi мереж (в порядку пріоритету)
#define KNOWN_NETWORKS_COUNT 2
const WiFiCredentials KNOWN_NETWORKS[KNOWN_NETWORKS_COUNT] = {
    {"Redmi Note 14", "12345678"},
    {"Redmi Note 9", "1234567890"}
};
// Додано: налаштування обмежень пристроїв за замовчуванням
#define PUMP_MIN_DEFAULT       30
#define PUMP_MAX_DEFAULT       100
#define FAN_MIN_DEFAULT        10
#define FAN_MAX_DEFAULT        100
#define EXTRACTOR_MIN_DEFAULT  6
#define EXTRACTOR_MAX_DEFAULT  100

#endif