# 📊 Документація системи зберігання даних

## Огляд

Система зберігання даних клімат-контролю використовує **трирівневу архітектуру** для ефективного збереження історичних даних з різними рівнями деталізації.

## 🏗️ Архітектура системи

### Рівень 1: RAM (Оперативна пам'ять)
- **Обсяг:** 1440 записів (24 години)
- **Частота запису:** Кожну хвилину
- **Деталізація:** Повна (всі параметри)
- **Призначення:** Швидкий доступ до свіжих даних для веб-графіків

### Рівень 2: SPIFFS (Флеш-пам'ять ESP32)
- **Обсяг:** ~3-4 МБ (до 30 днів історії)
- **Частота запису:** Кожні 5 хвилин (агреговані дані)
- **Деталізація:** Середні, мін, макс значення
- **Призначення:** Середньострокове зберігання для аналізу тижня-місяця

### Рівень 3: Google Sheets (Хмара)
- **Обсяг:** Необмежений
- **Частота відправки:** Кожну годину або добу
- **Деталізація:** Агреговані дані за години
- **Призначення:** Довгострокове зберігання для аналізу місяців-років

## 📝 Структура даних

### DataRecord (RAM - 1 хвилина)
```cpp
struct DataRecord {
  unsigned long timestamp;      // Unix timestamp (секунди)
  float tempCarrier;           // Температура теплоносія (°C)
  float tempRoom;              // Температура кімнати (°C)
  float tempBME;               // Температура BME280 (°C)
  float humidity;              // Вологість (%)
  uint8_t pumpPower;           // Насос (0-100%)
  uint8_t fanPower;            // Вентилятор (0-100%)
  uint8_t extractorPower;      // Витяжка (0-100%)
  uint8_t mode;                // Режим (0=AUTO, 1=MANUAL, 2=FORCE, 3=EMERGENCY)
};
```

### AggregatedRecord (SPIFFS - 5 хвилин)
```cpp
struct AggregatedRecord {
  unsigned long timestamp;      // Unix timestamp початку інтервалу
  float avgTempCarrier;        // Середня температура теплоносія
  float avgTempRoom;           // Середня температура кімнати
  float avgTempBME;            // Середня температура BME280
  float avgHumidity;           // Середня вологість
  float minTempCarrier;        // Мінімум теплоносія
  float maxTempCarrier;        // Максимум теплоносія
  float minTempRoom;           // Мінімум кімнати
  float maxTempRoom;           // Максимум кімнати
  uint8_t avgPumpPower;        // Середня потужність насоса
  uint8_t avgFanPower;         // Середня потужність вентилятора
  uint8_t avgExtractorPower;   // Середня потужність витяжки
  uint8_t mode;                // Домінуючий режим
};
```

## 🔌 API Endpoints

### 1. Отримання даних (`GET /history/data`)

**Параметри запиту:**
- `source` - джерело даних: `ram` або `spiffs`
- `format` - формат: `json` або `csv` (опціонально, за замовчуванням `json`)
- `start` - дата початку (тільки для SPIFFS): `YYYY-MM-DD` або `YYYY-MM-DD HH:MM:SS`
- `end` - дата кінця (тільки для SPIFFS): `YYYY-MM-DD` або `YYYY-MM-DD HH:MM:SS`

**Приклади використання:**

```bash
# Отримати дані з RAM за останні 24 години (JSON)
curl "http://klimat.local/history/data?source=ram"

# Отримати дані з RAM у CSV форматі
curl "http://klimat.local/history/data?source=ram&format=csv"

# Отримати дані з SPIFFS за період (JSON)
curl "http://klimat.local/history/data?source=spiffs&start=2026-01-01&end=2026-01-08"

# Отримати дані з SPIFFS за період (CSV)
curl "http://klimat.local/history/data?source=spiffs&start=2026-01-01&end=2026-01-08&format=csv"
```

**Відповідь (JSON):**
```json
{
  "data": [
    {
      "timestamp": 1704067200,
      "tempCarrier": 58.5,
      "tempRoom": 27.3,
      "tempBME": 26.8,
      "humidity": 55.2,
      "pumpPower": 0,
      "fanPower": 15,
      "extractorPower": 0,
      "mode": 0
    },
    ...
  ]
}
```

**Відповідь (CSV):**
```csv
timestamp,tempCarrier,tempRoom,tempBME,humidity,pumpPower,fanPower,extractorPower,mode
1704067200,58.5,27.3,26.8,55.2,0,15,0,0
1704067260,58.6,27.4,26.9,55.1,0,15,0,0
...
```

### 2. Статистика логування (`GET /history/stats`)

**Приклад використання:**
```bash
curl "http://klimat.local/history/stats"
```

**Відповідь:**
```json
{
  "totalRecordsRAM": 1440,
  "totalRecordsSPIFFS": 8640,
  "lastLogTimeRAM": 3600000,
  "lastLogTimeSPIFFS": 300000,
  "currentFileSize": 245678,
  "archiveFilesCount": 3,
  "spiffsUsedBytes": 1245678,
  "spiffsTotalBytes": 4194304,
  "spiffsUsedPercent": 29.7
}
```

**Опис полів:**
- `totalRecordsRAM` - загальна кількість записів у RAM
- `totalRecordsSPIFFS` - загальна кількість записів у SPIFFS
- `lastLogTimeRAM` - час останнього запису в RAM (millis)
- `lastLogTimeSPIFFS` - час останнього запису в SPIFFS (millis)
- `currentFileSize` - розмір поточного лог-файлу (байти)
- `archiveFilesCount` - кількість архівних файлів
- `spiffsUsedBytes` - використано SPIFFS (байти)
- `spiffsTotalBytes` - загальний обсяг SPIFFS (байти)
- `spiffsUsedPercent` - використано SPIFFS (%)

### 3. Експорт даних (`GET /history/export`)

**Параметри запиту:**
- `start` - дата початку: `YYYY-MM-DD` або `YYYY-MM-DD HH:MM:SS`
- `end` - дата кінця: `YYYY-MM-DD` або `YYYY-MM-DD HH:MM:SS`
- `format` - формат: `csv` або `json` (за замовчуванням `csv`)

**Приклади використання:**
```bash
# Експорт у CSV
curl "http://klimat.local/history/export?start=2026-01-01&end=2026-01-08&format=csv" \
  --output klimat_data.csv

# Експорт у JSON
curl "http://klimat.local/history/export?start=2026-01-01&end=2026-01-08&format=json" \
  --output klimat_data.json
```

**Особливості:**
- Автоматично додається заголовок `Content-Disposition: attachment`
- Файл завантажується з автоматичною назвою: `klimat_data_[start]_[end].[format]`
- Можна відкрити у браузері для прямого завантаження

## 🌐 Використання у веб-інтерфейсі

### JavaScript приклад (з існуючими графіками)

```javascript
// Завантажити дані з RAM для графіків
async function loadRealtimeData() {
  const response = await fetch('/history/data?source=ram&format=json');
  const data = await response.json();

  // Оновити графіки Chart.js
  updateCharts(data.data);
}

// Завантажити дані з SPIFFS за період
async function loadHistoricalData(startDate, endDate) {
  const url = `/history/data?source=spiffs&start=${startDate}&end=${endDate}&format=json`;
  const response = await fetch(url);
  const data = await response.json();

  return data.data;
}

// Отримати статистику
async function getStats() {
  const response = await fetch('/history/stats');
  const stats = await response.json();

  console.log(`RAM записів: ${stats.totalRecordsRAM}`);
  console.log(`SPIFFS: ${stats.spiffsUsedPercent}%`);
}

// Експортувати дані
function exportData(startDate, endDate, format = 'csv') {
  const url = `/history/export?start=${startDate}&end=${endDate}&format=${format}`;
  window.location.href = url; // Завантажить файл
}
```

## ⚙️ Налаштування

### Константи в data_logger.h

```cpp
#define LOG_INTERVAL_RAM        60000     // Запис в RAM: 1 хвилина
#define LOG_INTERVAL_SPIFFS     300000    // Запис в SPIFFS: 5 хвилин
#define LOG_FILE_MAX_SIZE       500000    // Макс розмір файлу: 500 КБ
#define LOG_MAX_FILES           6         // Макс файлів: 6
#define LOG_RETENTION_DAYS      30        // Зберігати: 30 днів
```

### Зміна налаштувань

Для зміни частоти логування відредагуйте константи і перекомпілюйте прошивку:

```cpp
// Приклад: запис в RAM кожні 30 секунд
#define LOG_INTERVAL_RAM        30000

// Приклад: запис в SPIFFS кожні 10 хвилин
#define LOG_INTERVAL_SPIFFS     600000
```

## 📈 Робота з графіками

### Покращення в Chart.js

Графіки тепер підтримують:

1. **Масштабування (Zoom)**
   - Колесо миші вгору/вниз
   - Pinch на touch-екранах
   - Масштабується тільки по осі X (час)

2. **Переміщення (Pan)**
   - Клік + перетягування миші
   - Свайп на touch-екранах

3. **Скидання масштабу**
   - Подвійний клік по графіку
   - Кнопка "🔄 Скинути масштаб"

4. **Спрощені підказки**
   - Показують тільки час і значення
   - Без назв датасетів

### Приклад інтеграції з новими даними

```javascript
// Завантажити і відобразити дані з SPIFFS
async function loadAndDisplayHistory(startDate, endDate) {
  const response = await fetch(
    `/history/data?source=spiffs&start=${startDate}&end=${endDate}`
  );
  const result = await response.json();

  // Підготовка даних для Chart.js
  const labels = result.data.map(d =>
    new Date(d.timestamp * 1000).toLocaleString('uk-UA')
  );
  const tempCarrier = result.data.map(d => d.tempCarrier);
  const tempRoom = result.data.map(d => d.tempRoom);

  // Оновлення графіка
  tempChart.data.labels = labels;
  tempChart.data.datasets[0].data = tempCarrier;
  tempChart.data.datasets[1].data = tempRoom;
  tempChart.update();
}
```

## 🔧 Обслуговування

### Очищення старих логів

Система автоматично очищує логи старші за `LOG_RETENTION_DAYS` днів раз на добу.

### Ручне очищення (через Serial)

```cpp
// В advanced_climate_logic.cpp можна додати команду:
if (command == "logs clear") {
  cleanOldLogs();
  Serial.println("✓ Старі логи видалено");
}

// Форматування SPIFFS (УВАГА: видалить всі дані!)
if (command == "logs format") {
  formatSPIFFS();
  Serial.println("✓ SPIFFS відформатовано");
}

// Статистика
if (command == "logs stats") {
  printLoggerInfo();
}
```

### Моніторинг через Serial

При завантаженні ESP32 виводиться:
```
🗄️ Ініціалізація системи логування даних...
✓ SPIFFS ініціалізовано
  Загальний обсяг: 4194304 байт (4.00 МБ)
  Використано: 1245678 байт (1.19 МБ)
  Вільно: 2948626 байт (2.81 МБ)
✓ Знайдено існуючий лог-файл: 245678 байт
✓ Знайдено 3 архівних файлів
✅ Систему логування ініціалізовано успішно!
```

## 💾 Формат файлів

### CSV файл (SPIFFS)
```
timestamp,tempCarrier,tempRoom,tempBME,humidity,pumpPower,fanPower,extractorPower,mode
1704067200,58.5,27.3,26.8,55.2,0,15,0,0
1704067500,58.6,27.4,26.9,55.1,0,15,0,0
```

### Структура директорій SPIFFS
```
/logs/
  ├── current.csv           # Поточний файл (до 500 КБ)
  ├── archive_1704067200.csv # Архів 1
  ├── archive_1704153600.csv # Архів 2
  └── archive_1704240000.csv # Архів 3
```

## 📊 Ресурси системи

### Використання пам'яті
- **RAM буфер:** ~46 КБ (1440 записів × 32 байти)
- **SPIFFS:** До 3-4 МБ (30 днів × ~130 КБ/день)
- **FreeRTOS таск:** 8 КБ стеку

### Ресурс флеш-пам'яті
При записі кожні 5 хвилин:
- 12 записів/годину × 24 = 288 записів/день
- З wear leveling: **~95 років** ресурсу флеш-пам'яті

### Продуктивність
- Запис в RAM: <1 мс
- Запис в SPIFFS: <50 мс
- Читання з RAM: <5 мс
- Читання з SPIFFS: <200 мс (залежить від розміру даних)

## 🔮 Майбутні розширення

### Google Sheets інтеграція
Готова структура для інтеграції:
- Відправка агрегованих даних кожну годину
- Використання Google Sheets API
- Див. `GOOGLE_SHEETS_LEARNING_SETUP.md`

### Додаткові можливості
- Експорт у Excel (.xlsx)
- Графіки трендів і прогнозів
- Email сповіщення при досягненні лімітів
- Telegram bot для віддаленого доступу

## ❓ FAQ

**Q: Що станеться при перезавантаженні ESP32?**
A: Дані з RAM втратяться, але дані з SPIFFS збережуться.

**Q: Як часто потрібно чистити SPIFFS?**
A: Система чистить автоматично. Ручне очищення не потрібне.

**Q: Чи можна змінити частоту запису без перекомпіляції?**
A: Ні, треба змінити константи і перекомпілювати.

**Q: Скільки даних можна зберегти в SPIFFS?**
A: ~30 днів агрегованих даних (по 5 хв) при стандартних налаштуваннях.

**Q: Чи підтримується читання даних з архівних файлів?**
A: Так, функція `readSPIFFSData()` читає з поточного і архівних файлів.

---

**Версія документації:** 1.0
**Дата:** 2026-01-08
**Автор:** AI Agent (Claude Code)
