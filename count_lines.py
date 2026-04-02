Import('env')
import os
from datetime import datetime

# Підрахунок рядків коду
src_dir = "src"
total_lines = 0

for root, dirs, files in os.walk(src_dir):
    for file in files:
        if file.endswith(('.cpp', '.h')):
            filepath = os.path.join(root, file)
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    total_lines += len(f.readlines())
            except:
                pass

# Підрахунок днів розробки
project_start = datetime(2025, 12, 7)
current_date = datetime.now()
days_in_dev = (current_date - project_start).days

# Версія v5.0 - реліз з OTA та backup
# major_version = 5 (фіксована після релізу v5.0)
major_version = 5
minor_version = 0
version_str = f"v{major_version}.{minor_version}-D{days_in_dev}"
version_comment = f"v5.0 Release: OTA + Backup (День {days_in_dev})"

# Отримуємо розмір прошивки
firmware_path = ".pio/build/esp32s3/firmware.bin"
firmware_size_kb = 0
if os.path.exists(firmware_path):
    firmware_size_kb = os.path.getsize(firmware_path) // 1024

# Записуємо у header файл
header_content = f"""// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "system_core.h"

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define PROJECT_START_DATE "Dec 7 2025"
#define VERSION "{version_str}"
#define VERSION_COMMENT "{version_comment}"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES {total_lines}
#define FIRMWARE_SIZE_KB {firmware_size_kb}

// Версіонування конфігурації (для міграції)
#define CONFIG_VERSION_MAJOR {major_version}
#define CONFIG_VERSION_MINOR {minor_version}

// Оголошення всіх глобальних змінних

#endif
"""

with open('src/global_declarations.h', 'w', encoding='utf-8') as f:
    f.write(header_content)

print(f"[OK] Ryadkiv kodu: {total_lines}")
print(f"[OK] Versiya: {version_str}")
print(f"[OK] Den rozrobky: {days_in_dev}")
if firmware_size_kb > 0:
    print(f"[OK] Rozmir proshyvky: {firmware_size_kb} KB")
