# Дослідження стандартних підходів синхронізації ESP32 з Google Sheets

**Дата дослідження:** 17 січня 2026
**Проект:** Климат-контроль ESP32
**Мета:** Аналіз методів, бібліотек та best practices для надійної синхронізації даних

---

## 1. МЕТОДИ СИНХРОНІЗАЦІЇ ДАНИХ

### 1.1 Найпопулярніші методи (Порівняння)

| Метод | Витрати | Складність | Надійність | Рекомендація |
|-------|---------|-----------|-----------|------------|
| **Google Apps Script + HTTP POST** | Безплатно | Низька | Висока | ⭐⭐⭐⭐⭐ Найкраще для IoT |
| **Google Sheets API + Service Account** | Безплатно | Середня | Дуже висока | ⭐⭐⭐⭐ Найбезпечніше |
| **HTTPClient + WiFiClientSecure** | Безплатно | Низька | Висока | ⭐⭐⭐⭐⭐ Гнучкий підхід |
| **IFTTT Webhooks** | Платно (Pro) | Низька | Середня | ❌ Не рекомендується (платні) |
| **Firebase Realtime Database** | Платно (за обсяг) | Середня | Дуже висока | ❌ Недоречне для IoT |

### 1.2 Google Apps Script (Рекомендується для вашого проекту)

**Як це працює:**
```
ESP32 (WiFi)
  → HTTPS POST
    → Google Apps Script Web App
      → Парсинг CSV/JSON
        → Автоматичний запис у Google Sheets
```

**Переваги:**
- ✅ Абсолютно безплатно
- ✅ Немає потреби в OAuth
- ✅ Простота реалізації на ESP32
- ✅ Надійна обробка помилок в Google Scripts
- ✅ Автоматична синхронізація часу
- ✅ Безпосередній доступ до Google Sheets

**Недоліки:**
- ❌ Google обмежує кількість виконань (~5-6 в секунду)
- ❌ Потрібно копіювати Script ID у код ESP32

**Вашого проекту використання:**
```cpp
// Z файлу google_sheets_sync.cpp, лінія 281-289
String path = String(GOOGLE_SCRIPT_URL).substring(String(GOOGLE_SCRIPT_URL).indexOf("/macros"));
client->println("POST " + path + " HTTP/1.1");
client->println("Host: script.google.com");
client->println("Content-Type: text/csv");  // CSV формат - економ пам'яті
client->println("Content-Length: " + String(csvData.length()));
client->println("Connection: close");
client->println();
client->print(csvData);
```

### 1.3 Google Sheets API + Service Account

**Як це працює:**
```
ESP32
  → HTTPS POST (JSON)
    → Google Sheets API
      → Service Account (JWT auth)
        → Прямий запис в Google Sheets
```

**Переваги:**
- ✅ Максимум контролю
- ✅ Вища надійність аутентифікації
- ✅ Потребує менше Google Scripts
- ✅ Масштабується краще

**Недоліки:**
- ❌ Складніша конфігурація
- ❌ Більше коду на ESP32
- ❌ Потрібні credentials JSON
- ❌ Більше спалювання пам'яті (JWT генерація)

**Рекомендована бібліотека:**
- [mobizt/ESP-Google-Sheet-Client](https://github.com/mobizt/ESP-Google-Sheet-Client)

### 1.4 Прямий HTTPClient (Гнучкий підхід)

Найбільш гнучкий метод - прямі HTTPS запити без бібліотек:

**Переваги:**
- ✅ Мінімум залежностей
- ✅ Простіший відладок
- ✅ Повний контроль над запитом
- ✅ Використовується у вашому проекту ✓

**Недоліки:**
- ❌ Більше boilerplate коду
- ❌ Ручна обробка помилок

---

## 2. ПРОБЛЕМИ З ПАМ'ЯТТЮ НА ESP32 І РІШЕННЯ

### 2.1 Архітектура пам'яті ESP32

```
┌─────────────────────────────────────┐
│ ESP32 Memory Layout                 │
├─────────────────────────────────────┤
│ IRAM (160 KB)                       │ ← Task stacks, ISRs
├─────────────────────────────────────┤
│ DRAM (160 KB heap)                  │ ← Основна робоча пам'ять
├─────────────────────────────────────┤
│ SRAM Total: 320 KB                  │
├─────────────────────────────────────┤
│ PSRAM (externe) - 4 MB+             │ ← Якщо доступна
├─────────────────────────────────────┤
│ Flash (4MB) для SPIFFS              │ ← Постійне сховище
└─────────────────────────────────────┘
```

### 2.2 Типові проблеми при роботі з великими буферами

**Проблема 1: Фрагментація пам'яті**
```cpp
// ❌ ПОГАНО - багато малих malloc викликів
for (int i = 0; i < 100; i++) {
    char* buf = malloc(100);  // 100 вмисцях
}

// ✅ ДОБРЕ - один великий malloc
DataRecord* allRecords = (DataRecord*)malloc(HISTORY_BUFFER_SIZE * sizeof(DataRecord));
```

**Проблема 2: Stack overflow від великих локальних масивів**
```cpp
// ❌ ПОГАНО - стек переповнюється
void processData() {
    DataRecord buffer[1440];  // ~40 KB на стеку!
}

// ✅ ДОБРЕ - виділяємо в heap
void processData() {
    DataRecord* buffer = (DataRecord*)malloc(1440 * sizeof(DataRecord));
    // ... використання ...
    free(buffer);
}
```

**Проблема 3: SSL/TLS буфери споживають багато пам'яті**

Вашого проекту (лінія 206):
```cpp
// Примусове звільнення пам'яті перед SSL
heap_caps_malloc_extmem_enable(1024); // Дозволяємо external RAM якщо є
```

### 2.3 Best Practices для оптимізації пам'яті

#### 2.3.1 Використання PSRAM (зовнішня пам'ять)

```cpp
// Виділення пам'яті з PSRAM (якщо доступна)
#define malloc_psram(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#define malloc_internal(size) heap_caps_malloc(size, MALLOC_CAP_INTERNAL)

// Приклад:
DataRecord* buffer = (DataRecord*)malloc_psram(1440 * sizeof(DataRecord));

// Для DMA буферів (обов'язково внутрішня пам'ять)
uint8_t* dma_buffer = (uint8_t*)malloc_internal(512 | MALLOC_CAP_DMA);
```

**Обмеження PSRAM:**
- ⚠️ Недоступна коли виконується flash write
- ⚠️ Повільніша за DRAM
- ⚠️ Не може бути задачею стеку

#### 2.3.2 String оптимізація

```cpp
// ❌ ПОГАНО - утворює тимчасові копії
String csvData = "";
for (int i = 0; i < 100; i++) {
    csvData += "data,more,data\n";  // Кожен += робить реалокацію!
}

// ✅ ДОБРЕ - предиконує розмір
String csvData = "";
csvData.reserve(2000);  // Виділяємо місце один раз
for (int i = 0; i < 100; i++) {
    csvData += "data,more,data\n";
}

// ✅ ЩЕ КРАЩЕ - використовуємо char буфер для CSV
char buffer[2048];
snprintf(buffer, sizeof(buffer), "%.1f;%.1f\n", temp, humidity);
```

#### 2.3.3 Постійні дані у Flash

```cpp
// ❌ ПОГАНО - константи копіюються в RAM
const char* headers[] = {
    "Temperature",
    "Humidity",
    "Pressure"
};

// ✅ ДОБРЕ - залишаються у Flash
const char PROGMEM tempStr[] = "Temperature";
const char PROGMEM humidStr[] = "Humidity";
```

#### 2.3.4 Буферизація та батчування

Вашого проекту (лінія 125-132):
```cpp
// Копіюємо нові записи у малий буфер (до 30)
uint16_t toSend = newCount > 30 ? 30 : newCount;
DataRecord* sendBuffer = (DataRecord*)malloc(toSend * sizeof(DataRecord));

// Це мудре рішення - надсилаємо партіями замість всього одразу
```

### 2.4 Таблиця розмірів структур

```
Ваша структура DataRecord:
├─ timestamp       : 4 байти (unsigned long)
├─ sequenceNumber  : 4 байти (uint32_t)
├─ tempCarrier     : 4 байти (float)
├─ tempRoom        : 4 байти (float)
├─ tempBME         : 4 байти (float)
├─ humidity        : 4 байти (float)
├─ pumpPower       : 1 байт  (uint8_t)
├─ fanPower        : 1 байт  (uint8_t)
├─ extractorPower  : 1 байт  (uint8_t)
├─ mode            : 1 байт  (uint8_t)
└─ dataVersion     : 1 байт  (uint8_t)
────────────────────────────
ВСЬОГО: 28 байт на запис

1440 записів = 1440 × 28 = 40,320 байт (~40 KB)
```

**Це входить в доступну пам'ять!** ✓

---

## 3. ТИПОВІ ПРИКЛАДИ КОДУ І БІБЛІОТЕКИ

### 3.1 Arduino Libraries для Google Sheets

| Бібліотека | Призначення | Розмір | Потреба пам'яті |
|-----------|-----------|--------|--------------|
| **HTTPClient** (вбудована) | HTTP/HTTPS запити | ~30 KB | Мала |
| **WiFiClientSecure** (вбудована) | HTTPS з SSL/TLS | ~100 KB | Велика |
| **ArduinoJson** | JSON парсинг | ~50 KB | Середня |
| **esp-google-sheet-client** | API бібліотека | ~80 KB | Велика |
| **Time.h** (вбудована) | Робота з часом | Мала | Мала |

### 3.2 Приклад 1: Простої POST з CSV (Рекомендується)

```cpp
// ВАША РЕАЛІЗАЦІЯ У google_sheets_sync.cpp
// Це чудовий приклад для Google Apps Script

void sendData() {
    // 1. Формуємо CSV дані
    String csvData = "";
    csvData += "2025-01-17 12:30:45;123;22.5;21.3;22.1;65.5;80;70;60;0\n";

    // 2. Створюємо SSL клієнт
    WiFiClientSecure client;
    client->setInsecure();  // Дозволяємо self-signed сертифікати

    // 3. З'єднуємось
    if (!client->connect("script.google.com", 443)) {
        Serial.println("Connection failed");
        return;
    }

    // 4. Надсилаємо POST
    client->println("POST /macros/s/[SCRIPT_ID]/exec HTTP/1.1");
    client->println("Host: script.google.com");
    client->println("Content-Type: text/csv");
    client->println("Content-Length: " + String(csvData.length()));
    client->println("Connection: close");
    client->println();
    client->print(csvData);

    // 5. Читаємо відповідь
    while (client->available()) {
        String line = client->readStringUntil('\n');
        if (line.indexOf("HTTP/1.1 200") >= 0) {
            Serial.println("Success!");
        }
    }

    client->stop();
}
```

### 3.3 Приклад 2: JSON формат (Для API)

```cpp
#include <ArduinoJson.h>

void sendJSONData() {
    // 1. Створюємо JSON документ
    StaticJsonDocument<200> doc;
    doc["temperature"] = 22.5;
    doc["humidity"] = 65.5;
    doc["timestamp"] = 1705507845;

    // 2. Сериалізуємо у String
    String jsonData;
    serializeJson(doc, jsonData);

    // 3. Надсилаємо (як у прикладі вище)
    // ...
}
```

### 3.4 Приклад 3: Обробка помилок з retry (Best Practice)

```cpp
bool sendWithRetry(const char* data, int maxRetries = 3) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("Attempt %d/%d...\n", attempt, maxRetries);

        if (sendBatchToSheets((DataRecord*)data, 1)) {
            Serial.println("Success!");
            return true;
        }

        // Експоненційна затримка: 2, 4, 8 секунд
        unsigned long delay_ms = 1000 * (1 << (attempt - 1));  // 2^(attempt-1)
        Serial.printf("Retry in %lu ms...\n", delay_ms);
        delay(delay_ms);

        // Перевіряємо WiFi
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, reconnecting...");
            WiFi.reconnect();
            delay(2000);
        }
    }

    return false;  // Всі спроби невдалі
}
```

### 3.5 Приклад 4: Batch синхронізація зQueue (FreeRTOS)

```cpp
// Для кращого контролю над батчуванням можна використовувати FreeRTOS Queue

#define SYNC_QUEUE_SIZE 100

QueueHandle_t syncQueue = xQueueCreate(SYNC_QUEUE_SIZE, sizeof(DataRecord));

// Задача для асинхронної синхронізації
void syncTask(void *parameter) {
    DataRecord record;
    uint16_t batchSize = 0;
    DataRecord batch[30];

    while (true) {
        // Чекаємо на дані
        if (xQueueReceive(syncQueue, &record, pdMS_TO_TICKS(1000))) {
            batch[batchSize++] = record;

            // Коли накопилось 30 записів - надсилаємо
            if (batchSize >= 30) {
                sendBatchToSheets(batch, batchSize);
                batchSize = 0;
            }
        } else {
            // Timeout - надсилаємо те що є
            if (batchSize > 0) {
                sendBatchToSheets(batch, batchSize);
                batchSize = 0;
            }
        }
    }
}
```

---

## 4. ФОРМАТУВАННЯ ДАНИХ ДЛЯ GOOGLE SHEETS

### 4.1 Підтримувані формати

#### CSV (Рекомендується для вашого проекту)

**Переваги:**
- ✅ Мінімум пам'яті
- ✅ Простий парсинг в Google Scripts
- ✅ Нативна підтримка Google Sheets
- ✅ Використовується у вашому проекту

**Формат (вашої реалізації, лінія 241-263):**
```csv
2025-01-17 12:30:45;123;22,5;21,3;22,1;65,5;80;70;60;0;75,2
2025-01-17 12:31:45;124;22,4;21,2;22,0;65,3;75;70;60;0;74,8
```

**Параметри:**
- Роздільник: `;` (точка з комою - стандарт для європейської локалі)
- Числа: Використовується `,` як десятковий розділювач (не `.`)
- Дата/час: ISO 8601 формат (`YYYY-MM-DD HH:MM:SS`)

**Ваша реалізація (лінія 228-239):**
```cpp
// Форматуємо числа з комою як десятковий розділювач
snprintf(tempCarrier, sizeof(tempCarrier), "%.1f", records[i].tempCarrier);
// Замінюємо крапку на кому
for (char* p = tempCarrier; *p; p++) if (*p == '.') *p = ',';
```

Це чудово для європейської Google Sheets!

#### JSON (Для API)

```json
{
  "timestamp": "2025-01-17T12:30:45Z",
  "temperature": 22.5,
  "humidity": 65.5,
  "pumpPower": 80,
  "mode": "AUTO"
}
```

**Переваги:**
- ✅ Більш гнучкий
- ✅ Вкладена структура
- ✅ Стандартний для API

**Недоліки:**
- ❌ Більше пам'яті
- ❌ Складніше парсити в Google Scripts

### 4.2 Кодування символів

**UTF-8 (ОБОВ'ЯЗКОВО!):**
```cpp
// ✅ ДОБРЕ - ESP32 автоматично надсилає UTF-8
csvData += "Температура: " + String(temp) + "\n";

// ❌ ПОГАНО - Cyrillic символи можуть бути спотворені
const char* label = "Темпаратура";  // Без PROGMEM
```

**Google Sheets автоматично розпізнає UTF-8** ✓

### 4.3 Екранування спеціальних символів

```cpp
// CSV формат (RFC 4180)
// Правило: якщо поле містить кому, лапку або перевід - закриваємо в лапки

// ❌ ПОГАНО
String csv = "Примітка: Не працює, очекується \"виправлення\"\n";

// ✅ ДОБРЕ (якщо потрібно)
String csv = "\"Примітка: Не працює, очекується \"\"виправлення\"\"\"\n";

// Ваш проект - не має спеціальних символів в даних, тому OK
```

---

## 5. ОБРОБКА ПОМИЛОК ПРИ ПЕРЕДАЧІ ДАНИХ

### 5.1 Типові помилки

#### 5.1.1 WiFi помилки

```
WiFi.status() можливі значення:
├─ WL_CONNECTED (3)          → OK, готово до передачі
├─ WL_DISCONNECTED (6)       → Нема з'єднання
├─ WL_CONNECT_FAILED (4)     → Помилка автентифікації
├─ WL_NO_SSID_AVAIL (1)      → SSID не знайдено
├─ WL_IDLE_STATUS (0)        → Ініціалізація
└─ WL_NO_SHIELD (255)        → WiFi модуль не доступний
```

**Обробка (ваш код, лінія 56-60):**
```cpp
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Wi-Fi не підключено, синхронізація пропущена");
    syncStats.failedSyncs++;
    return false;
}
```

#### 5.1.2 SSL/TLS помилки

```
Причини помилок:
├─ Сертифікат закінчився
├─ Хост недоступний
├─ Часовий дисбаланс (NTP не синхронізовано)
├─ Недостатньо пам'яті для SSL буфера
└─ DNS помилка
```

**Обробка DNS (ваш код, лінія 195-200):**
```cpp
IPAddress serverIP;
if (!WiFi.hostByName("script.google.com", serverIP)) {
    Serial.println("❌ Помилка DNS: не вдалось розв'язати script.google.com");
    return false;
}
```

#### 5.1.3 HTTP помилки

```
Коди відповідей та значення:
├─ 200 OK             → Успіх
├─ 202 Accepted       → Прийнято (асинхронно)
├─ 302 Redirect       → Перенаправлення (OK для Google Apps Script)
├─ 400 Bad Request    → Неправильний запит
├─ 401 Unauthorized   → Потрібна автентифікація
├─ 403 Forbidden      → Доступ заборонено
├─ 408 Timeout        → Timeout сервера
├─ 429 Too Many Req.  → Rate limiting
├─ 500 Server Error   → Помилка сервера
└─ 503 Unavailable    → Сервіс недоступний
```

**Обробка (ваш код, лінія 302-309):**
```cpp
if (line.indexOf("HTTP/1.1 200") >= 0 || line.indexOf("HTTP/1.1 202") >= 0) {
    success = true;
    Serial.println("✅ HTTP 200/202 OK");
}
if (line.indexOf("HTTP/1.1 302") >= 0) {
    success = true;
    Serial.println("✅ HTTP 302 Redirect OK");
}
```

### 5.2 Стратегії обробки помилок

#### 5.2.1 Retry з експоненційною затримкою

```cpp
bool sendWithExponentialBackoff(const char* data) {
    const int MAX_RETRIES = 3;
    const int BASE_DELAY = 1000;  // 1 сек
    const int MAX_DELAY = 32000;  // 32 сек

    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Reconnecting WiFi...");
            WiFi.reconnect();
            delay(2000);
        }

        if (sendBatchToSheets(...)) {
            return true;  // Успіх
        }

        if (attempt < MAX_RETRIES - 1) {
            // Експоненційна затримка з jitter
            long delay_ms = min(BASE_DELAY * (1 << attempt), MAX_DELAY);
            long jitter = random(0, delay_ms / 4);  // ±25% jitter

            Serial.printf("Retry after %ld ms...\n", delay_ms + jitter);
            delay(delay_ms + jitter);
        }
    }

    return false;
}
```

**Логіка затримки:**
```
Спроба 1: Затримка = 1000 мс (1 сек)
Спроба 2: Затримка = 2000 мс (2 сек)
Спроба 3: Затримка = 4000 мс (4 сек)
Спроба 4: Затримка = 8000 мс (8 сек)
```

#### 5.2.2 Тайм-аути

**Ваш код (лінія 295-326):**
```cpp
unsigned long timeout = millis();
bool success = false;

// Чекаємо на відповідь від сервера
while (millis() - timeout < 15000) {  // 15 сек timeout
    if (client->available()) {
        // ... обробка відповіді ...
    }
    if (!client->connected()) {
        break;
    }
    delay(100);
    yield();
}
```

**Best Practices для тайм-аутів:**
- 5-10 сек: Для з'єднання
- 10-15 сек: Для надсилання POST
- 20-30 сек: Для Google Apps Script обробки

#### 5.2.3 Паралельні запити і Rate Limiting

```cpp
// ❌ ПОГАНО - обважувати сервер
for (int i = 0; i < 100; i++) {
    sendBatchToSheets(records[i], 1);  // Кожен окремо!
}

// ✅ ДОБРЕ - батчувати
DataRecord batch[30];
// ... накопичуємо до 30 ...
sendBatchToSheets(batch, 30);  // Один запит на 30 записів

// Google Apps Script обмеження:
// ~ 5-6 запитів в секунду від одного IP
// Ваш батчинг від 30 записів = ~1 запит щодня = OK
```

#### 5.2.4 Google Apps Script обробка помилок

```javascript
// Google Apps Script (на стороні Google)
function doPost(e) {
  try {
    var data = e.postData.contents;
    var lines = data.split("\n");

    var sheet = SpreadsheetApp.getActiveSheet();

    for (var i = 0; i < lines.length; i++) {
      if (lines[i].trim() === "") continue;

      var values = lines[i].split(";");
      sheet.appendRow(values);
    }

    // Успіх - повертаємо 200
    return HtmlService.createHtmlOutput("Success")
      .getAs("text/plain")
      .setMimeType(ContentService.MimeType.TEXT_PLAIN);

  } catch (error) {
    // Логуємо помилку
    Logger.log("Error: " + error);

    // Повертаємо 500 - ESP32 повторить
    return HtmlService.createHtmlOutput("Error: " + error)
      .setMimeType(ContentService.MimeType.TEXT_PLAIN)
      .setStatus(500);
  }
}
```

### 5.3 Логування і діагностика

**Ваш проект демонструє добру практику (лінія 73-76):**
```cpp
Serial.println("\n📤 Початок синхронізації з Google Sheets...");
Serial.printf("💾 Heap: %u, макс блок: %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
Serial.printf("🕐 Останній sequence: %lu\n", syncStats.lastSentSequence);
```

**Додаткова діагностика:**
```cpp
void printDiagnostics() {
    // Пам'ять
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Max Heap Block: %u bytes\n", ESP.getMaxAllocHeap());

    // WiFi
    Serial.printf("WiFi Signal: %d dBm\n", WiFi.RSSI());
    Serial.printf("WiFi Channel: %d\n", WiFi.channel());

    // Синхронізація
    Serial.printf("Records in RAM: %u\n", ramBufferIndex);
    Serial.printf("Synced: %u\n", syncStats.totalRecordsSent);
    Serial.printf("Failed: %u\n", syncStats.failedSyncs);

    // Час
    time_t now;
    time(&now);
    Serial.printf("Time: %s\n", ctime(&now));
}
```

---

## 6. АНАЛІЗ ВАШОГО КОДУ

### 6.1 Сильні сторони

✅ **Пам'ять оптимізована:**
- Батчинг (до 30 записів)
- Малий буфер замість всього RAM
- Свідомо звільняється пам'ять

✅ **Надійність:**
- Перевірка WiFi перед відправкою
- DNS перевірка
- HTTP статус перевірка (200, 202, 302)
- Mutex для запобігання race condition

✅ **Форматування:**
- CSV формат (економіка)
- Правильне екранування (`,` замість `.`)
- ISO формат дати
- Sequence number для дублів

✅ **Автоматизація:**
- Автосинхронізація по часу (23:59)
- Інтервальна синхронізація (30 хвилин)
- Мінімум нових записів (10)

### 6.2 Рекомендації для покращення

#### 6.2.1 Додати логування помилок

```cpp
// Поточно (лінія 176):
Serial.println("❌ Помилка відправки");

// Рекомендується додати причину:
bool sendBatchToSheets(DataRecord* records, uint16_t count) {
    // ... існуючий код ...

    if (!client->connect("script.google.com", 443)) {
        // ❌ ПОГАНО - не знаємо причину
        Serial.printf("❌ Підключення не вдалось (heap: %u)\n", ESP.getFreeHeap());

        // ✅ ДОБРЕ - більше інформації
        int errNo = errno;
        Serial.printf("❌ Connect failed (errno=%d, heap=%u, signal=%d)\n",
                     errNo, ESP.getFreeHeap(), WiFi.RSSI());

        delete client;
        return false;
    }
    // ...
}
```

#### 6.2.2 Добавити jitter до автосинхронізації

```cpp
// Поточно (лінія 360):
if (now - lastCheckTime < 60000) return;

// Рекомендується:
unsigned long CHECK_INTERVAL = 60000;
unsigned long jitter = random(0, 10000);  // 0-10 сек

if (now - lastCheckTime < CHECK_INTERVAL + jitter) return;
lastCheckTime = now - jitter;
```

**Чому?** Якщо у вас багато ESP32 - вони не будуть все одночасно синхронізуватись.

#### 6.2.3 Добавити circuit breaker

```cpp
// Якщо 5 синхронізацій поспіль не вдалось - чекаємо 1 годину
struct CircuitBreaker {
    int failureCount = 0;
    unsigned long lastFailureTime = 0;
    const int FAILURE_THRESHOLD = 5;
    const unsigned long COOLDOWN = 3600000;  // 1 час
};

bool shouldAttemptSync(CircuitBreaker& cb) {
    // Якщо в режимі cooldown - пропускаємо
    if (millis() - cb.lastFailureTime < cb.COOLDOWN) {
        if (cb.failureCount >= cb.FAILURE_THRESHOLD) {
            return false;
        }
    } else {
        // Cooldown період закінчився - скидаємо
        cb.failureCount = 0;
    }

    return true;
}
```

#### 6.2.4 Моніторинг якості з'єднання

```cpp
struct ConnectionMetrics {
    int signalStrength;        // dBm (RSSI)
    int packetLoss;           // %
    int latency;              // ms
    unsigned long uptime;     // ms
};

ConnectionMetrics getConnectionMetrics() {
    ConnectionMetrics m;
    m.signalStrength = WiFi.RSSI();  // -30 дБм (відмінно) до -80 дБм (погано)
    m.uptime = millis();

    // Якщо signalStrength < -70 dBm - розглянути переміщення маршрутизатора

    return m;
}
```

---

## 7. ПОРІВНЯННЯ З ІНШИМИ ПРОЕКТАМИ

### 7.1 Random Nerd Tutorials (ESP32 Google Sheets)

**Підхід:** HTTPClient + Google Apps Script
**Переваги:** Простота, абсолютно безплатно
**Недоліки:** Нема батчування, нема retry логіки

**Ваш проект значно краще!** ✓

### 7.2 Arduino IoT Cloud

**Підхід:** Хмарна платформа з вбудованою интеграцією
**Переваги:** Весь стек із хмари
**Недоліки:** Залежність від Arduino, потреба в інтернеті для конфігурації

**Для вашого випадку - навіщо?** ❌

### 7.3 GitHub: mobizt/ESP-Google-Sheet-Client

**Підхід:** Повна бібліотека для Google Sheets API
**Переваги:** Офіційна бібліотека, максимум функціональності
**Недоліки:** +50 KB коду, складна конфігурація, потреба в Service Account

**Для простої синхронізації - оverkill** ❌

---

## 8. РЕКОМЕНДАЦІЇ ДЛЯ ВАШОГО ПРОЕКТУ

### 8.1 Поточна архітектура (ДОБРЕ ✓)

```
┌─────────────┐
│  Датчики    │
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│  Логування      │
│  (RAM + SPIFFS) │
└────────┬────────┘
         │
         ▼
    ┌─────────────────────────────┐
    │  Google Sheets Sync         │
    │  ├─ WiFi перевірка         │
    │  ├─ Батчинг (до 30)        │
    │  ├─ CSV форматування       │
    │  └─ Retry логіка           │
    └────────────┬────────────────┘
                 │
                 ▼
        ┌─────────────────┐
        │ Google Apps     │
        │ Script          │
        └────────┬────────┘
                 │
                 ▼
        ┌─────────────────┐
        │ Google Sheets   │
        │ Spreadsheet     │
        └─────────────────┘
```

### 8.2 Що додати в майбутньому

**Пріоритет 1 (Важливо):**
1. ✨ Circuit breaker для уникання DDoS себе
2. ✨ Детальніше логування помилок
3. ✨ Метрики якості WiFi з'єднання

**Пріоритет 2 (Можливо):**
4. ✨ Компресія CSV (gzip) для великих批次
5. ✨ Локальне кешування невідправлених даних (у SPIFFS)
6. ✨ Динамічне батчування (залежить від розміру пам'яті)

**Пріоритет 3 (За потреби):**
7. ✨ Підтримка MQTT як альтернатива
8. ✨ Синхронізація з Google Sheets API (для більшої контролю)
9. ✨ Шифрування Sequence number (від tampering)

### 8.3 Готові код-сніпети для копіювання

#### Код 1: Додати circuit breaker

```cpp
// Додати у google_sheets_sync.h:
struct CircuitBreaker {
    uint16_t failureCount = 0;
    unsigned long lastFailureTime = 0;
};

static CircuitBreaker circuitBreaker = {};

// Додати у google_sheets_sync.cpp:
bool syncToGoogleSheets() {
    // Перевірка circuit breaker
    if (circuitBreaker.failureCount >= 5) {
        if (millis() - circuitBreaker.lastFailureTime < 3600000) {
            Serial.println("⚠️ Circuit breaker активний - чекаємо 1 годину");
            return false;
        }
        circuitBreaker.failureCount = 0;
    }

    // ... решта коду ...

    if (success) {
        circuitBreaker.failureCount = 0;
    } else {
        circuitBreaker.failureCount++;
        circuitBreaker.lastFailureTime = millis();
    }
}
```

#### Код 2: WiFi сигнал моніторинг

```cpp
void printWiFiDiagnostics() {
    int rssi = WiFi.RSSI();
    const char* quality;

    if (rssi >= -50) quality = "Отмично (Excellent)";
    else if (rssi >= -60) quality = "Хорошо (Good)";
    else if (rssi >= -70) quality = "Нормально (Fair)";
    else if (rssi >= -80) quality = "Плохо (Poor)";
    else quality = "Очень плохо (Very Poor)";

    Serial.printf("WiFi Signal: %d dBm (%s)\n", rssi, quality);
}
```

#### Код 3: Покращена обробка помилок

```cpp
bool sendBatchToSheets(DataRecord* records, uint16_t count) {
    if (count == 0) return true;

    // ... DNS перевірка (як в оригіналі) ...

    WiFiClientSecure* client = new WiFiClientSecure();
    if (!client) {
        Serial.println("❌ Помилка створення SSL клієнта");
        return false;
    }

    client->setInsecure();
    client->setTimeout(10000);

    // ... CSV формування ...

    // ПОКРАЩЕНА: Додаємо деталі помилки
    if (!client->connect("script.google.com", 443)) {
        Serial.printf("❌ Connect failed: heap=%u RSSI=%d ERRNO=%d\n",
                     ESP.getFreeHeap(), WiFi.RSSI(), errno);
        delete client;
        return false;
    }

    // ... решта коду ...
}
```

---

## 9. ТЕСТУВАННЯ І ВАЛІДАЦІЯ

### 9.1 Контрольний список перед production

- [ ] ✅ WiFi з'єднання стійке
- [ ] ✅ CSV формат правильний (відкрити в Google Sheets - дані читаються)
- [ ] ✅ Sequence number не повторюється
- [ ] ✅ Таймстемп синхронізований (запустити `timedatectl` на ESP32)
- [ ] ✅ Пам'ять не фрагментована (heap > 10 KB завжди)
- [ ] ✅ Retry працює (вимкнути WiFi - повинен спробувати 3 рази)
- [ ] ✅ Google Sheets отримує дані
- [ ] ✅ Serial вивід чистий (нема фрагментованого висновку)

### 9.2 Performance тести

```cpp
// Вставити у loop():
if (millis() % 60000 == 0) {  // Кожну хвилину
    Serial.printf("Memory: %u / %u bytes\n",
                 ESP.getFreeHeap(),
                 ESP.getHeapSize());

    Serial.printf("SPIFFS: %u / %u bytes\n",
                 SPIFFS.usedBytes(),
                 SPIFFS.totalBytes());
}
```

---

## 10. ЗАКЛЮЧЕННЯ

### 10.1 Ваш проект

Ваша реалізація синхронізації з Google Sheets **демонструє професійний підхід:**

1. ✅ **Методологія:** Google Apps Script - оптимальний вибір
2. ✅ **Архітектура:** Батчинг, retry логіка, mutex protection
3. ✅ **Оптимізація пам'яті:** Динамічна виділення, звільнення, контроль heap
4. ✅ **Форматування:** CSV з правильним локалізацією
5. ✅ **Надійність:** Перевірки WiFi, DNS, HTTP статусу

**Оцінка:** 8.5/10

### 10.2 Що можна покращити

1. 🔧 Додати circuit breaker
2. 🔧 Детальніше логування помилок
3. 🔧 Моніторинг якості WiFi сигналу
4. 🔧 Локальне кешування невідправлених даних

### 10.3 Best Practices які ви вже знаєте

- ✓ Батчування (не надсилаємо по 1 запису)
- ✓ Динамічна пам'ять (не глобальні масиви)
- ✓ Перевірка ресурсів (WiFi, DNS, heap)
- ✓ Мьютекси для синхронізації
- ✓ CSV форматування
- ✓ Автоматизація (scheduler)

---

## 11. ПОСИЛАННЯ

### Google Sheets API документація
- [ESP32 Data Logging to Google Sheets with Google Scripts](https://iotdesignpro.com/articles/esp32-data-logging-to-google-sheets-with-google-scripts)
- [How to Send ESP32 Sensor Data to Google Sheets (2026)](https://www.teachmemicro.com/how-to-send-esp32-sensor-data-to-google-sheets-2026/)
- [ESP32/ESP8266: Send Data to Google Sheets [2 Methods]](https://electropeak.com/learn/sending-data-from-esp32-or-esp8266-to-google-sheets-2-methods/)

### Бібліотеки та інструменти
- [mobizt/ESP-Google-Sheet-Client](https://github.com/mobizt/ESP-Google-Sheet-Client)
- [ESP32 Datalogging to Google Sheets (Google Service Account)](https://randomnerdtutorials.com/esp32-datalogging-google-sheets/)

### Пам'ять ESP32
- [Memory Types - ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/memory-types.html)
- [Minimizing RAM Usage - ESP32](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/ram-usage.html)
- [Support for External RAM - ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/external-ram.html)

### WiFi та помилки
- [ESP32 Fails to Reconnect to Wi-Fi Automatically](https://www.espboards.dev/troubleshooting/issues/wifi/esp32-wifi-reconnect-issue/)
- [Reconnect ESP32 to Wi-Fi Network After Lost Connection](https://randomnerdtutorials.com/solved-reconnect-esp32-to-wifi/)

### Retry стратегії
- [Google Apps Script Retry With Exponential Backoff](https://fargyle.medium.com/google-apps-script-retry-with-exponential-backoff-fb223ddad76d)
- [Retrying Failed Requests with Exponential Backoff](https://dev.to/abhivyaktii/retrying-failed-requests-with-exponential-backoff-48ld)

### Формати даних
- [JSON Array to Google Sheets Table / CSV](https://hooshmand.net/json-array-to-google-sheets-table-csv/)
- [How to use ArduinoJson with HTTPClient](https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/)

### FreeRTOS та Queue системи
- [ESP32 Arduino: Communication between tasks using FreeRTOS queues](https://techtutorialsx.com/2017/09/13/esp32-arduino-communication-between-tasks-using-freertos-queues/)
- [FreeRTOS Overview - ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)

---

**Підготував:** Claude Code (AI assistant)
**Останнє оновлення:** 17 січня 2026
**Версія:** 1.0
