# 📚 Документація ESP32 Klimat Control

## Структура документації

### 🤖 `.ai-instructions/` - Інструкції для Claude Code
Конфігурація поведінки AI асистента:
- `architecture.md` - архітектура системи, memory management
- `code-conventions.md` - стандарти коду
- `production-ready.md` - вимоги до продакшн-коду
- `skeptical-architect.md` - методологія перевірки якості
- `anti-patterns.md` - що не треба робити
- `pavlo-archive.md` - правила архівування

### 🔧 `setup/` - Інструкції з налаштування
- `GOOGLE_APPS_SCRIPT.md` - налаштування Google Sheets sync
- `GOOGLE_SHEETS_SYNC_GUIDE.md` - повний гайд синхронізації
- `GOOGLE_SHEETS_LEARNING_SETUP.md` - інтеграція з learning system
- `NETWORK_SETUP.md` - налаштування WiFi
- `ANDROID_ACCESS.md` - доступ з Android
- `INTERNET_ACCESS.md` - віддалений доступ
- `DATA_SYNC_SETUP.md` - синхронізація даних
- `ENERGY_MONITOR_SETUP.md` - PZEM-004T енергомонітор

### 🔬 `research/` - Дослідження та аналіз
- `RESEARCH_GOOGLE_SHEETS_SYNC.md` - дослідження Google Sheets
- `RESEARCH_INDEX.md` - індекс досліджень

### 📋 `logs/` - Логи виконаних робіт
- `ENERGY_INTEGRATION_LOG.md` - інтеграція енергоконтролера

## Файли в корені проєкту

### Основні
- `README.md` - головна документація проєкту
- `.clinerules.txt` - основні правила для Claude Code

### Довідкові
- `QUICK_ACCESS.md` - швидкий доступ до системи
- `DATA_STORAGE_API.md` - API для роботи з даними
- `TOKEN_USAGE.md` - використання токенів AI
- `RECOMMENDATIONS.md` - рекомендації
- `QUICK_COPY_PASTE_CODE.md` - готові сніпети
- `copilot-instructions.md` - інструкції для GitHub Copilot
- `pavlo_says.md` - архів користувацьких запитів

## Навігація

**Для початку роботи:**
1. Прочитайте [головний README](../README.md)
2. Налаштуйте WiFi: [NETWORK_SETUP](setup/NETWORK_SETUP.md)
3. Налаштуйте Google Sheets: [GOOGLE_APPS_SCRIPT](setup/GOOGLE_APPS_SCRIPT.md)

**Для розробників:**
1. Ознайомтесь з [architecture.md](.ai-instructions/architecture.md)
2. Дотримуйтесь [code-conventions.md](.ai-instructions/code-conventions.md)
3. Перевіряйте [anti-patterns.md](.ai-instructions/anti-patterns.md)

**Для Claude Code:**
- Основні правила: [../.clinerules.txt](../.clinerules.txt)
- Детальні інструкції: `.ai-instructions/*.md`
