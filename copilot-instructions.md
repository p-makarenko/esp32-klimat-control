Instructions for Copilot AI Agent in the ESP32-Klimat-Control Project
USER ARCHIVE HANDLING
Always check for the existence of the file pavlo_says.md in the project root. If it does not exist, create it.
This file is a permanent archive for USER messages ONLY during interactions with the AI agent.
Archiving must happen CONTINUOUSLY and AUTOMATICALLY for EVERY user message.
STRICT PROHIBITIONS FOR THE AGENT:
NEVER add your own responses, code, explanations, or any content except filtered user messages.
NEVER edit, format, or delete existing content in the file.
NEVER insert code snippets, error messages, logs, JSON, configuration, or technical fragments.
NEVER propose or request any changes to this file or its format.
AUTOMATIC ARCHIVING RULES (APPLY TO EVERY USER MESSAGE):
Each time the user sends a message, its filtered version is automatically appended to the archive.
REMOVE from the user message:
All code (code blocks or indented code)
Error messages (Error:, Traceback:, Exception:, stack traces)
Logs, console output, command results
Technical details (paths, configs, JSON, XML, terminal commands)
Variables/functions inside backticks unless they are part of natural language instruction
KEEP ONLY:
Clean natural language instructions (Ukrainian or English)
Questions, tasks, requests, clarifications
Execution criteria, requirement descriptions
ARCHIVE FORMAT (AUTOMATIC):
Code
[Date: YYYY-MM-DD] Session
Time: HH:MM
[Filtered user message 1]

Time: HH:MM
[Filtered user message 2]

--- (separates days)
With every user message:
Automatically filter the message according to rules above
Automatically append the filtered version to the archive
Maintain chronological order (date → time)
NEVER mention the archival process in your answers
NEVER ask whether to archive the message
NEVER propose format/content changes for the archive
WHEN REPLYING TO USERS:
Do not confirm record addition or mention the archive.
Simply perform the requested instruction.
TECHNICAL REQUIREMENTS:
Archiving works CONTINUOUSLY with no activation words.
EVERY user request is automatically archived.
Filtering applies to EVERY message.
Archive formatting must remain consistent.
PRIOR TO PROVIDING AN ANSWER, ALWAYS:
Clarify all relevant details,
Analyze the previous logic if changing a function,
Elaborate a new logic if required,
Suggest the single best solution after considering all possible approaches.
For research and optimal solution-finding, you may access and use any information from the internet, including publications, videos, forums, code/file repositories (e.g., GitHub). Pay particular attention to real experience of practitioners in climate control system development and related fields.

The agent has UNRESTRICTED ACCESS to the entire project, including source code, documentation, configuration files. Do NOT ask the user for access or project files – the agent already has access to everything.

🇺🇦 LANGUAGE & COMMUNICATION STYLE
Default language: Ukrainian. All answers, explanations, and code examples should be in Ukrainian.
Code comments & documentation: All new comments/documentation in code must be in Ukrainian.
Example:
C++
// Ініціалізація усіх підсистем (сенсори, виконавчі механізми, веб-сервер)
void setup() {
    Serial.begin(115200);
    ...
}
Compilation: Agents must not suggest ready commands for compiling/flashing (like pio run --target upload). User compiles independently. You may discuss platformio.ini configuration.
Web interface: All UI strings in src/web_interface.* must be in Ukrainian.
Answer structure: If user message contains a question alongside other information, always first provide a direct answer to the question, then the rest/context.
🏗️ PROJECT ARCHITECTURE (C++/PlatformIO, ESP32-S3)
Modular architecture; clear separation of responsibilities.
Main components:
Core (system_core.*): Main loop(), mode management (AUTO, MANUAL, FORCE, EMERGENCY), watchdog.
Sensors (sensor_manager.*): Work with BME280 (I2C) and DS18B20 (OneWire).
Example error handling pattern:
C++
if (isnan(temperature)) {
    Serial.println("Помилка: отримано NaN з датчика BME280");
    return lastValidValue;
}
Logic (advanced_climate_logic., learning_system.): PID-regulator, adaptive learning.
Actuators (actuator_manager.*): PWM control for pump, fan, exhaust, servos; use ledcWrite().
Web interface (web_interface.*): ESPAsyncWebServer serves HTML/CSS/JS & API endpoints (e.g., /api/data, /api/control).
Configuration (config.*): Store/load settings in EEPROM.
Global declarations (global_declarations.h): Central instance/state declarations. Only add globals if essential.
⚙️ CONFIGURATION & TOOLS
PlatformIO (build tool): config in platformio.ini, target is esp32s3.
Compilation: Agent does not suggest compile/upload commands.
Debugging: Use Serial Monitor (115200 baud); logs auto-saved to logs/. platformio.ini filters enabled for exception decoding.
🔌 HARDWARE DETAILS
Board: ESP32-S3, with designated GPIOs (see README). Change pin assignment ONLY in config.h and global_declarations.h.
📁 CODE CONVENTIONS
File naming: .cpp for implementation, .h/.hpp for headers.
Variable naming: Clear, in Ukrainian or English. Global constants: UPPER_CASE.
C++
const int PUMP_PWM_CHANNEL = 0; // Канал ШІМ для насоса
extern float targetTemperature; // Цільова температура (глобальна змінна)
Function structure: Each function performs a single task. Typical processing loops: read → process → apply via manager.
Error handling: Validate sensor values (e.g., -127 for DS18B20); use lastValidValue for stability.
🌐 NETWORK FEATURES
Connectivity: System supports multiple Wi-Fi networks with auto-reconnect (see system_core.cpp).
Access: Use http://klimat (NetBIOS, Windows) or http://klimat.local (mDNS, Mac/Linux); fallback – IP.
API: Web interface uses AJAX requests to web_interface.cpp-defined endpoints.
🧪 TESTING & SAFETY
Safety modes: FORCE (<20°C), EMERGENCY (<18°C). Do not disable unless necessary.
Watchdog: Built-in timer; main loop() must execute quickly.
💡 PATTERNS FOR NEW FEATURES
To add new sensor/actuator:

Add configuration (pin, params) in config.h.
Initialize in the relevant manager (sensor_manager/actuator_manager).
Implement logic in advanced_climate_logic.cpp if needed.
Update web interface: add data/control in the UI & corresponding API.
DO NOT call loop() or setup() directly for components; integrate via managers.
🔍 CODE REVIEW
Check code for full compliance with these instructions BEFORE giving it to the user.
Do NOT give multiple variations for the same task – choose the best as per instructions.
Double-check new code for accidental addition of unrelated or non-conforming code.
Carefully check for syntax errors.
Propose testing methods for new functions or changes.
Check for duplicate functions/variables.
Make all code changes yourself – do NOT ask the user to update code manually.
Modify all necessary files in accordance with these instructions. If a change in one file requires edits in another, make those edits together.
If several changes relate to one file, make them in a single pass.
Summary:

Strict archiving of user messages ONLY.
Answers and code must be in Ukrainian.
Rigorously follow code/project conventions and architecture.
All changes should be as automated and user-friendly as possible.
The user never has to clarify what is available: you have access to everything.
