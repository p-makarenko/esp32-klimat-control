#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include "system_core.h"
#include "utility_functions.h"  // Добавьте этот инклюд

// Змінні історії даних
extern HistoryData history[HISTORY_BUFFER_SIZE];
extern int historyIndex;
extern bool historyInitialized;

// Основні функції
void addToHistory();
void timeTask(void *parameter);

// Функції для доступу до історії
HistoryData* getHistoryBuffer();
int getHistoryIndex();
bool isHistoryInitialized();
void setHistoryInitialized(bool initialized);

#endif