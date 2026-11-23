# Повний аудит проєкту Klimat Kontrol (ESP32-S3)

**Дата аудиту:** 23.11.2026  
**Охоплення:** усі файли `src/*.cpp`, `src/*.h` (46 файлів) та `include/*`  
**Режим:** лише діагностика (жодних змін у C++ коді не внесено).

---

## 🔴 Рівень 1: КРИТИЧНІ БАГИ (впливають на стабільність, залізо та дані)

### 1.1. Конфлікт файлових систем `SPIFFS` vs `LittleFS` на одному розділі флешу (192 КБ)
- **Де:** `src/data_logger.cpp:43` (`SPIFFS.begin(true)`) та `src/energy_monitor.cpp:82` (`LittleFS.begin(true)`), `partitions_ota.csv:7`.
- **Суть:** Обидва модулі монтують **один і той самий** розділ флешу 192 КБ з `formatOnFail = true`. Взаємний запуск призводить до форматування розділу та втрати логів.
- **Додатково:** `LOG_FILE_MAX_SIZE = 150000` (`src/data_logger.h:15`) + архів 150 КБ фізично не влазять у розділ 192 КБ.

### 1.2. Зламана система Backup / Restore та NVS-ключі
- **Де:** `src/system_core.cpp:331-546` vs `src/config_manager.cpp:188-227, 434-476, 617-632`.
- **Суть:**
  - `system_core.cpp` і `config_manager.cpp` використовують **різні назви NVS-ключів** (`"heating"` vs `"heatingEnabled"`, `"pumpMin"` vs `"pumpMinPercent"`, `"authPass"` vs `"authPassword"` тощо).
  - Частина ключів у `config_manager.cpp` перевищує ліміт ESP-IDF NVS у **15 символів** (`"humidifierEnabled"`, `"servoClosedAngle"`, `"autoStatusEnabled"`, `"tempEmergencyLow"`) і мовчки відхиляється.
  - `restoreBackup()` викликає `prefs.clear()` і записує ключі, які `loadConfiguration()` не читає — **відновлення з бекапу стирає реальні налаштування**.
  - Статичний IP зберігається у namespace `"climate"` (`system_core.cpp:535`), а скидається з `"wifi"` (`system_core.cpp:145`).
  - `saveConfiguration()` не зберігає 8 полів адаптивного обігріву (`a_adaptive_min`, `b_start_percent` тощо), хоча `loadConfiguration()` їх читає.

### 1.3. Кліматична логіка: порівняння PWM (0..255) з відсотками (0..100) та інвертоване охолодження
- **Де:** `src/advanced_climate_logic.cpp:1490` — `bool fanAtMax = (heatingState.fanPower >= config.fanMaxPercent);`
  - `fanPower` — це PWM `0..255`, а `fanMaxPercent` — відсотки `0..100`. Вже при ~35–40% обертів система вважає, що вентилятор на максимумі, і через 2 хв починає **автоматично знижувати `config.tempMin`/`tempMax` до 18°C і перезаписувати NVS**.
- **Де:** `src/advanced_climate_logic.cpp:969-987` (`smartCoolingControl`):
  - Гілка `tempRoom < config.tempMin` (коли в кімнаті **холодніше** за мінімум) вмикає активне охолодження.
  - У рядках `:986-987` Undefined Behavior у `printf` (4 специфікатори формату, 3 аргументи). Аналогічно одиночний `%` у `:1017, 1024`.

### 1.4. Хибна валідність датчиків та ігнорування відмов датчиків
- **Де:** `src/sensor_manager.cpp:81-118, 138-172`, `src/advanced_climate_logic.cpp:859, 1555-1605`, `src/actuator_manager.cpp:361-409`.
- **Суть:**
  - При відключеному на старті датчику перші 9 циклів `errorCount < 10`, тому `roomValid`/`bmeValid`/`carrierValid` стають **`true` з нульовими даними (`0°C`, `0%`)**, запускаючи аварійний обігрів і зволожувач.
  - Лічильники помилок (`uint8_t`) не обмежені зверху — після 255 помилок (~8.5 хв) вони переповнюються в `0` і датчик знову стає `Valid = true`.
  - В аварії 220В (`advanced_climate_logic.cpp:1555`) і в `advancedHumidityControl` (`:1605`) не перевіряються `roomValid` / `bmeValid`.
  - У `sensor_manager.cpp:77` відкидається реальна температура теплоносія `85.0°C`.
  - Немає I2C таймаутів (`Wire.setTimeOut`) та перевірки `!isnan(co2)` у `src/co2_sensor.cpp:93`.

### 1.5. Аварійна зупинка, сервопривід, насос і зволожувач
- **Серво (`src/actuator_manager.cpp:274-324`):**
  - Необмежена рекурсія в `moveServoSmooth()` при зміні стану кінцевика (ризик stack overflow при стеку 4 КБ).
  - `emergencyStop()` (`src/main.cpp:411`) **НЕ закриває заслонку**, бо `moveServoSmooth()` блокує виклик без прапорця `ventState.moving`.
  - Через 2 хвилини `checkEmergencyTimeout()` (`src/system_core.cpp:724-750`) **автоматично скидає `emergencyMode = false`** після `emergencyStop()`.
- **Насос (`src/actuator_manager.cpp:151-214`):** `pumpStartupRamp()` ігнорує `config.pumpDisabled` і блокує задачу на 5 сек.
- **Зволожувач (`src/actuator_manager.cpp:361-409`):** якщо вимкнути `humidifierEnabled` під час роботи, реле залишається `HIGH`; ліміт `maxRunTime` не перевіряється; у `utility_functions.cpp:110-113` `maxHum` перезаписується.

### 1.6. Веб-інтерфейс не працює в режимі AP + критичні діри безпеки
- **AP Mode зламаний:** 8 сторінок (`handleRoot`, `/control`, `/settings`, `/wifi-settings`, `/history`, `/help`, `/debug`, `/servo`) роблять `if (WiFi.status() != WL_CONNECTED) return;` без `server.send()` — у режимі точки доступу налаштувати Wi-Fi неможливо.
- **Відсутній `checkAuth()`** на критичних ендпоінтах:
  - `POST /saveNetwork` (`src/web_pages_wifi.cpp:419`) — можна вимкнути авторизацію або змінити пароль адміна без входу.
  - `POST /save-wifi`, `GET /update`, `GET /update-simple`, `POST /servo/api`, `POST /history/clear-logs`, `GET /history/*`.
  - `POST /update` (`src/web_interface.cpp:780-792`): `handleOTAUploadResult()` викликає `ESP.restart()` навіть без авторизації.
- **XSS / ін'єкції:** неекрановані `server.arg("error")`/`success` (`web_pages_wifi.cpp:169-174`), `innerHTML` у консолі (`web_pages_main.cpp:319-329`), неекрановані мережеві поля та SSID у JS-атрибутах.
- **Непрацюючі елементи UI:**
  - Чекбокс блокування ручного режиму на `/control` шле `lock_manual_on/off` замість `lock` (`web_pages_control.cpp:259` vs `web_pages_command.cpp:86`).
  - У `/settings` поля адаптації (`adaptiveActTime` тощо) і зміни Wi-Fi (`wifi_ssid`, `wifi_password`) **не зберігаються** у `handleSaveSettings()`.
  - `POST /api/ota/password` при битому JSON встановлює пароль у `"null"` (`web_interface.cpp:699-707`).

### 1.7. Апаратні баги: DMX512, OTA, `LED_BUILTIN` та розклад світла
- **DMX512 (`src/dmx_controller.cpp:28, 50-72`):** `DmxSerial.end()` / `begin()` викликаються **10 разів/сек** (постійне перестворення UART-драйвера), стек задачі лише 2048 байт, гонка даних на `_dmxBuffer`, ігнорується `DMX_START_ADDRESS`.
- **OTA (`src/ota_manager.cpp:28-39, 312-418`):**
  - `LED_BUILTIN` (fallback GPIO 2) конфліктує з `I2C_SCL = 2` (`src/config.h:11`) і ламає I2C-шину датчиків.
  - Перевірка MD5 виконується **після** `Update.end()` і не блокує перезавантаження у биту прошивку.
  - Таймаут у `updateFromURL()` знаходиться всередині `if (available)` — при зависанні TCP-потоку цикл висить вічно.
  - Ділення на нуль у `ota_manager.cpp:110` (`progress * 100 / total`).
- **Світло (`src/light_scheduler.cpp:89-107`):** при розкладі через північ (наприклад, `22:00 → 02:00`) світло різко гасне о `00:00`; немає перевірки `keyframeCount <= LIGHT_KEYFRAME_COUNT`.
- **Логи та статистика:**
  - `data_logger.cpp:585`: до синхронізації NTP відбувається unsigned underflow `cutoffTime`, що видаляє всі архіви.
  - `energy_monitor.cpp:297-311`: кумулятивні покази kWh сумуються між собою замість різниці.

---

## 🟠 Рівень 2: ПОТЕНЦІЙНІ РИЗИКИ (зависання, пам'ять, потокобезпека)

1. **Тривалі блокування `loop()` та `webTask`:**
   - `autoSyncTask()` (`google_sheets_sync.cpp:462-508`) виконує синхронний HTTPS прямо в `loop()` (десятки секунд/хвилини блокування) і не перевіряє `isOTAInProgress()`.
   - `main.cpp:293-345`: при відсутності Wi-Fi `loop()` блокується на 10–15 сек кожні 30 сек.
   - `energy_monitor.cpp:64-68`: при відсутності 220В busy-wait на 500 мс кожні 2 сек без `yield()`.
   - `web_pages_wifi.cpp:260-274`: блокування `webTask` до 16 сек.
   - `web_pages_control.cpp:226-228`: слайдери шлють 2 POST-запити на кожен піксель руху без debounce.
2. **Потокобезпека та `vTaskSuspend()`:**
   - `suspendAllTasks()` (`system_core.cpp:756-809`) може зупинити задачу, яка тримає м'ютекс/I2C/SPIFFS (deadlock), або призупинити сам `loop()`.
   - `createTasks()` у `main.cpp:163` стартує задачі **до** завершення ініціалізації структур (`:166-201`).
   - Відсутні м'ютекси при доступі до `energyData`, `_cfg` світла, глобального `preferences` (у `light_scheduler.cpp` і `main.cpp`), `sensorData` в `data_logger.cpp` та `data_storage.cpp`.
3. **Фрагментація Heap (ризик OOM):**
   - Жоден веб-генератор (`handleSettingsPage` ~25–30 КБ, `handleEnergyPage` ~20 КБ, `handleHistoryPage`, `handleRoot`) не викликає `html.reserve()`.
   - `readSPIFFSData()` / `/history/export` читають увесь 150 КБ файл в один `String`/`JsonDocument` у RAM.
4. **Конфлікти заголовків і пінів:**
   - Папка `include/` містить застарілі дублікати `sensor_manager.h`, `global_declarations.h`, `advanced_climate_logic.h` без include guards (ризик ODR-порушень).
   - `actuator_manager.h:14-15` призначає GPIO 4/5 для реле, тоді як `config.h:12-13` використовує їх для `SCD30_I2C`.
5. **Зовнішні залежності в локальній мережі:**
   - Google Fonts `Inter` (`web_common.h:23`), `api.qrserver.com` (`web_pages_main.cpp:86`), `cdn.jsdelivr.net` для Chart.js (`web_common.h:210-213`).
6. **Хардкод секретів:** паролі Wi-Fi у `config.h`, `OTA_DEFAULT_PASSWORD` у `config_manager.h`, URL Google Sheets у `google_sheets_sync.h` (і у відкритому JS на `/energy`), відкритий `authPassword` в `exportConfigJSON()` та HTML `/wifi`.

---

## 🟡 Рівень 3: КОДУВАННЯ («КРАКОЗЯБРИ»), ШРИФТИ, КОМЕНТАРІ ТА МЕРТВИЙ КОД

### 3.1. Бите кодування UTF-8/CP1251 (Mojibake) — виявлено у 20 файлах
Найбільше пошкоджених рядків:
- **`src/system_core.cpp` — 50 рядків** (зокрема **в робочому коді** рядки `289, 307, 325` повертають `"РџРѕРјРёР»РєР° С‡Р°СЃСѓ"`, через що перевірка `if (timeStr != "Помилка часу")` у `main.cpp:471` ніколи не спрацьовує!).
- `src/advanced_climate_logic.cpp` — 14 рядків
- `src/web_pages_settings.cpp` — 7 рядків
- `src/web_pages_main.cpp` — 5 рядків, `src/web_pages_misc.cpp` — 5 рядків
- `src/web_pages_history.cpp` — 4 рядки, `src/system_core.h` — 4 рядки
- `src/sensor_manager.cpp`, `src/web_pages_control.cpp` — по 3 рядки
- Інші 11 файлів (`config_manager.*`, `energy_monitor.cpp`, `main.cpp`, `web_pages_wifi.cpp`, `web_interface.cpp` тощо) — по 1–2 рядки.
- **Неоднорідний UTF-8 BOM:** 11 файлів збережено з BOM, решта — без BOM.

### 3.2. Зайві шрифти та змішані стилі
- `src/web_common.h:23` — `@import url('https://fonts.googleapis.com/css2?family=Inter...')` (гальмує без інтернету).
- `src/web_interface.cpp:330` (`/update`) і `src/web_pages_wifi.cpp:279` — захардкоджений `font-family:Arial` та окрема темна/нестандартна тема.
- Дублювання сторінок: Wi-Fi (3 місця: `/wifi`, `/wifi-settings`, `/settings` Tab 8), OTA (3 сторінки: `/ota`, `/update`, `/update-simple`), Серво (2 місця: `/servo`, `/settings` Tab 7).

### 3.3. Зайві коментарі та мертвий код
- Понад **230 рядків** декоративних розділювачів (`// ============`) та залишків рефакторингу (`// RUS_REMOVED`, `"Скептичний Архітектор"`, `"Критика Крок 2"`).
- Збита нумерація вкладок у коментарях `src/web_pages_settings.cpp` (`tab6`/`tab7`/`tab8`/`tab9`).
- **Подвійний RAM-буфер історії:** `history[360]` (`data_storage.cpp`) + `ramBuffer[360]` (`data_logger.cpp`) дублюють ~15–20 КБ RAM.
- Мертві файли/функції: `src/config.cpp`, `src/global_declarations.cpp` (`TEMP_DIFF_CALIBRATION`, `isRoomSensorFault`), ~15 нереалізованих прототипів у `actuator_manager.h`, `system_core.h`, `web_interface.h`, невикористовувані `getChartZoomConfig()`, `encryptionTypeToString()`, `formatSPIFFS()`, `calculateConfigCRC()`, `updateModeButtons()`.
- Незакритий `</div>` у `src/web_pages_light.cpp:25,109`, подвійний `#include <Arduino.h>` у `main.cpp` і `#include <esp_task_wdt.h>` у `web_interface.cpp`.

---

## 📋 Рекомендований план виправлень (поетапно)

1. **Етап 1 — Критичне залізо та дані (найвищий пріоритет):**
   - Уніфікувати ФС на один тип (наприклад, лише `LittleFS` або лише `SPIFFS`) та виправити розміри логів під 192 КБ.
   - Полагодити NVS-ключі у `config_manager.cpp` (усі ≤ 15 символів, 1:1 із `system_core.cpp`).
   - Виправити `fanPower` (PWM vs %) та інвертовану логіку охолодження в `advanced_climate_logic.cpp`.
   - Виправити валідність датчиків на старті, overflow `uint8_t`, рекурсію серво та `emergencyStop()`.
   - Виправити конфлікт `LED_BUILTIN` (GPIO 2 / I2C) та MD5/таймаут в `ota_manager.cpp`, а також UART в `dmx_controller.cpp`.
2. **Етап 2 — Веб-інтерфейс та безпека:**
   - Полагодити роботу сайту в AP-режимі (прибрати блокування `WiFi.status() != WL_CONNECTED`).
   - Додати `checkAuth()` на `/saveNetwork`, `/save-wifi`, `/update`, `/servo/api`, `/history/*` та екранування XSS.
   - Полагодити збереження полів у `/settings` і `lock` на `/control`, додати `html.reserve()` та debounce на слайдери.
3. **Етап 3 — Гігієна коду (чистка):**
   - Виправити всі 20 файлів із битим кодуванням (включаючи `"Помилка часу"` у `system_core.cpp`) та прибрати BOM.
   - Видалити застарілі дублікати з `include/`, прибрати Google Fonts `@import`, мертвий код і зайві коментарі.
4. **Етап 4 — Новий дизайн:**
   - Інтегрувати затверджений прототип із `docs/ui_prototype.html` у `src/web_common.h`.
