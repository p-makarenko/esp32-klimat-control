# Instruction Loader - Classification Rules

## Purpose
Автоматично визначити тип запиту й вибрати мінімальний набір інструкцій без явної активації.

## Classification Matrix

### 1. SIMPLE - Очевидні фікси
**Токени:** 400 (тільки `.clinerules`)

**Ознаки:**
- Ключові слова: fix, typo, comment, rename, delete, polish, clarify
- Зміна: ОДНОГО файлу / одної лінії
- Складність: очевидна, логіка не змінюється
- Час реалізації: < 5 хвилин
- Приклади: "Виправ typo в сенсорі", "Додай коментар", "Переіменуй змінну"

**Умови для класифікації:**
```
IF (keyword IN [fix, typo, comment, rename, delete, polish])
   AND (scope = SINGLE_FILE OR scope = SINGLE_LINE)
   AND (complexity = OBVIOUS)
THEN SIMPLE
```

---

### 2. CODE - Стандартне розширення функціоналу
**Токени:** 1100 (`clinerules` + `code-conventions.md`)

**Ознаки:**
- Ключові слова: add, implement, modify, refactor, function, variable, logic, method, class, pattern
- Зміна: ОДНОГО компоненту (один `.cpp`/`.h` файл)
- Складність: немає алгоритму, є чіткий паттерн
- Час реалізації: 15-30 хвилин
- Приклади: "Додай LED індикатор", "Реалізуй валідацію", "Додай web endpoint"

**Умови для класифікації:**
```
IF (keyword IN [add, implement, modify, refactor, function])
   AND (scope = SINGLE_COMPONENT)
   AND (complexity < 5 files affected)
   AND (pattern_exists_in_codebase = true)
THEN CODE
```

**Файли для завантаження:**
- `.clinerules` (400)
- `code-conventions.md` (700)

---

### 3. CODE_COMPLEX - Складні алгоритми й критична логіка
**Токени:** 1700 (`clinerules` + `code-conventions.md` + `skeptical-architect.md`)

**Ознаки:**
- Ключові слова: algorithm, edge case, optimization, critical, robust, fail, safety, learning, adaptive
- Зміна: КІЛЬКОХ компонентів або новий алгоритм
- Складність: вимагає критичного мислення, trade-offs
- Час реалізації: 1-2 години
- Приклади: "Реалізуй адаптивне навчання", "Оптимізуй читання сенсорів", "Додай обробку помилок"

**Умови для класифікації:**
```
IF (keyword IN [algorithm, edge case, optimization, critical, robust])
   AND (complexity = HIGH OR affected_components >= 3)
   AND (requires_methodology = true)
THEN CODE_COMPLEX
```

**Файли для завантаження:**
- `.clinerules` (400)
- `code-conventions.md` (700)
- `skeptical-architect.md` (600)

---

### 4. ARCH - Архітектурні рішення & дизайн системи
**Токени:** 1600 (`clinerules` + `architecture.md` + `skeptical-architect.md`)

**Ознаки:**
- Ключові слова: architecture, component, module, interface, flow, diagram, dependency, design, system, structure
- Зміна: СИСТЕМНІ рівень
- Масштаб: впливає на кількох компонентів
- Час реалізації: 2-4 години (дизайн + реалізація)
- Приклади: "Як інтегрувати енергомонітор?", "Перепроектуй архітектуру", "Додай новий компонент"

**Умови для класифікації:**
```
IF (keyword IN [architecture, component, module, interface, flow])
   AND (scope = SYSTEM_LEVEL OR new_component = true)
   AND (affects_multiple_components = true)
THEN ARCH
```

**Файли для завантаження:**
- `.clinerules` (400)
- `architecture.md` (600)
- `skeptical-architect.md` (600)

---

### 5. PROD - Production-ready & вивід в продакшн
**Токени:** 1900 (`clinerules` + `production-ready.md` + `skeptical-architect.md`)

**Ознаки:**
- Ключові слова: config, OTA, release, setup, wizard, backup, production, emergency, safety, user, deploy
- Зміна: USER-FACING функціонал
- Вимоги: transferability, configuration, safety
- Час реалізації: 3-5 годин
- Приклади: "Підготуй релізу v5.0", "Реалізуй OTA", "Додай конфігурацію"

**Умови для класифікації:**
```
IF (keyword IN [config, OTA, release, setup, backup, production])
   AND (user_facing = true OR production_critical = true)
   AND (requires_configuration = true)
THEN PROD
```

**Файли для завантаження:**
- `.clinerules` (400)
- `production-ready.md` (900)
- `skeptical-architect.md` (600)

---

### 6. QUALITY - Code review & best practices
**Токени:** 2400 (максимум - `clinerules` + `code-conventions.md` + `skeptical-architect.md` + `anti-patterns.md`)

**Ознаки:**
- Ключові слова: review, pattern, practice, anti-pattern, security, performance, test, quality, best, smell
- Зміна: ОЦІНКА та ВДОСКОНАЛЕННЯ існуючого коду
- Вимоги: критичне мислення,評估найкращих практик
- Час реалізації: 2-3 години
- Приклади: "Перевір цей код", "Знайди anti-patterns", "Як покращити?", "Security review"

**Умови для класифікації:**
```
IF (keyword IN [review, pattern, anti-pattern, security, performance, quality])
   AND (intent = EVALUATE_OR_IMPROVE)
   AND (scope = CODE_EVALUATION)
THEN QUALITY
```

**Файли для завантаження:**
- `.clinerules` (400)
- `code-conventions.md` (700)
- `skeptical-architect.md` (600)
- `anti-patterns.md` (700)

---

## Scoring Algorithm

### 1. Keyword Matching
```
confidence = (matched_keywords / total_keywords) × keyword_weight
```

**Приклад для CODE запиту:**
- Запит: "Додай валідацію температури в web інтерфейс"
- Ключові слова: ["add", "validate", "web"] → CODE keywords
- Точність: 3/3 = 1.0 (100%)

### 2. Scope Assessment
```
scope_modifier =
  + 0.3 if single_component
  + 0.2 if multi_component
  - 0.2 if system_wide
```

### 3. Complexity Heuristic
```
complexity = (lines_affected × new_dependencies × algorithm_complexity) / stability_score

IF complexity < 5 THEN CODE
IF complexity 5-20 THEN CODE_COMPLEX
IF complexity > 20 THEN ARCH
```

### 4. Final Confidence
```
final_confidence = (keyword_confidence × 0.5) + (scope_confidence × 0.3) + (complexity_confidence × 0.2)

Результат: 0.0-1.0
- > 0.85: HIGH confidence → Execute immediately
- 0.65-0.85: MEDIUM confidence → Can execute, verify later
- < 0.65: LOW confidence → Ask for clarification
```

---

## Примеры классификации

### Пример 1: SIMPLE (Confidence: 0.95)
```
User: "Виправ typo в назві функції temperature -> temperaturee"

Keywords matched: ["fix", "typo"]
Scope: SINGLE_FILE (global_declarations.h)
Complexity: OBVIOUS (rename)

CLASSIFY: SIMPLE
Files: .clinerules (400 tokens)
Confidence: 0.95 (HIGH)
```

### Пример 2: CODE (Confidence: 0.88)
```
User: "Додай LED індикатор для відображення статусу насоса"

Keywords matched: ["add", "indicator", "status"]
Scope: SINGLE_COMPONENT (new led_indicator.cpp)
Complexity: MEDIUM (5 functions, known patterns)

CLASSIFY: CODE
Files: .clinerules + code-conventions.md (1100 tokens)
Confidence: 0.88 (HIGH)
```

### Пример 3: CODE_COMPLEX (Confidence: 0.92)
```
User: "Реалізуй адаптивне навчання для PID регулятора"

Keywords matched: ["algorithm", "adaptive", "learning"]
Scope: MULTI_COMPONENT (learning_system.cpp + climate_logic.cpp)
Complexity: HIGH (new algorithm, edge cases, testing)

CLASSIFY: CODE_COMPLEX
Files: .clinerules + code-conventions.md + skeptical-architect.md (1700 tokens)
Confidence: 0.92 (HIGH)
```

### Пример 4: PROD (Confidence: 0.91)
```
User: "Підготуй систему до релізу v5.0 з OTA та backup"

Keywords matched: ["release", "OTA", "backup"]
Scope: SYSTEM_WIDE (production requirements)
Complexity: HIGH (multiple configurations, safety-critical)

CLASSIFY: PROD
Files: .clinerules + production-ready.md + skeptical-architect.md (1900 tokens)
Confidence: 0.91 (HIGH)
```

### Пример 5: QUALITY (Confidence: 0.87)
```
User: "Перевір цей PID код на anti-patterns та безпеку"

Keywords matched: ["review", "anti-pattern", "security"]
Scope: CODE_EVALUATION
Complexity: EVALUATION (analyze existing code)

CLASSIFY: QUALITY
Files: .clinerules + code-conventions.md + skeptical-architect.md + anti-patterns.md (2400 tokens)
Confidence: 0.87 (HIGH)
```

### Пример 6: AMBIGUOUS (Confidence: 0.64)
```
User: "Зроби систему кращою"

Keywords matched: [] (generic)
Scope: UNKNOWN
Complexity: UNKNOWN

CLASSIFY: UNKNOWN (Confidence: 0.64 - LOW)
Action: Ask for clarification
Response: "Що саме ви хочете поліпшити? Навести приклад."
```

---

## Fallback Strategy

**Якщо confidence < 0.65:**
1. Запитати уточнення у користувача
2. Умовно: "Це означає...?" з варіантами
3. Після уточнення: re-classify з новою інформацією

**Якщо classification вийшло неправильно:**
1. Користувач може явно сказати: "Застосуй метод скептичного архітектора"
2. Тоді агент завантажить CODE_COMPLEX / QUALITY файли

---

## Usage in Practice

**Для Claude instructions (implicit):**

```
CLASSIFICATION ENGINE:

1. Read user request
2. Extract keywords
3. Calculate confidence using algorithm above
4. If confidence > 0.65:
   - Select instruction files automatically
   - Load context silently
   - Execute request with full methodology
5. If confidence <= 0.65:
   - Ask user for clarification
   - Re-classify after response
6. NEVER mention classification
   - Respond with solution
   - Let quality speak for itself
```

---

## Maintenance

**Update rules when:**
- Adding new instruction files
- Discovering new patterns in requests
- Token budget changes
- New project requirements emerge

**Never modify:**
- Core confidence algorithm
- Token allocations (unless system-wide change)
- Classification types (only add new ones if needed)
