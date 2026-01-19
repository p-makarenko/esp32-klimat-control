# Google Sheets Sync - ГОТОВІ КОД-СНІПЕТИ ДЛЯ КОПІЮВАННЯ

**Используйте ці коди для швидкого покращення вашого проекту без глибокого аналізу.**

## 1. CIRCUIT BREAKER (Копіюй у google_sheets_sync.h)

```cpp
// ============================================================================
// CIRCUIT BREAKER - ЗАПОБІГАННЯ DDoS СЕБЕ
// ============================================================================

struct CircuitBreaker {
    uint16_t failureCount = 0;
    unsigned long lastFailureTime = 0;
    static const int THRESHOLD = 5;
    static const unsigned long COOLDOWN_MS = 3600000;  // 1 година
};

extern CircuitBreaker syncCircuitBreaker;

bool isCircuitBreakerActive(CircuitBreaker& cb);
void recordCircuitBreakerFailure(CircuitBreaker& cb);
void resetCircuitBreakerFailure(CircuitBreaker& cb);
```

---

## 2. РЕАЛІЗАЦІЯ CIRCUIT BREAKER (Копіюй у google_sheets_sync.cpp на початок)

```cpp
// Глобальна змінна
static CircuitBreaker syncCircuitBreaker = {};

bool isCircuitBreakerActive(CircuitBreaker& cb) {
    if (cb.failureCount < cb.THRESHOLD) {
        return false;  // Не активний
    }

    unsigned long timeSinceFail = millis() - cb.lastFailureTime;
    if (timeSinceFail >= cb.COOLDOWN_MS) {
        Serial.println("🔄 Circuit breaker: охолоджування закінчилось, спробуємо знову");
        cb.failureCount = 0;
        cb.lastFailureTime = 0;
        return false;  // Можемо спробувати
    }

    unsigned long remainingMs = cb.COOLDOWN_MS - timeSinceFail;
    Serial.printf("⛔ Circuit breaker: блокований ще %lu хвилин\n", remainingMs / 60000);
    return true;  // Залишається активним
}

void recordCircuitBreakerFailure(CircuitBreaker& cb) {
    cb.failureCount++;
    cb.lastFailureTime = millis();

    if (cb.failureCount == cb.THRESHOLD) {
        Serial.printf("⚠️ УВАГА: %d невдалих синхронізацій - circuit breaker АКТИВОВАН\n",
                     cb.THRESHOLD);
    }
}

void resetCircuitBreakerFailure(CircuitBreaker& cb) {
    if (cb.failureCount > 0) {
        Serial.printf("✅ Синхронізація успішна - reset failure counter (%u -> 0)\n",
                     cb.failureCount);
    }
    cb.failureCount = 0;
}
```

---

## 3. ВИКОРИСТАННЯ CIRCUIT BREAKER (Копіюй у функцію syncToGoogleSheets)

**Знайдіть у вашому коді:**
```cpp
bool syncToGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) {
```

**Додайте ДО перевірки WiFi:**
```cpp
bool syncToGoogleSheets() {
  // ДОДАЙ ЦЕ ПЕРШИМ:
  if (isCircuitBreakerActive(syncCircuitBreaker)) {
    syncStats.failedSyncs++;
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    // ... решта коду ...
```

**Знайдіть у вашому коді:**
```cpp
  if (success) {
    syncStats.lastSentSequence = lastSeq;
```

**Додайте ДО успішної синхронізації:**
```cpp
  if (success) {
    resetCircuitBreakerFailure(syncCircuitBreaker);  // ДОДАЙ ЦЕ
    syncStats.lastSentSequence = lastSeq;
```

**Знайдіть у вашому коді:**
```cpp
  } else {
    syncStats.failedSyncs++;
    Serial.println("❌ Помилка відправки");
  }
```

**Додайте при помилці:**
```cpp
  } else {
    recordCircuitBreakerFailure(syncCircuitBreaker);  // ДОДАЙ ЦЕ
    syncStats.failedSyncs++;
    Serial.println("❌ Помилка відправки");
  }
```

---

## 4. ПОКРАЩЕНЕ ЛОГУВАННЯ (Копіюй у функцію sendBatchToSheets)

**Знайдіть:**
```cpp
  if (!client->connect("script.google.com", 443)) {
    Serial.printf("❌ Підключення не вдалось (heap: %u)\n", ESP.getFreeHeap());
```

**Замініть на:**
```cpp
  if (!client->connect("script.google.com", 443)) {
    int signal = WiFi.RSSI();
    Serial.printf("❌ SSL Connect failed:\n");
    Serial.printf("   Heap: %u bytes (max block: %u)\n",
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    Serial.printf("   WiFi Signal: %d dBm\n", signal);
    Serial.printf("   WiFi Status: %d\n", WiFi.status());
    Serial.printf("   errno: %d\n", errno);
```

---

## 5. МОНІТОРИНГ WiFi (Копіюй у функцію sendBatchToSheets ДО з'єднання)

**Додайте ДО `if (!client->connect...):`**

```cpp
  // WiFi ДІАГНОСТИКА
  Serial.println("📡 WiFi Diagnostics:");
  int rssi = WiFi.RSSI();
  Serial.printf("  Signal Strength: %d dBm\n", rssi);

  const char* signalQuality;
  if (rssi >= -50) signalQuality = "Excellent";
  else if (rssi >= -60) signalQuality = "Good";
  else if (rssi >= -70) signalQuality = "Fair";
  else if (rssi >= -80) signalQuality = "Poor";
  else signalQuality = "Very Poor";

  Serial.printf("  Quality: %s\n", signalQuality);
  Serial.printf("  Channel: %d\n", WiFi.channel());
  Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
```

---

## 6. JITTER ДЛЯ АВТОСИНХРОНІЗАЦІЇ (Копіюй у autoSyncTask)

**Знайдіть:**
```cpp
  // Перевіряємо не частіше ніж раз на хвилину
  if (now - lastCheckTime < 60000) return;
  lastCheckTime = now;
```

**Замініть на:**
```cpp
  // Перевіряємо не частіше ніж раз на хвилину (з jitter ±25%)
  static unsigned long nextCheckTime = now + 60000;
  if (now < nextCheckTime) return;

  // Додаємо jitter (±25%) щоб уникнути синхронізації всіх пристроїв одночасно
  unsigned long jitter = random(0, 15000);  // ±7.5 сек
  nextCheckTime = now + 60000 + jitter;
```

---

## 7. ДІАГНОСТИЧНА ФУНКЦІЯ (Копіюй у google_sheets_sync.cpp)

```cpp
void printSyncDiagnostics() {
    Serial.println("\n=== 🔍 ДІАГНОСТИКА СИНХРОНІЗАЦІЇ ===");

    // WiFi
    Serial.println("\n📡 WiFi:");
    Serial.printf("  Status: %d\n", WiFi.status());
    Serial.printf("  Signal: %d dBm\n", WiFi.RSSI());
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());

    // Пам'ять
    Serial.println("\n💾 Пам'ять:");
    Serial.printf("  Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("  Max Block: %u bytes\n", ESP.getMaxAllocHeap());

    // Синхронізація
    Serial.println("\n📤 Синхронізація:");
    Serial.printf("  In Progress: %s\n", syncStats.syncInProgress ? "Yes" : "No");
    Serial.printf("  Last Sent Seq: %lu\n", syncStats.lastSentSequence);
    Serial.printf("  Total Sent: %u records\n", syncStats.totalRecordsSent);
    Serial.printf("  Failed: %u times\n", syncStats.failedSyncs);

    // Circuit Breaker
    Serial.println("\n⛔ Circuit Breaker:");
    Serial.printf("  Failure Count: %u/%u\n",
                 syncCircuitBreaker.failureCount,
                 syncCircuitBreaker.THRESHOLD);
    Serial.printf("  Active: %s\n",
                 isCircuitBreakerActive(syncCircuitBreaker) ? "YES" : "No");

    Serial.println("\n====================================\n");
}
```

**Вмикати виклич у Serial командах:**
```cpp
// У web_interface.cpp або main.cpp при отриманні команди "sync_diag":
} else if (command == "sync_diag") {
    printSyncDiagnostics();
}
```

---

## 8. ЛОКАЛЬНЕ КЕШУВАННЯ (СПРОЩЕНА ВЕРСІЯ)

**Додай у google_sheets_sync.h:**

```cpp
void cacheFailedBatch(DataRecord* records, uint16_t count);
void resendCachedBatches();
```

**Додай у google_sheets_sync.cpp:**

```cpp
void cacheFailedBatch(DataRecord* records, uint16_t count) {
    // Переконуємся директорія існує
    if (!SPIFFS.exists("/cache")) {
        SPIFFS.mkdir("/cache");
    }

    // Формуємо ім'я з часом
    char filename[50];
    snprintf(filename, sizeof(filename), "/cache/batch_%lu.tmp",
             millis() / 1000);

    // Пишемо бінарний файл (економія місця)
    File file = SPIFFS.open(filename, "wb");
    if (!file) {
        Serial.printf("❌ Can't open %s\n", filename);
        return;
    }

    file.write((uint8_t*)&count, 2);
    file.write((uint8_t*)records, count * sizeof(DataRecord));
    file.close();

    Serial.printf("💾 Cached %u records to %s\n", count, filename);
}

void resendCachedBatches() {
    if (!SPIFFS.exists("/cache")) {
        return;
    }

    File root = SPIFFS.open("/cache");
    File file = root.openNextFile();

    uint16_t totalResent = 0;

    while (file) {
        if (file.name()[0] != '.') {  // Пропускаємо системні файли
            Serial.printf("📤 Resending: %s\n", file.name());
            // Читаємо лічильник
            uint16_t count = 0;
            file.read((uint8_t*)&count, 2);

            // Читаємо дані
            DataRecord* buffer = (DataRecord*)malloc(count * sizeof(DataRecord));
            if (buffer) {
                file.read((uint8_t*)buffer, count * sizeof(DataRecord));

                // Пробуємо надіслати
                if (sendBatchToSheets(buffer, count)) {
                    free(buffer);
                    String name = file.name();
                    file.close();
                    SPIFFS.remove(name);  // Видаляємо успішно надіслане
                    totalResent += count;
                } else {
                    free(buffer);
                }
            }
        }

        file = root.openNextFile();
    }

    if (totalResent > 0) {
        Serial.printf("✅ Resent %u cached records\n", totalResent);
    }
}
```

**Використання у syncToGoogleSheets:**

```cpp
  if (success) {
    // ... існуючий код ...

    // ДОДАЙ: Спробуємо надіслати кешовані дані
    resendCachedBatches();

  } else {
    recordCircuitBreakerFailure(syncCircuitBreaker);

    // ДОДАЙ: Кешуємо невідправлене
    cacheFailedBatch(sendBuffer, copied);

    syncStats.failedSyncs++;
    Serial.println("❌ Помилка відправки");
  }
```

---

## 9. ТЕСТУВАННЯ CIRCUIT BREAKER

**Додай у главну loop для тестування:**

```cpp
// ТІЛЬКИ ДЛЯ ТЕСТУВАННЯ - видаліть перед production
if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'r') {
        // 'r' - Reset circuit breaker
        syncCircuitBreaker.failureCount = 0;
        Serial.println("✅ Circuit breaker reset");
    }
    if (cmd == 't') {
        // 't' - Trigger failure
        recordCircuitBreakerFailure(syncCircuitBreaker);
        Serial.println("⚠️ Triggered failure");
    }
    if (cmd == 'd') {
        // 'd' - Print diagnostics
        printSyncDiagnostics();
    }
}
```

---

## 10. КОНТРОЛЬНИЙ СПИСОК ВПРОВАДЖЕННЯ

```
Порядок впровадження:

1. [ ] Додати Circuit Breaker структури (.h файл) - 5 хвилин
2. [ ] Додати Circuit Breaker функції (.cpp файл) - 5 хвилин
3. [ ] Інтегрувати в syncToGoogleSheets - 10 хвилин
4. [ ] Додати WiFi діагностику - 5 хвилин
5. [ ] Додати Jitter до autoSyncTask - 5 хвилин
6. [ ] Додати printSyncDiagnostics функцію - 5 хвилин
7. [ ] Тестування - 30 хвилин
8. [ ] Commit в Git - 5 хвилин

Всього: ~1.5 години
```

---

## 11. DEBUGGING TIPS

### Перевірка Circuit Breaker в Serial:

```
Типова послідовність при проблемі:
1. ❌ SSL Connect failed: (помилка з'єднання)
2. ⚠️ УВАГА: 5 невдалих синхронізацій - circuit breaker АКТИВОВАН
3. ⛔ Circuit breaker: блокований ще 60 хвилин
4. (чекати 1 годину)
5. 🔄 Circuit breaker: охолоджування закінчилось, спробуємо знову
```

### Перевірка WiFi сигналу:

```
Сила сигналу рівні:
-30 dBm: Excellent (рідко трапляється)
-50 dBm: Excellent
-60 dBm: Good (нормально)
-70 dBm: Fair (можливо проблеми)
-80 dBm: Poor (регулярні помилки)
-90 dBm: Very Poor (часті відключення)

Якщо RSSI < -75 dBm - розглянути переміщення маршрутизатора
```

---

## 12. ВИДАЛЕННЯ ТЕСТОВОГО КОДУ (ПЕРЕД PRODUCTION)

Видаліть перед production:

```cpp
// Видаліть це:
if (Serial.available()) {
    char cmd = Serial.read();
    // ... тестування кода ...
}
```

Залиште це:

```cpp
// Залиште це:
if (isCircuitBreakerActive(syncCircuitBreaker)) {
    syncStats.failedSyncs++;
    return false;
}

// Залиште це:
printSyncDiagnostics();  // Для моніторингу
```

---

## ПИТАННЯ І ВІДПОВІДІ

**В: Чи впливає Circuit Breaker на синхронізацію?**
A: Тільки блокує спроби при N невдачах на 1 годину. Потім автоматично перезавантажується.

**В: Чи буде потеря даних із Circuit Breaker?**
A: Ні, дані залишаються у RAM buffer і відправляються як тільки з'єднання сталізується.

**В: Коли вмикається Circuit Breaker?**
A: Після 5 послідовних невдалих синхронізацій.

**В: Як видалити Circuit Breaker потім?**
A: Просто видаліть вся кода, що у разділі 1-3. Проект буде роботи як раніше.

---

**Дата:** 17 січня 2026
**Версія:** 1.0
**Статус:** Готова до використання
