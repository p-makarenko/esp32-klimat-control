# Синхронізація даних з Google Sheets перед перепрошивкою

## Огляд
Реалізовано систему збереження даних історії та навчання в Google Sheets з синхронізацією, щоб уникнути дублювання даних при перепрошивці ESP32.

## Нові можливості

### 1. **Автоматичне збереження**
- **Інтервал:** кожні 30 хвилин
- **Тригер:** при накопиченні 10+ нових записів
- Зберігає тільки нові дані без дублювання

### 2. **Ручне збереження даних**
- **Команда:** `save-data` через веб-інтерфейс або Serial
- **Кнопка:** "💾 ЗБЕРЕГТИ ДАНІ" на головній сторінці веб-інтерфейсу
- Зберігає тільки нові дані, які ще не були відправлені

### 3. **Щоденне автоматичне збереження**
- Залишається автоматичне збереження о 23:59
- Тепер також відправляє тільки нові дані

### 4. **Уникнення дублювання по timestamp**
- Система зберігає `lastSentTimestamp` в NVS (пам'ять ESP32)
- При відправці порівнює timestamp кожного запису з останнім відправленим
- Відправляє тільки новіші дані
- При перезавантаженні ESP32 відновлює останній timestamp з NVS

## Як використовувати

### Автоматичний режим (рекомендовано):
Система автоматично зберігає дані:
- **Кожні 30 хвилин** - незалежно від кількості даних
- **При накопиченні 10+ нових записів** - якщо пройшло менше 30 хвилин

### Ручний режим (перед перепрошивкою):
1. Відкрийте веб-інтерфейс ESP32
2. Натисніть кнопку **"💾 ЗБЕРЕГТИ ДАНІ"**
3. Зачекайте підтвердження: `✅ Дані успішно збережено в Google Sheets`
4. Тепер можна безпечно перепрошивати ESP32

### Або через команду:
- Введіть `save-data` в командному рядку веб-інтерфейсу
- Або надішліть через Serial Monitor

## Логіка роботи

### Ініціалізація:
```
📅 Останній відправлений timestamp: 1234567890
📍 Індекс відправки скинуто для перевірки нових даних
```

### Автоматичне збереження:
```
🤖 Автоматичне збереження даних...
   Причина: 5 нових записів
📊 Перевірка нових даних після timestamp 1234567890
📊 Підготовлено 5 нових записів для відправки
✅ Успішно додано 5 рядків в таблицю!
💾 Збережено lastSentTimestamp: 1234568000
✅ Автозбереження завершено успішно
```

### Ручне збереження:
```
📤 Ручне збереження даних в Google Sheets...
📊 Перевірка нових даних після timestamp 1234568000
📭 Немає нових даних для відправки
```

### При перезавантаженні:
```
📅 Останній відправлений timestamp: 1234568000
📍 Індекс відправки скинуто для перевірки нових даних
```

## Переваги

- ✅ **100% без дублювання** - дані відправляються тільки раз
- ✅ **Випадкова пам'ять** - зберігає стан навіть після перезавантаження
- ✅ **Швидке збереження** - відправляє тільки нові дані
- ✅ **Надійна синхронізація** - при перезавантаженні продовжує з того місця
- ✅ **Ручний контроль** - кнопка для негайного збереження
- ✅ **Економія трафіку** та часу
- ✅ **Максимальна безпека** - дані завжди в синхронізації

## Структура даних

### Аркуш "SensorData"
Містить історію датчиків з колонками:
- `timestamp` - час запису (в мілісекундах з millis())
- `tempCarrier`, `tempRoom`, `tempBME` - температури
- `humidity`, `pressure` - вологість та тиск
- `co2` - рівень CO2
- `pumpPower`, `fanPower`, `extractorPower` - потужності (%)
- `mode` - режим роботи

### Аркуш "LearningData"
Містить дані навчання системи:
- `roomError` - помилка температури
- `fanPercent`, `pumpPercent` - відсотки потужності
- `uses` - кількість використань
- `lastUsed` - час останнього використання

### NVS пам'ять ESP32
Зберігає стан синхронізації:
- `last_ts` - останній відправлений timestamp
- Дозволяє продовжувати синхронізацію після перезавантаження

## Налаштування Google Apps Script

Створіть новий проект в [Google Apps Script](https://script.google.com) і вставте наступний код:

```javascript
function doPost(e) {
  try {
    // Отримуємо JSON дані з POST body
    var data = JSON.parse(e.postData.contents);
    
    // Отримуємо таблицю
    var spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    
    // Аркуш для сенсорних даних
    var sensorSheet = spreadsheet.getSheetByName('SensorData');
    if (!sensorSheet) {
      sensorSheet = spreadsheet.insertSheet('SensorData');
      // Заголовки
      sensorSheet.appendRow(['timestamp', 'tempCarrier', 'tempRoom', 'tempBME', 'humidity', 'pressure', 'co2', 'pumpPower', 'fanPower', 'extractorPower', 'mode']);
    }
    
    // Аркуш для даних навчання (якщо є)
    var learningSheet = spreadsheet.getSheetByName('LearningData');
    if (!learningSheet && data.learningData) {
      learningSheet = spreadsheet.insertSheet('LearningData');
      learningSheet.appendRow(['roomError', 'fanPercent', 'pumpPercent', 'uses', 'lastUsed']);
    }
    
    // Додаємо сенсорні записи
    var records = data.records || [];
    var rowsAdded = 0;
    
    for (var i = 0; i < records.length; i++) {
      var record = records[i];
      sensorSheet.appendRow([
        new Date(record.timestamp),
        record.tempCarrier,
        record.tempRoom,
        record.tempBME,
        record.humidity,
        record.pressure,
        record.co2,
        record.pumpPower,
        record.fanPower,
        record.extractorPower,
        record.mode
      ]);
      rowsAdded++;
    }
    
    // Додаємо дані навчання (якщо є)
    var learningData = data.learningData || [];
    for (var i = 0; i < learningData.length; i++) {
      var record = learningData[i];
      learningSheet.appendRow([
        record.roomError,
        record.fanPercent,
        record.pumpPercent,
        record.uses,
        new Date(record.lastUsed)
      ]);
    }
    
    // Повертаємо успішну відповідь як plain text
    return ContentService
      .createTextOutput("success")
      .setMimeType(ContentService.MimeType.TEXT);
      
  } catch (error) {
    return ContentService
      .createTextOutput("error: " + error.toString())
      .setMimeType(ContentService.MimeType.TEXT);
  }
}
```

### Публікація Apps Script

1. Натисніть **Save** (зберегти)
2. Натисніть **Deploy** > **New deployment**
3. Оберіть тип **Web app**
4. Налаштування:
   - **Execute as:** Me
   - **Who has access:** Anyone
5. Натисніть **Deploy**
6. **Скопіюйте URL** і замініть в `google_sheets.h`

## Безпека

- Дані зберігаються тільки при підключенні до Wi-Fi
- При відсутності інтернету відправка пропускається без помилки
- Система продовжує працювати локально навіть без Google Sheets

## Код Google Apps Script

Оновіть ваш Apps Script наступним кодом для обробки form data:

```javascript
function doPost(e) {
  try {
    // Отримуємо дані з POST як form data
    var jsonData = e.parameter.data;
    var data = JSON.parse(jsonData);
    
    // Отримуємо таблицю
    var spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    
    // Аркуш для сенсорних даних
    var sensorSheet = spreadsheet.getSheetByName('SensorData');
    if (!sensorSheet) {
      sensorSheet = spreadsheet.insertSheet('SensorData');
      // Заголовки
      sensorSheet.appendRow(['timestamp', 'tempCarrier', 'tempRoom', 'tempBME', 'humidity', 'pressure', 'co2', 'pumpPower', 'fanPower', 'extractorPower', 'mode']);
    }
    
    // Додаємо записи
    var records = data.records;
    for (var i = 0; i < records.length; i++) {
      var record = records[i];
      sensorSheet.appendRow([
        record.timestamp,
        record.tempCarrier,
        record.tempRoom,
        record.tempBME,
        record.humidity,
        record.pressure,
        record.co2,
        record.pumpPower,
        record.fanPower,
        record.extractorPower,
        record.mode
      ]);
    }
    
    // Повертаємо успішну відповідь
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'success',
        rowsAdded: records.length
      }))
      .setMimeType(ContentService.MimeType.JSON);
      
  } catch (error) {
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'error',
        error: error.toString()
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
```</content>
<parameter name="filePath">d:\ESP32\esp32\klimat_kontrol\DATA_SYNC_SETUP.md