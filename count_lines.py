Import('env')
import os

def count_code_lines(node):
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
    
    # Записуємо кількість рядків у header файл
    header_content = f"""// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "learning_system.h"

// Версія та інформація про збірку (автоматично оновлюється при компіляції)
#define VERSION __DATE__ " " __TIME__
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TOTAL_CODE_LINES {total_lines}
#define SESSION_TOKENS 61313

// Оголошення всіх глобальних змінних
extern Preferences prefs;
extern LearningEntry learnings[MAX_LEARNINGS];
extern int learningCount;
extern PumpMode currentPumpMode;
extern bool learningEnabled;

// Функції для налаштувань
void initLearningPreferences();
void saveLearningPreferences();

#endif
"""
    
    with open('src/global_declarations.h', 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"Code lines counted: {total_lines}")

env.AddPreAction("buildprog", count_code_lines)
