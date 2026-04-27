// ============================================================================
// PLANT_FAN.CPP - Керування вентилятором обдуву рослин
// ============================================================================
// Логіка: таймер визначає КОЛИ працює, breezeEnabled — ЯК працює під час ON

#include "plant_fan.h"
#include "system_core.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// СТАН (runtime)
// ============================================================================
static uint8_t       currentPowerPct  = 0;
static BreezePhase   breezePhase      = BREEZE_CALM;

static bool          timerState         = false;
static unsigned long timerPhaseStart   = 0;
static bool          breezeResetNeeded = false;  // скинути фазу при зміні конфігу

static unsigned long breezePhaseStart = 0;
static unsigned long breezePhaseDur   = 0;
static uint8_t       breezeBasePct    = 0;
static uint8_t       breezeTargetPct  = 0;
static uint8_t       breezeStartPct   = 0;

// ============================================================================
// ДОПОМІЖНІ
// ============================================================================
static uint32_t randRange(uint32_t minVal, uint32_t maxVal) {
    if (minVal >= maxVal) return minVal;
    return minVal + (esp_random() % (maxVal - minVal + 1));
}

static uint8_t lerp8(uint8_t from, uint8_t to, uint32_t elapsed, uint32_t duration) {
    if (elapsed >= duration) return to;
    return from + (int32_t)(to - from) * (int32_t)elapsed / (int32_t)duration;
}

static void applyPower(uint8_t percent) {
    if (percent > 0 && percent < config.plantFanTimer.minStartPercent) {
        percent = config.plantFanTimer.minStartPercent;
    }
    currentPowerPct = constrain(percent, 0, 100);
    ledcWrite(PLANT_FAN_CHANNEL, map(currentPowerPct, 0, 100, 0, 255));
}

// ============================================================================
// СТАН-МАШИНА ВІТРУ
// ============================================================================
static void startCalmPhase() {
    const BreezeConfig& b = config.breezeConfig;
    breezePhase      = BREEZE_CALM;
    breezePhaseStart = millis();
    breezeBasePct    = (uint8_t)randRange(b.baseSpeedMin, b.baseSpeedMax);
    breezeStartPct   = currentPowerPct;
    breezeTargetPct  = breezeBasePct;
    breezePhaseDur   = randRange((uint32_t)b.calmMinSec * 1000, (uint32_t)b.calmMaxSec * 1000);
}

static void startRisePhase() {
    breezePhase      = BREEZE_RISE;
    breezePhaseStart = millis();
    breezeStartPct   = currentPowerPct;
    breezeTargetPct  = constrain(breezeBasePct + config.breezeConfig.gustBoost, 0, 100);
    breezePhaseDur   = randRange(500, 1500);
}

static void startHoldPhase() {
    const BreezeConfig& b = config.breezeConfig;
    breezePhase      = BREEZE_HOLD;
    breezePhaseStart = millis();
    breezeTargetPct  = currentPowerPct;
    breezePhaseDur   = randRange((uint32_t)b.gustMinSec * 1000, (uint32_t)b.gustMaxSec * 1000);
}

static void startDecayPhase() {
    breezePhase      = BREEZE_DECAY;
    breezePhaseStart = millis();
    breezeStartPct   = currentPowerPct;
    breezeTargetPct  = breezeBasePct;
    breezePhaseDur   = randRange(800, 2000);
}

// Один крок стан-машини вітру (викликається тільки під час ON фази)
static void stepBreeze() {
    if (breezeResetNeeded) {
        breezeResetNeeded = false;
        startCalmPhase();
        return;
    }
    unsigned long elapsed = millis() - breezePhaseStart;

    switch (breezePhase) {
        case BREEZE_CALM:
            applyPower(lerp8(breezeStartPct, breezeTargetPct, elapsed, breezePhaseDur));
            if (elapsed >= breezePhaseDur) {
                if ((esp_random() % 10) < 4) startRisePhase();
                else                          startCalmPhase();
            }
            break;

        case BREEZE_RISE:
            applyPower(lerp8(breezeStartPct, breezeTargetPct, elapsed, breezePhaseDur));
            if (elapsed >= breezePhaseDur) startHoldPhase();
            break;

        case BREEZE_HOLD: {
            int8_t jitter = (int8_t)(esp_random() % 11) - 5;
            applyPower(constrain((int)breezeTargetPct + jitter, 0, 100));
            if (elapsed >= breezePhaseDur) startDecayPhase();
            break;
        }

        case BREEZE_DECAY:
            applyPower(lerp8(breezeStartPct, breezeTargetPct, elapsed, breezePhaseDur));
            if (elapsed >= breezePhaseDur) startCalmPhase();
            break;
    }
}

// ============================================================================
// ГОЛОВНА ЛОГІКА ТАЙМЕРА
// ============================================================================
static void updateFan() {
    const PlantFanTimer& t = config.plantFanTimer;

    if (!t.enabled) {
        applyPower(0);
        return;
    }

    unsigned long now     = millis();
    unsigned long elapsed = now - timerPhaseStart;

    if (timerState) {
        // ON фаза — перевіряємо чи не вичерпався час
        unsigned long onDur = ((unsigned long)t.onMinutes * 60 + t.onSeconds) * 1000UL;
        if (onDur == 0) onDur = 1000;

        if (elapsed >= onDur) {
            timerState      = false;
            timerPhaseStart = now;
            applyPower(0);
            Serial.println("🌿 Вентилятор рослин: ПАУЗА");
            return;
        }

        // ON: вітер або фіксована потужність
        if (t.breezeEnabled) {
            stepBreeze();
        } else {
            applyPower(t.powerPercent);
        }

    } else {
        // OFF фаза
        unsigned long offDur = ((unsigned long)t.offMinutes * 60 + t.offSeconds) * 1000UL;
        if (offDur == 0 || elapsed >= offDur) {
            timerState      = true;
            timerPhaseStart = now;
            // Скидаємо стан вітру на початок ON фази
            breezeBasePct = config.breezeConfig.baseSpeedMin;
            startCalmPhase();
            Serial.printf("🌿 Вентилятор рослин: РОБОТА (%s)\n",
                          t.breezeEnabled ? "природній вітер" : String(t.powerPercent).c_str());
        }
        // Під час OFF — вимкнено
    }
}

// ============================================================================
// ПУБЛІЧНИЙ API
// ============================================================================
void plantFanInit() {
    plantFanLoad();

    ledcSetup(PLANT_FAN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PLANT_FAN_PIN, PLANT_FAN_CHANNEL);
    ledcWrite(PLANT_FAN_CHANNEL, 0);

    timerPhaseStart = millis();
    timerState      = false;
    breezeBasePct   = config.breezeConfig.baseSpeedMin;
    startCalmPhase();

    Serial.printf("✓ Вентилятор рослин: таймер=%s, вітер=%s\n",
                  config.plantFanTimer.enabled ? "ON" : "OFF",
                  config.plantFanTimer.breezeEnabled ? "ON" : "OFF");
}

void    plantFanResetBreeze()      { breezeResetNeeded = true; }
uint8_t plantFanGetCurrentPower()  { return currentPowerPct; }
BreezePhase plantFanGetBreezePhase() { return breezePhase; }
bool plantFanGetTimerState()       { return timerState; }

unsigned long plantFanGetTimeToNextChange() {
    const PlantFanTimer& t = config.plantFanTimer;
    unsigned long phaseDur = timerState
        ? ((unsigned long)t.onMinutes  * 60 + t.onSeconds)  * 1000UL
        : ((unsigned long)t.offMinutes * 60 + t.offSeconds) * 1000UL;
    unsigned long elapsed = millis() - timerPhaseStart;
    if (elapsed >= phaseDur) return 0;
    return (phaseDur - elapsed) / 1000;
}

void plantFanTask(void* parameter) {
    Serial.println("✓ Задачу вентилятора рослин запущено");
    plantFanInit();
    while (1) {
        updateFan();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// ЗБЕРЕЖЕННЯ / ЗАВАНТАЖЕННЯ
// ============================================================================
void plantFanSave() {
    Preferences prefs;
    prefs.begin("plant_fan", false);

    const PlantFanTimer& t = config.plantFanTimer;
    prefs.putBool("enabled",      t.enabled);
    prefs.putBool("breezeEnabled", t.breezeEnabled);
    prefs.putUShort("onMin",      t.onMinutes);
    prefs.putUShort("onSec",      t.onSeconds);
    prefs.putUShort("offMin",     t.offMinutes);
    prefs.putUShort("offSec",     t.offSeconds);
    prefs.putUChar("power",       t.powerPercent);
    prefs.putUChar("minStart",    t.minStartPercent);

    const BreezeConfig& b = config.breezeConfig;
    prefs.putUChar("brzBaseMin",  b.baseSpeedMin);
    prefs.putUChar("brzBaseMax",  b.baseSpeedMax);
    prefs.putUChar("brzBoost",    b.gustBoost);
    prefs.putUShort("brzGstMin",  b.gustMinSec);
    prefs.putUShort("brzGstMax",  b.gustMaxSec);
    prefs.putUShort("brzCalmMin", b.calmMinSec);
    prefs.putUShort("brzCalmMax", b.calmMaxSec);

    prefs.end();
}

void plantFanLoad() {
    Preferences prefs;
    prefs.begin("plant_fan", true);

    PlantFanTimer& t = config.plantFanTimer;
    t.enabled        = prefs.getBool("enabled",       false);
    t.breezeEnabled  = prefs.getBool("breezeEnabled",  false);
    t.onMinutes      = prefs.getUShort("onMin",       PLANT_FAN_TIMER_DEFAULT_ON);
    t.onSeconds      = prefs.getUShort("onSec",       0);
    t.offMinutes     = prefs.getUShort("offMin",      PLANT_FAN_TIMER_DEFAULT_OFF);
    t.offSeconds     = prefs.getUShort("offSec",      0);
    t.powerPercent   = prefs.getUChar("power",        PLANT_FAN_TIMER_DEFAULT_POWER);
    t.minStartPercent = prefs.getUChar("minStart",    60);

    BreezeConfig& b = config.breezeConfig;
    b.baseSpeedMin = prefs.getUChar("brzBaseMin",  BREEZE_BASE_MIN);
    b.baseSpeedMax = prefs.getUChar("brzBaseMax",  BREEZE_BASE_MAX);
    b.gustBoost    = prefs.getUChar("brzBoost",    BREEZE_GUST_BOOST);
    b.gustMinSec   = prefs.getUShort("brzGstMin",  BREEZE_GUST_MIN_SEC);
    b.gustMaxSec   = prefs.getUShort("brzGstMax",  BREEZE_GUST_MAX_SEC);
    b.calmMinSec   = prefs.getUShort("brzCalmMin", BREEZE_CALM_MIN_SEC);
    b.calmMaxSec   = prefs.getUShort("brzCalmMax", BREEZE_CALM_MAX_SEC);

    prefs.end();
}
