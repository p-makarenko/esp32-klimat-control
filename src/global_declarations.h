// global_declarations.h
#ifndef GLOBAL_DECLARATIONS_H
#define GLOBAL_DECLARATIONS_H

#include <Preferences.h>
#include "learning_system.h"

// Объявления всех глобальных переменных
extern Preferences prefs;
extern LearningEntry learnings[MAX_LEARNINGS];
extern int learningCount;
extern PumpMode currentPumpMode;
extern bool learningEnabled;
extern bool isLearningActive;
extern unsigned long learningStartTime;
extern float learningStartTemp;
extern uint8_t learningCurrentFan;
extern uint8_t learningCurrentPump;

#endif