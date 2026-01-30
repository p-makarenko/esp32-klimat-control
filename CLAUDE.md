# ESP32 Klimat Control - AI Instructions

## ОБОВ'ЯЗКОВО: Перед кожною відповіддю

1. **Спочатку відповідай на питання** - потім контекст (правило з .clinerules.txt)
2. **Класифікуй запит** за типом (див. нижче)
3. **Завантаж потрібні інструкції** з `docs/.ai-instructions/`

## Класифікація запитів

| Тип | Ключові слова | Файли для завантаження |
|-----|---------------|------------------------|
| SIMPLE | fix, typo, rename, delete | тільки цей файл |
| CODE | add, implement, modify, function | + code-conventions.md |
| CODE_COMPLEX | algorithm, critical, adaptive, edge case | + code-conventions.md + skeptical-architect.md |
| ARCH | architecture, component, module, design | + architecture.md + skeptical-architect.md |
| PROD | config, OTA, release, production | + production-ready.md + skeptical-architect.md |
| QUALITY | review, pattern, security, anti-pattern | + code-conventions.md + skeptical-architect.md + anti-patterns.md |

## Core Rules (з .clinerules.txt)

- **Platform:** ESP32-S3, PlatformIO, C++
- **Мова:** Українська (код, UI, відповіді)
- **NO code blocks >3 lines** в чаті
- **NO permission requests** - виконуй одразу
- **Answer question first** - контекст потім
- **Token economy** - стислі відповіді

## Критичні файли

- `global_declarations.h` - центральні інстанси
- `config.h` - налаштування (HISTORY_BUFFER_SIZE!)
- `platformio.ini` - конфіг збірки

## Компіляція

Користувач компілює сам. НЕ запускати `pio run` без явного запиту.

## Детальні інструкції

Шлях: `docs/.ai-instructions/`
- architecture.md - компоненти системи
- code-conventions.md - патерни коду
- production-ready.md - конфігурація, OTA
- skeptical-architect.md - методологія якості
- anti-patterns.md - що уникати
- CLASSIFIER_RULES.md - повні правила класифікації
- ROUTER_LOGIC.md - логіка маршрутизації
