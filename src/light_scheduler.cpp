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

    // {год, хв, R, G, B, master(CH4=dimmer)}
    // 05:00 - ніч
    _cfg.keyframes[0] = {5,  0,   0,   0,   0,   0};
    // 06:00 - світанок (червоно-помаранчевий, тьмяно)
    _cfg.keyframes[1] = {6,  0,  80,  10,   0, 100};
    // 07:30 - ранок (тепло-жовтий)
    _cfg.keyframes[2] = {7, 30, 255, 100,  10, 200};
    // 10:00 - день (яскраве з синім)
    _cfg.keyframes[3] = {10, 0, 180, 180, 255, 255};
    // 14:00 - полудень (максимум)
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
            bool prevEco = false;
            uint8_t tickCounter = 0;
            while (true) {
                bool curEco = isEcoModeActive();
                // Оновлюємо кожну хвилину (12 * 5с) або миттєво при зміні стану ЕКО
                if (tickCounter == 0 || curEco != prevEco) {
                    prevEco = curEco;
                    lightSchedulerUpdate();
                }
                tickCounter = (tickCounter + 1) % 12;
                vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5000)); // перевірка ЕКО кожні 5 сек
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
    Preferences prefs;
    prefs.begin("light", true);
    size_t len = prefs.getBytesLength("cfg");
    if (len == sizeof(_cfg)) {
        prefs.getBytes("cfg", &_cfg, sizeof(_cfg));
        if (_cfg.keyframeCount > LIGHT_KEYFRAME_COUNT) {
            _cfg.keyframeCount = LIGHT_KEYFRAME_COUNT;
        }
    } else {
        lightSchedulerLoadDefaultPreset();
    }
    prefs.end();
}

// ============================================================================
// ОБЧИСЛЕННЯ RGBW ДЛЯ ЗАДАНОГО ЧАСУ
// Лінійна інтерполяція між двома сусідніми keyframe (з підтримкою переходу через 00:00)
// ============================================================================
RGBWColor lightSchedulerCalculate(uint8_t hour, uint8_t minute) {
    uint8_t count = (_cfg.keyframeCount > LIGHT_KEYFRAME_COUNT) ? LIGHT_KEYFRAME_COUNT : _cfg.keyframeCount;
    if (count == 0) return {0, 0, 0, 0};

    uint16_t nowMin = (uint16_t)hour * 60 + minute;

    int prevIdx = -1;
    int nextIdx = -1;

    for (int i = 0; i < count; i++) {
        uint16_t kMin = (uint16_t)_cfg.keyframes[i].hour * 60 + _cfg.keyframes[i].minute;
        if (kMin <= nowMin) prevIdx = i;
        else if (nextIdx == -1) nextIdx = i;
    }

    // Підтримка нічних розкладів, що перетинають північ (00:00)
    bool wrappedMidnight = false;
    if (prevIdx == -1) {
        prevIdx = count - 1;
        nextIdx = 0;
        wrappedMidnight = true;
    } else if (nextIdx == -1) {
        nextIdx = 0;
        wrappedMidnight = true;
    }

    auto& prev = _cfg.keyframes[prevIdx];
    auto& next = _cfg.keyframes[nextIdx];

    auto isOff = [](const LightKeyframe& k) {
        return k.r == 0 && k.g == 0 && k.b == 0 && k.master == 0;
    };

    // Якщо попередня точка = вимкнено → різке вмикання (тримаємо нулі до наступної)
    if (isOff(prev)) return {0, 0, 0, 0};

    // Якщо наступна точка = вимкнено → тримаємо prev без плавного переходу
    if (isOff(next)) return {prev.r, prev.g, prev.b, prev.master};

    // Обидві точки ненульові → плавна інтерполяція
    uint16_t prevMin = (uint16_t)prev.hour * 60 + prev.minute;
    uint16_t nextMin = (uint16_t)next.hour * 60 + next.minute;
    uint16_t span = 0;
    uint16_t elapsed = 0;
    if (!wrappedMidnight && nextMin >= prevMin) {
        span = nextMin - prevMin;
        elapsed = (nowMin >= prevMin) ? (nowMin - prevMin) : 0;
    } else {
        span = (1440 - prevMin) + nextMin;
        elapsed = (nowMin >= prevMin) ? (nowMin - prevMin) : ((1440 - prevMin) + nowMin);
    }

    float t = (span > 0) ? constrain((float)elapsed / span, 0.0f, 1.0f) : 1.0f;

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
    // Під час ЕКО-режиму примусово гасимо або приглушуємо DMX-світло згідно з політикою
    if (isEcoModeActive()) {
        if (config.powerSave.lightPolicy == ECO_DEV_OFF) {
            dmxSetRGBW(0, 0, 0, 0);
            return;
        }
        if (config.powerSave.lightPolicy == ECO_DEV_LIMITED) {
            uint8_t scale = constrain(config.powerSave.ecoLightScalePercent, 0, 100);
            if (_cfg.enabled) {
                struct tm* t = getTimeInfo();
                if (t != nullptr) {
                    RGBWColor color = lightSchedulerCalculate(t->tm_hour, t->tm_min);
                    uint8_t scaledMaster = (uint8_t)(((uint16_t)color.master * scale) / 100);
                    dmxSetRGBW(color.r, color.g, color.b, scaledMaster);
                }
            } else {
                RGBWColor cur = dmxGetCurrent();
                uint8_t scaledMaster = (uint8_t)(((uint16_t)cur.master * scale) / 100);
                dmxSetRGBW(cur.r, cur.g, cur.b, scaledMaster);
            }
            return;
        }
    }

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
    if (_cfg.enabled) {
        struct tm* t = getTimeInfo();
        if (t) {
            RGBWColor color = lightSchedulerCalculate(t->tm_hour, t->tm_min);
            Serial.printf("[Light] SetConfig: time=%02d:%02d R=%d G=%d B=%d M=%d\n",
                t->tm_hour, t->tm_min, color.r, color.g, color.b, color.master);
            dmxSetRGBW(color.r, color.g, color.b, color.master);
        }
    }
}
