// ============================================================================
// LIGHT_SCHEDULER.CPP - Емуляція природного освітлення
// ============================================================================

#include "light_scheduler.h"
#include "system_core.h"
#include <Preferences.h>

static LightScheduleConfig _cfg;
TaskHandle_t lightSchedulerTaskHandle = nullptr;

extern Preferences preferences;

// ============================================================================
// ПРЕСЕТ "ПРИРОДНЕ СОНЦЕ"
// ============================================================================
void lightSchedulerLoadDefaultPreset() {
    _cfg.enabled = true;
    _cfg.keyframeCount = 8;

    // {год, хв, R, G, B, master(CH1)}
    // 05:00 - ніч
    _cfg.keyframes[0] = {5,  0,   0,   0,   0,   0};
    // 06:00 - світанок (червоно-помаранчевий, тьмяно)
    _cfg.keyframes[1] = {6,  0,  80,  10,   0, 100};
    // 07:30 - ранок (тепло-жовтий)
    _cfg.keyframes[2] = {7, 30, 255, 100,  10, 200};
    // 10:00 - день (яскраве з синім)
    _cfg.keyframes[3] = {10, 0, 180, 180, 255, 255};
    // 14:00 - полудень (максимум білого)
    _cfg.keyframes[4] = {14, 0, 255, 255, 255, 255};
    // 18:00 - вечір (теплий жовтий)
    _cfg.keyframes[5] = {18, 0, 255,  80,   0, 220};
    // 20:00 - захід (червоний)
    _cfg.keyframes[6] = {20, 0,  60,   5,   0, 150};
    // 21:00 - ніч
    _cfg.keyframes[7] = {21, 0,   0,   0,   0,   0};
}

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================
void lightSchedulerInit() {
    lightSchedulerLoad();

    xTaskCreatePinnedToCore(
        [](void*) {
            TickType_t lastWake = xTaskGetTickCount();
            while (true) {
                lightSchedulerUpdate();
                vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(60000)); // кожну хвилину
            }
        },
        "LightSched",
        3072,
        nullptr,
        1,
        &lightSchedulerTaskHandle,
        0
    );

    Serial.println("[Light] Планувальник освітлення ініціалізовано");
}

// ============================================================================
// ЗАВАНТАЖЕННЯ / ЗБЕРЕЖЕННЯ
// ============================================================================
void lightSchedulerSave() {
    preferences.begin("light", false);
    preferences.putBytes("cfg", &_cfg, sizeof(_cfg));
    preferences.end();
}

void lightSchedulerLoad() {
    preferences.begin("light", true);
    size_t len = preferences.getBytesLength("cfg");
    if (len == sizeof(_cfg)) {
        preferences.getBytes("cfg", &_cfg, sizeof(_cfg));
    } else {
        lightSchedulerLoadDefaultPreset();
    }
    preferences.end();
}

// ============================================================================
// ОБЧИСЛЕННЯ RGBW ДЛЯ ЗАДАНОГО ЧАСУ
// Лінійна інтерполяція між двома сусідніми keyframe
// ============================================================================
RGBWColor lightSchedulerCalculate(uint8_t hour, uint8_t minute) {
    if (_cfg.keyframeCount == 0) return {0, 0, 0, 0};

    uint16_t nowMin = (uint16_t)hour * 60 + minute;

    int prevIdx = -1;
    int nextIdx = -1;

    for (int i = 0; i < _cfg.keyframeCount; i++) {
        uint16_t kMin = (uint16_t)_cfg.keyframes[i].hour * 60 + _cfg.keyframes[i].minute;
        if (kMin <= nowMin) prevIdx = i;
        else if (nextIdx == -1) nextIdx = i;
    }

    if (prevIdx == -1) return {_cfg.keyframes[0].r, _cfg.keyframes[0].g,
                               _cfg.keyframes[0].b, _cfg.keyframes[0].master};
    if (nextIdx == -1) {
        auto& last = _cfg.keyframes[_cfg.keyframeCount - 1];
        return {last.r, last.g, last.b, last.master};
    }

    auto& prev = _cfg.keyframes[prevIdx];
    auto& next = _cfg.keyframes[nextIdx];
    uint16_t prevMin = (uint16_t)prev.hour * 60 + prev.minute;
    uint16_t nextMin = (uint16_t)next.hour * 60 + next.minute;
    uint16_t span = nextMin - prevMin;
    uint16_t elapsed = nowMin - prevMin;

    float t = (span > 0) ? (float)elapsed / span : 1.0f;

    RGBWColor out;
    out.r      = (uint8_t)(prev.r      + t * ((int)next.r      - prev.r));
    out.g      = (uint8_t)(prev.g      + t * ((int)next.g      - prev.g));
    out.b      = (uint8_t)(prev.b      + t * ((int)next.b      - prev.b));
    out.master = (uint8_t)(prev.master + t * ((int)next.master - prev.master));
    return out;
}

// ============================================================================
// ОНОВЛЕННЯ (кожну хвилину)
// ============================================================================
void lightSchedulerUpdate() {
    if (!_cfg.enabled) return; // ручний режим — не чіпаємо

    struct tm* t = getTimeInfo();
    if (t == nullptr) return;

    RGBWColor color = lightSchedulerCalculate(t->tm_hour, t->tm_min);
    dmxSetRGBW(color.r, color.g, color.b, color.master);
}

// ============================================================================
// ДОСТУП ДО КОНФІГУ
// ============================================================================
LightScheduleConfig& lightSchedulerGetConfig() {
    return _cfg;
}

void lightSchedulerSetConfig(const LightScheduleConfig& cfg) {
    _cfg = cfg;
    lightSchedulerSave();
    // Застосувати відразу
    struct tm* t = getTimeInfo();
    if (t) {
        RGBWColor color = lightSchedulerCalculate(t->tm_hour, t->tm_min);
        dmxSetRGBW(color.r, color.g, color.b, color.master);
    }
}
