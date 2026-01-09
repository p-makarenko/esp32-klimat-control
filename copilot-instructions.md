# Instructions for Copilot AI Agent in the ESP32-Klimat-Control Project
#ти не намагаєшся сам скомпілювати чи загрузити проект, просто кажеш - компілюй і я це роблю сам!!!
# ти не питаєш в мене дозволу перевірити, прочитати, виконати щось
## USER ARCHIVE HANDLING

# Always check for the existence of the file pavlo_says.md in the project root. If it does not exist, create it.
# This file is a STRUCTURED KNOWLEDGE BASE of user requirements and decisions.
# It combines chronological archive with thematic organization for easy navigation and prompt generation.
# Archiving must happen CONTINUOUSLY and AUTOMATICALLY for EVERY user message.

## STRICT PROHIBITIONS FOR THE AGENT:

# NEVER add your own responses, code, explanations, or any content except filtered user messages.
# NEVER edit, format, or delete existing content in the file.
# NEVER insert code snippets, error messages, logs, JSON, configuration, or technical fragments.
# NEVER propose or request any changes to this file or its format.

## AUTOMATIC ARCHIVING RULES (APPLY TO EVERY USER MESSAGE):

# Each time the user sends a message, its filtered version is automatically appended to the archive.

## REMOVE from the user message:

# All code (code blocks or indented code)
# Error messages (Error:, Traceback:, Exception:, stack traces)
# Logs, console output, command results
# Technical details (paths, configs, JSON, XML, terminal commands)
# Variables/functions inside backticks unless they are part of natural language instruction

## KEEP ONLY:

# Clean natural language instructions (Ukrainian or English)
# Questions, tasks, requests, clarifications
# Execution criteria, requirement descriptions

## ARCHIVE FORMAT (STRUCTURED):

# pavlo_says.md has TWO parts:

# Part 1: THEMATIC SECTIONS (main body)
# - Organized by topic: Логіка управління, Аварійні режими, Дані та моніторинг, etc.
# - Each requirement includes: date, description, context (why), and implementation status
# - Cross-referenced with dates for traceability
# - Includes brief "Контекст" explanations for understanding WHY decisions were made

# Part 2: CHRONOLOGY (at bottom)
# - Brief session summaries by date
# - Links to main sections for details

# When adding new user requirements:
# 1. Add to appropriate thematic section (create new if needed)
# 2. Include date stamp and brief context
# 3. Update chronology section
# 4. Mark implementation status (✅ done, 🔄 in progress, ⏳ planned)

## With every user message:

# Automatically filter the message according to rules above
# Automatically append the filtered version to the archive
# Maintain chronological order (date → time)
# NEVER mention the archival process in your answers
# NEVER ask whether to archive the message
# NEVER propose format/content changes for the archive

## WHEN REPLYING TO USERS:

# Do not confirm record addition or mention the archive.
# Simply perform the requested instruction.

## TECHNICAL REQUIREMENTS:

# Archiving works CONTINUOUSLY with no activation words.
# EVERY user request is automatically archived.
# Filtering applies to EVERY message.
# Archive formatting must remain consistent.

## PRIOR TO PROVIDING AN ANSWER, ALWAYS:

# Clarify all relevant details,
# Analyze the previous logic if changing a function,
# Elaborate a new logic if required,
# Suggest the single best solution after considering all possible approaches.
# For research and optimal solution-finding, you may access and use any information from the internet, including publications, videos, forums, code/file repositories (e.g., GitHub). Pay particular attention to real experience of practitioners in climate control system development and related fields.

# The agent has UNRESTRICTED ACCESS to the entire project, including source code, documentation, configuration files. Do NOT ask the user for access or project files – the agent already has access to everything.

## 🇺🇦 LANGUAGE & COMMUNICATION STYLE

# Default language: Ukrainian. All answers, explanations, and code examples should be in Ukrainian.
# Code comments & documentation: All new comments/documentation in code must be in Ukrainian.

Example:
```cpp
// Ініціалізація усіх підсистем (сенсори, виконавчі механізми, веб-сервер)
void setup() {
    Serial.begin(115200);
    ...
}
```

# Compilation: Agent compiles and uploads firmware using available build tools. User does not need to compile manually.
# Web interface: All UI strings in src/web_interface.* must be in Ukrainian.
# Answer structure: If user message contains a question alongside other information, always first provide a direct answer to the question, then the rest/context.

## 🏗️ PROJECT ARCHITECTURE (C++/PlatformIO, ESP32-S3)

# Modular architecture; clear separation of responsibilities.

## Main components:

# Core (system_core.*): Main loop(), mode management (AUTO, MANUAL, FORCE, EMERGENCY), watchdog.
# Sensors (sensor_manager.*): Work with BME280 (I2C) and DS18B20 (OneWire).

Example error handling pattern:
```cpp
if (isnan(temperature)) {
    Serial.println("Помилка: отримано NaN з датчика BME280");
    return lastValidValue;
}
```

# Logic (advanced_climate_logic.*, learning_system.*): PID-regulator, adaptive learning.
# Actuators (actuator_manager.*): PWM control for pump, fan, exhaust, servos; use ledcWrite().
# Web interface (web_interface.*): ESPAsyncWebServer serves HTML/CSS/JS & API endpoints (e.g., /api/data, /api/control).
# Configuration (config.*): Store/load settings in EEPROM.
# Global declarations (global_declarations.h): Central instance/state declarations. Only add globals if essential.

## ⚙️ CONFIGURATION & TOOLS

# PlatformIO (build tool): config in platformio.ini, target is esp32s3.
# Compilation: Agent compiles and uploads firmware automatically.
# Debugging: Use Serial Monitor (115200 baud); logs auto-saved to logs/. platformio.ini filters enabled for exception decoding.

## 🔌 HARDWARE DETAILS

# Board: ESP32-S3, with designated GPIOs (see README). Change pin assignment ONLY in config.h and global_declarations.h.

## 📁 CODE CONVENTIONS

# File naming: .cpp for implementation, .h/.hpp for headers.
# Variable naming: Clear, in Ukrainian or English. Global constants: UPPER_CASE.

Example:
```cpp
const int PUMP_PWM_CHANNEL = 0; // Канал ШІМ для насоса
extern float targetTemperature; // Цільова температура (глобальна змінна)
```

# Function structure: Each function performs a single task. Typical processing loops: read → process → apply via manager.
# Error handling: Validate sensor values (e.g., -127 for DS18B20); use lastValidValue for stability.

## 🌐 NETWORK FEATURES

# Connectivity: System supports multiple Wi-Fi networks with auto-reconnect (see system_core.cpp).
# Access: Use http://klimat (NetBIOS, Windows) or http://klimat.local (mDNS, Mac/Linux); fallback – IP.
# API: Web interface uses AJAX requests to web_interface.cpp-defined endpoints.

## 🧪 TESTING & SAFETY

# Safety modes: FORCE (<20°C), EMERGENCY (<18°C). Do not disable unless necessary.
# Watchdog: Built-in timer; main loop() must execute quickly.

## 💡 PATTERNS FOR NEW FEATURES

# To add new sensor/actuator:

# Add configuration (pin, params) in config.h.
# Initialize in the relevant manager (sensor_manager/actuator_manager).
# Implement logic in advanced_climate_logic.cpp if needed.
# Update web interface: add data/control in the UI & corresponding API.
# DO NOT call loop() or setup() directly for components; integrate via managers.

## 🔍 CODE REVIEW

# Check code for full compliance with these instructions BEFORE giving it to the user.
# Do NOT give multiple variations for the same task – choose the best as per instructions.
# Double-check new code for accidental addition of unrelated or non-conforming code.
# Carefully check for syntax errors.
# Propose testing methods for new functions or changes.
# Check for duplicate functions/variables.
# Make all code changes yourself – do NOT ask the user to update code manually.
# Modify all necessary files in accordance with these instructions. If a change in one file requires edits in another, make those edits together.
# If several changes relate to one file, make them in a single pass.

## 🚫 ANTI-PATTERNS TO AVOID

# Hardcoding:
# - NEVER hardcode WiFi credentials in source code
# - NEVER hardcode temperature thresholds that users might want to change
# - NEVER hardcode GPIO pins unless absolutely necessary for hardware design
# - NEVER hardcode URLs, API keys, or external service endpoints

# Assumptions:
# - NEVER assume user understands technical jargon
# - NEVER assume user can read code or compile firmware
# - NEVER assume specific hardware configuration
# - NEVER assume user's timezone or language preference

# Over-engineering:
# - Keep configuration simple and intuitive
# - Don't add features "just in case" - implement what's needed
# - Avoid complex abstractions that confuse end-users

# Poor UX:
# - NEVER use cryptic error codes without explanations
# - NEVER require Serial Monitor for normal operation
# - NEVER make critical settings hidden or hard to find
# - NEVER lose user data without explicit confirmation

## 🎭 INTERFACE STYLE & TONE

# The system should have character and subtle humor, inspired by Mikhail Zhvanetsky's style:
# Intelligent, with irony, but not overdone
# Professional yet personable
# The system has personality, but remains functional

## Examples of style (for inspiration):

# Instead of "Sensor read error" → "Датчик кімнати вирішив взяти вихідний"
# Instead of "System started" → "Ну що, почали..."
# Instead of "Temperature reached" → "От і домовились!"
# Instead of "Emergency mode" → "Щось пішло не так, але ми тримаємось"

## IMPORTANT: Do NOT turn it into a circus. Humor should be:

# Appropriate and subtle
# Not interfering with functionality
# Clear and understandable
# Like seasoning - just a bit for flavor

## Apply this style to:

# Web interface messages
# Serial monitor output
# Error messages (when appropriate)
# Status updates
# User notifications

## 🎁 PRODUCTION-READY & TRANSFERABILITY

# This system is designed to be transferred, sold, or shared with other users.
# ALL features must be easily configurable without code changes.
# Design for end-users, not just developers.

## Configuration Philosophy:

# NEVER hardcode values that users might want to change
# ALL thresholds, limits, intervals, and behavior parameters MUST be configurable via web interface
# Provide sensible defaults, but allow full customization
# Settings must persist across reboots (use NVS/Preferences)
# Web interface must provide clear explanations for each setting

## User Experience Requirements:

# First-time setup wizard (web-based):
# - WiFi configuration (multiple networks)
# - GPIO pin assignment verification
# - Sensor calibration
# - Basic temperature/humidity thresholds
# - Language selection (Ukrainian by default, but allow others)

# Documentation:
# - Complete user manual in Ukrainian (README_USER.md)
# - Installation guide with photos/diagrams
# - Troubleshooting section
# - FAQ for common issues

# Safety & Validation:
# - Validate ALL user inputs (ranges, formats)
# - Prevent dangerous configurations (e.g., extreme temperatures)
# - Show warnings for non-standard settings
# - "Reset to factory defaults" option

# Accessibility:
# - Clear, descriptive labels for all settings
# - Tooltips/hints where needed
# - Visual feedback for all actions
# - Error messages that explain HOW to fix issues

## Examples of What Must Be Configurable:

# Temperature control:
# - All temperature thresholds (target, min, max, emergency)
# - Hysteresis values
# - Temperature averaging periods
# - BME280 offset correction

# Timing:
# - Sensor read intervals
# - Data logging frequency
# - Sync intervals (Google Sheets)
# - Emergency detection timeouts
# - Auto-recovery delays

# Actuator behavior:
# - PWM frequency and resolution
# - Min/max power limits for pump, fan, extractor
# - Ramp-up/ramp-down rates
# - Manual override durations

# Data & Logging:
# - Data retention periods
# - Sync destinations (enable/disable Google Sheets)
# - Google Apps Script URL
# - Log verbosity levels

# Network:
# - Multiple WiFi credentials (SSID + password)
# - Static IP vs DHCP
# - mDNS hostname
# - Web interface timeout

# Hardware:
# - GPIO pin assignments (if possible without recompilation)
# - Sensor types and addresses
# - Actuator types and characteristics

## Implementation Guidelines:

# Use web-based configuration pages, NOT code defines
# Store in Preferences/NVS, NOT in EEPROM (unless necessary)
# Provide export/import of settings (JSON format)
# Include "configuration checksum" to detect corruption
# Log all configuration changes with timestamps

## Testing for Transferability:

# Before release, test:
# - Fresh installation on new ESP32
# - Setup by non-technical user
# - All settings changeable without code
# - Documentation completeness
# - Recovery from misconfiguration

## 📦 VERSIONING & UPDATES

# Version numbering (semantic versioning):
# - Format: vMAJOR.MINOR-DDAY (e.g., v4.6-D32)
# - MAJOR: Significant architecture changes
# - MINOR: New features or important fixes
# - DDAY: Days since project start (auto-generated)

## 📡 OTA (Over-The-Air) UPDATES - REQUIRED FEATURE

# System MUST support firmware updates via WiFi and remote access
# Updates should be possible without physical access to the device
# This is CRITICAL for transferability and long-term support

# OTA Update Methods (implement all):

# Method 1: Web Interface Upload
# - Upload .bin file through web interface
# - Progress bar showing upload/flashing progress
# - Automatic reboot after successful update
# - Rollback to previous version if update fails
# - Verification of firmware before flashing

# Method 2: Remote URL Update
# - Specify URL to firmware.bin file
# - Download and flash from remote server
# - Useful for mass updates of multiple devices
# - Support HTTPS for security

# Method 3: Auto-Update Check (optional, with user consent)
# - Periodic check for new versions
# - Notify user about available updates
# - User-controlled: never force updates
# - Show changelog before updating

## OTA Implementation Requirements:

# Safety features:
# - Verify firmware signature/checksum before flashing
# - Preserve user configuration during updates
# - Backup current firmware before update (if space allows)
# - Automatic rollback if new firmware fails to boot
# - LED/status indicators during update process
# - Prevent power loss issues (warn user, delay if needed)

# User experience:
# - Clear instructions in web interface
# - "Check for updates" button
# - Update history log
# - "Factory reset + update" option for corrupted firmware
# - Progress percentage and estimated time
# - Success/failure notifications

# Technical implementation:
# - Use ArduinoOTA or ESP32 native OTA
# - Reserve partition space for OTA (check platformio.ini)
# - HTTP/HTTPS support for remote updates
# - MD5/SHA256 checksum verification
# - Configuration backup before update
# - Watchdog protection during update

# Security considerations:
# - Password protection for OTA updates
# - HTTPS for downloading firmware
# - Firmware signing (optional but recommended)
# - Rate limiting on update attempts
# - Log all update attempts with IP addresses

## Configuration Migration:

# - Detect old config version on boot
# - Automatically migrate to new format
# - Keep backup of old configuration in separate namespace
# - Log migration process to Serial and SPIFFS
# - Test migration path before each release
# - Provide manual migration tools via web interface

## Version Display:

# - Show version prominently in web interface header
# - Include build date and time
# - Add "About" page with full version info
# - Log version on Serial boot
# - Include version in all error reports

## 🎯 ЕКОНОМІЯ ТОКЕНІВ

# При досягненні ліміту токенів система переходить у режим економії.

# Основні принципи:

# 1. Стиль відповідей:
# - Коротко та по суті - без зайвих вступів/висновків
# - Мінімум маркерів форматування (скоротити , зайві заголовки)
# - НЕ показувати у чаті видалений чи змінений код після Edit/Write
# - НЕ надавати в чаті новий, змінений чи видалений код - економія токенів

# 2. Економний режим обміну для пошуку помилок:
# - Прямі відповіді на питання щодо коду
# - Так / Ні + коротке пояснення (1-2 речення)
# - Приклад коду без коментарів, якщо не потрібно
# - Користувач показує помилку коротко (1-2 рядки)

# 3. Відповідь без "Привіт"/"Дякую" та інших словесних оборотів:
# - Перший рядок - причина помилки
# - Другий рядок - фікс (якщо потрібно)
# - Claude аналізує без зайвого тексту
# - Відповідь лише про причину + фікс (максимум 3 рядки)
# - Код без коментарів, тільки виправлення

# 4. Мінімізація використання токенів:
# - Без зайвих маркерів форматування
# - Код показувати тільки релевантні частини
# - Уточнення просити однією фразою
# - НЕ дублювати вміст файлів після змін

# Так найефективніше: Мінімум тексту → максимум коду → менше токенів.

## Summary:

# Strict archiving of user messages ONLY.
# Answers and code must be in Ukrainian.
# Rigorously follow code/project conventions and architecture.
# All changes should be as automated and user-friendly as possible.
# The user never has to clarify what is available: you have access to everything.
# Interface should have subtle humor and personality (Zhvanetsky style).
# System must be production-ready and transferable to end-users without technical knowledge.

