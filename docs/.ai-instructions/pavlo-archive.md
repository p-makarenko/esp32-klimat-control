# Pavlo Archive - User Requirements Storage

## Purpose
pavlo_says.md is a STRUCTURED KNOWLEDGE BASE of user requirements.
Combines chronological archive with thematic organization.

## File Structure

### Part 1: Thematic Sections (Main Body)
Organized by topic with cross-references.

**Standard Topics:**
- Логіка управління (Control Logic)
- Аварійні режими (Emergency Modes)
- Дані та моніторинг (Data & Monitoring)
- Веб-інтерфейс (Web Interface)
- OTA та оновлення (OTA & Updates)
- Конфігурація (Configuration)
- Інше (Other)

**Entry Format:**
```markdown
## Логіка управління

### [2026-01-10] PID регулятор для температури
**Контекст:** Потрібна точна підтримка температури ±0.5°C
**Вимога:** Реалізувати PID контролер з налаштуванням через web
**Статус:** ✅ Виконано
**Деталі:** Kp=2.0, Ki=0.5, Kd=0.1, налаштовується в реальному часі
```

### Part 2: Chronology (Bottom of File)
Brief session summaries by date.

**Format:**
```markdown
## Хронологія

### 2026-01-10
- Додано PID регулятор (→ Логіка управління)
- Налаштовано OTA оновлення (→ OTA та оновлення)
- Виправлено баг з BME280 (→ Дані та моніторинг)

### 2026-01-09
- Початок роботи над веб-інтерфейсом (→ Веб-інтерфейс)
```

---

## Archiving Rules

### AUTOMATIC ARCHIVING
Every user message is automatically filtered and archived.
NO activation words needed.
NO confirmation in responses.

### WHAT TO REMOVE (Filter Out):

**Code Blocks**
```cpp
// ❌ Don't archive
void setup() { ... }
```

**Error Messages**
```
Error: Compilation failed
Traceback: line 42
Exception in thread
```

**Technical Details**
```
/src/main.cpp
{"config": "value"}
platformio run -t upload
```

**Logs & Console Output**
```
[12345] Temperature: 22.5
DEBUG: Sensor initialized
```

**Variables in Backticks** (unless part of instruction)
```
❌ Don't archive: Check the `sensorValue` variable
✅ Archive: Make sure sensorValue is validated
```

### WHAT TO KEEP (Archive):

**Natural Language Instructions**
```
✅ "Додай перевірку температури перед увімкненням насоса"
✅ "Temperature should be validated before pump activation"
```

**Questions**
```
✅ "Чому датчик повертає -127?"
✅ "How to prevent WiFi disconnection during OTA?"
```

**Tasks & Requests**
```
✅ "Реалізуй автоматичне перепідключення WiFi"
✅ "Add Ukrainian translations to web interface"
```

**Criteria & Requirements**
```
✅ "Система повинна підтримувати 3 WiFi мережі"
✅ "Emergency mode activates below 18°C"
```

---

## Implementation Status Markers

- ✅ **Виконано** - implemented and tested
- 🔄 **В процесі** - currently working on
- ⏳ **Заплановано** - planned for future
- ❌ **Відхилено** - decided not to implement
- ⚠️ **Потребує уваги** - has issues, needs review

---

## Archive Entry Examples

### Good Entry:
```markdown
### [2026-01-10] Багатомережеве підключення WiFi
**Контекст:** Система має працювати в різних локаціях
**Вимога:** Підтримка до 5 WiFi мереж з автовибором
**Статус:** ✅ Виконано
**Деталі:** Перемикання при втраті сигналу, пріоритет по списку
```

### Bad Entry (what to avoid):
```markdown
### WiFi stuff
Added code for WiFi. Here's the implementation:
```cpp
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) { delay(500); }
```
Also fixed error: "Error: connection timeout"
Path: /src/wifi_manager.cpp line 42
```

---

## Context Field Purpose

**WHY include context:**
- Explains the reason behind decision
- Helps understand requirements later
- Prevents "why did we do this?" questions

**Good Context Examples:**
```
Контекст: Користувачі скаржились на втрату з'єднання під час OTA
Контекст: BME280 давав нестабільні показники через EMI
Контекст: Потрібна можливість віддаленого керування без доступу до пристрою
```

**Bad Context:**
```
Контекст: Це потрібно
Контекст: Павло сказав
Контекст: [порожньо]
```

---

## Special Cases

### Multi-Part Requirements
If user request touches multiple topics:

```markdown
## Веб-інтерфейс
### [2026-01-10] Сторінка налаштувань температури
**Вимога:** Slider для target temperature, показ поточної
**Статус:** ✅ Виконано

## Логіка управління  
### [2026-01-10] Зв'язок веб-інтерфейсу з PID
**Вимога:** Зміни через web одразу застосовуються до PID
**Статус:** ✅ Виконано
```

### Updates to Existing Requirements
Add new entry with reference:

```markdown
### [2026-01-11] Розширення PID налаштувань
**Контекст:** Потрібне тонке налаштування для різних умов
**Вимога:** Додати preset профілі (день/ніч/економ)
**Статус:** 🔄 В процесі
**Пов'язано:** [2026-01-10] PID регулятор
```

---

## Maintenance

### Weekly Review
- Consolidate related entries if needed
- Update statuses
- Archive completed old tasks (mark, don't delete)

### Monthly Cleanup
- Ensure cross-references are valid
- Update chronology if gaps exist
- Verify implementation statuses

---

## CRITICAL: Agent Behavior

**NEVER in Agent Responses:**
- ❌ "Я додав це до архіву"
- ❌ "Записав ваш запит"
- ❌ "Оновив pavlo_says.md"
- ❌ "Чи додати це до пам'яті?"

**Agent Should:**
- ✅ Silently filter and append
- ✅ Focus response on user's actual question
- ✅ Archive happens in background

---

## File Location
`/project-root/pavlo_says.md`

Create if doesn't exist. Never propose changes to format.

---

## Example Complete Structure

```markdown
# Pavlo's Requirements Archive

## Логіка управління

### [2026-01-10] PID регулятор
**Контекст:** Точна підтримка температури
**Вимога:** PID з web налаштуванням
**Статус:** ✅ Виконано

### [2026-01-11] Adaptive learning
**Контекст:** Оптимізація енергоспоживання
**Вимога:** Система вчиться на історії
**Статус:** 🔄 В процесі

## Веб-інтерфейс

### [2026-01-10] Українська мова
**Контекст:** Користувачі україномовні
**Вимога:** Весь UI українською
**Статус:** ✅ Виконано

---

## Хронологія

### 2026-01-11
- Adaptive learning (→ Логіка)
- WiFi fallback (→ Конфігурація)

### 2026-01-10  
- PID регулятор (→ Логіка)
- Українізація UI (→ Веб-інтерфейс)
```

---

## Summary

**Goal:** Clean, navigable knowledge base of user requirements.

**Process:** Automatic, silent, continuous.

**Result:** Easy to reference past decisions and requirements.

**Agent:** Just execute, never mention archiving.
