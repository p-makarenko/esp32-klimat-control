# Налаштування Google Sheets для збереження даних навчання

## Огляд
Система тепер підтримує збереження даних навчання в Google Sheets для запобігання втраті даних при перепрошивці ESP32.

## Необхідні зміни в Google Apps Script

### 1. Додайте підтримку дій `saveLearning` та `loadLearning`

Оновіть функцію `doPost` або `doGet` у вашому Google Apps Script:

```javascript
function doPost(e) {
  try {
    const data = JSON.parse(e.postData.contents);
    
    if (e.parameter.action === 'saveLearning') {
      return saveLearningData(data);
    }
    
    // Існуюча логіка для звичайних даних
    return saveSensorData(data);
  } catch (error) {
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'error',
        error: error.message
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

function doGet(e) {
  try {
    if (e.parameter.action === 'loadLearning') {
      return loadLearningData();
    }
    
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'error',
        error: 'Unknown action'
      }))
      .setMimeType(ContentService.MimeType.JSON);
  } catch (error) {
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'error',
        error: error.message
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
```

### 2. Додайте функції для роботи з даними навчання

```javascript
function saveLearningData(data) {
  try {
    const spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    let sheet = spreadsheet.getSheetByName('LearningData');
    
    if (!sheet) {
      sheet = spreadsheet.insertSheet('LearningData');
      // Додайте заголовки
      sheet.appendRow(['roomError', 'fanPercent', 'pumpPercent', 'uses', 'lastUsed']);
    }
    
    // Очистіть існуючі дані
    const lastRow = sheet.getLastRow();
    if (lastRow > 1) {
      sheet.deleteRows(2, lastRow - 1);
    }
    
    // Додайте нові дані
    const records = data.learningRecords;
    records.forEach(record => {
      sheet.appendRow([
        record.roomError,
        record.fanPercent,
        record.pumpPercent,
        record.uses,
        record.lastUsed
      ]);
    });
    
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'success',
        message: `Saved ${records.length} learning records`
      }))
      .setMimeType(ContentService.MimeType.JSON);
      
  } catch (error) {
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'error',
        error: error.message
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

function loadLearningData() {
  try {
    const spreadsheet = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = spreadsheet.getSheetByName('LearningData');
    
    if (!sheet) {
      return ContentService
        .createTextOutput(JSON.stringify({
          result: 'success',
          learningRecords: []
        }))
        .setMimeType(ContentService.MimeType.JSON);
    }
    
    const data = sheet.getDataRange().getValues();
    
    // Пропустіть заголовок
    const records = data.slice(1).map(row => ({
      roomError: parseFloat(row[0]) || 0,
      fanPercent: parseInt(row[1]) || 0,
      pumpPercent: parseInt(row[2]) || 0,
      uses: parseInt(row[3]) || 0,
      lastUsed: parseInt(row[4]) || 0
    }));
    
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'success',
        learningRecords: records
      }))
      .setMimeType(ContentService.MimeType.JSON);
      
  } catch (error) {
    return ContentService
      .createTextOutput(JSON.stringify({
        result: 'error',
        error: error.message
      }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
```

## Структура таблиці Google Sheets

### Аркуш "SensorData" (існуючий)
Зберігає історію сенсорів та стан обладнання.

### Аркуш "LearningData" (новий)
Зберігає дані навчання системи:
- **roomError**: Помилка температури кімнати
- **fanPercent**: Відсоток потужності вентилятора
- **pumpPercent**: Відсоток потужності насоса
- **uses**: Кількість використань цієї комбінації
- **lastUsed**: Timestamp останнього використання

## Як це працює

1. **При ініціалізації**: Якщо локальних даних навчання немає, система завантажує їх з Google Sheets
2. **При збереженні**: Дані навчання зберігаються як локально (NVS), так і в Google Sheets
3. **Синхронізація**: При старті система синхронізує локальні дані з хмарою

## Переваги

- ✅ Дані навчання не втрачаються при перепрошивці
- ✅ Резервна копія в хмарі
- ✅ Можливість аналізу даних навчання в таблиці
- ✅ Відновлення після скидання ESP32

## Налагодження

Перевірте Serial лог ESP32:
- `📚 Локальних даних навчання немає, завантажуємо з Google Sheets...`
- `✅ Завантажено X записів навчання з Google Sheets!`
- `📤 Синхронізуємо локальні дані навчання з Google Sheets...`
- `💾 Локально збережено X записів навчання`</content>
<parameter name="filePath">d:\ESP32\esp32\klimat_kontrol\GOOGLE_SHEETS_LEARNING_SETUP.md