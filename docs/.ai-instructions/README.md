# AI Instructions Structure

Оптимізована система інструкцій для Claude AI в проекті ESP32 Klimat Control.

## 📁 Структура файлів

```
project-root/
├── .clinerules                          (~400 токенів)
│   └── Основні правила - завжди читаються
│
└── docs/
    └── .ai-instructions/
        ├── architecture.md              (~600 токенів)
        ├── code-conventions.md          (~700 токенів)
        ├── production-ready.md          (~900 токенів)
        ├── skeptical-architect.md       (~600 токенів)
        ├── anti-patterns.md             (~700 токенів)
        ├── pavlo-archive.md             (~500 токенів)
        └── README.md                    (цей файл)
```

## 🎯 Філософія

**Core принцип:** Мінімальні токени на кожен запит, детальні інструкції за потреби.

### Базове використання токенів:

- **Звичайний запит:** 400 токенів (.clinerules)
- **Робота з архітектурою:** +600 токенів (architecture.md)
- **Складна задача:** +600 токенів (skeptical-architect.md)
- **Production checklist:** +900 токенів (production-ready.md)

### Економія vs оригінальна інструкція:

**Було:** ~3000 токенів × 30 запитів = 90,000 токенів/день

**Стало:** 
- Базові запити: 400 × 20 = 8,000
- Складні запити: 1000 × 10 = 10,000
- **Всього: ~18,000 токенів/день**

**Економія: 72,000 токенів/день (80%)** 🎉

## 📋 Опис файлів

### `.clinerules` (Core)
**Розмір:** ~400 токенів  
**Читається:** Завжди  
**Зміст:**
- Контекст проекту (ESP32-S3, PlatformIO)
- Ключові принципи (production-ready, safety)
- Стиль коду (українська, camelCase)
- Правила чату (no code >3 lines)
- Критичні файли (config.h, global_declarations.h)
- Посилання на детальні інструкції

### `architecture.md`
**Розмір:** ~600 токенів  
**Коли читати:** Робота з компонентами системи  
**Зміст:**
- Структура компонентів (sensors, logic, actuators, web)
- Hardware деталі (GPIO, ESP32-S3)
- Мережева архітектура
- Data flow patterns
- Safety modes

### `code-conventions.md`
**Розмір:** ~700 токенів  
**Коли читати:** Написання/рефакторинг коду  
**Зміст:**
- Naming conventions
- Function patterns
- Error handling
- PWM setup
- Web API patterns
- Common anti-patterns

### `production-ready.md`
**Розмір:** ~900 токенів  
**Коли читати:** Додавання конфігурацій, OTA, release prep  
**Зміст:**
- Configuration requirements
- User settings management
- OTA implementation
- First-time setup wizard
- Backup/restore
- Factory reset

### `skeptical-architect.md`
**Розмір:** ~600 токенів  
**Коли читати:** Складні рішення, нові функції  
**Зміст:**
- 7-step methodology
- Critical thinking process
- Alternative approaches
- Trade-off analysis
- Testing strategy

### `anti-patterns.md`
**Розмір:** ~700 токенів  
**Коли читати:** Code review, design decisions  
**Зміст:**
- Hardcoding issues
- Assumptions to avoid
- Over-engineering examples
- UX mistakes
- Security vulnerabilities
- Performance problems

### `pavlo-archive.md`
**Розмір:** ~500 токенів  
**Коли читати:** При роботі з pavlo_says.md  
**Зміст:**
- Archive structure (thematic + chronological)
- What to filter/keep
- Entry format
- Status markers
- Context importance

## 🚀 Як Claude використовує ці файли

### Автоматично (завжди):
```
Кожен запит → читає .clinerules (400 токенів)
```

### За потреби (коли релевантно):
```
"Додай новий датчик" → architecture.md
"Як назвати цю змінну?" → code-conventions.md  
"Реалізуй OTA" → production-ready.md
"Це найкраще рішення?" → skeptical-architect.md
"Чи це good practice?" → anti-patterns.md
```

## 📊 Приклади використання

### Простий запит (400 токенів):
```
User: "Виправ typo в коментарі"
Claude: [читає тільки .clinerules]
→ 400 токенів
```

### Середній запит (1000 токенів):
```
User: "Додай валідацію температури"
Claude: [читає .clinerules + code-conventions.md]
→ 400 + 700 = 1100 токенів
```

### Складний запит (2000 токенів):
```
User: "Реалізуй OTA з backup конфігурації"
Claude: [читає .clinerules + production-ready.md + skeptical-architect.md]
→ 400 + 900 + 600 = 1900 токенів
```

### Release prep (2300 токенів):
```
User: "Підготуй систему до релізу"
Claude: [читає .clinerules + production-ready.md + anti-patterns.md]
→ 400 + 900 + 700 = 2000 токенів
```

## 🔧 Оновлення інструкцій

### Коли додавати в .clinerules:
- Критична інформація, потрібна в 90%+ запитів
- Правила, які завжди застосовуються
- Контекст проекту

### Коли створювати новий файл:
- Специфічна тема (>500 токенів деталей)
- Використовується <30% запитів
- Окрема методологія/процес

### Коли оновлювати існуючі:
- Нові патерни в коді
- Додаткові best practices
- Виправлення помилок в прикладах

## 💡 Поради

1. **Тримайте .clinerules компактним** - тільки essential info
2. **Детальні приклади** - в окремі файли
3. **Посилання** - замість дублювання
4. **Регулярний review** - видаляйте застарілі правила
5. **Вимірюйте** - якщо файл >1000 токенів, розбийте його

## 📈 Моніторинг ефективності

Періодично перевіряйте:
- Скільки токенів використовується на день
- Які файли читаються найчастіше
- Чи є дублювання між файлами
- Чи всі файли актуальні

## 🎓 Навчання нових AI агентів

При додаванні нового AI помічника:
1. Почніть з .clinerules
2. Надайте доступ до всіх .ai-instructions/
3. Поясніть структуру (цей README)
4. Тестуйте на простих → складних задачах

---

**Версія інструкцій:** 1.0  
**Дата створення:** 2026-01-11  
**Автор:** Оптимізовано для ESP32 Klimat Control project
