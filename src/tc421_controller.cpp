// ============================================================================
// TC421_CONTROLLER.CPP - Керування Wi-Fi фітолампою TC421 + Конструктор дня
// ============================================================================

#include "tc421_controller.h"
#include "system_core.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <math.h>

static TC421State _state = {
    "",
    "",
    {0, 0, 0, 0, 0},
    false,
    false,
    false,
    0
};

static TC421ScheduleConfig _profiles[TC421_PROFILE_COUNT];
static uint8_t _activeProfile = TC421_PRESET_NATURAL;
#define _sched (_profiles[_activeProfile])
static SemaphoreHandle_t _tc421Mutex = nullptr;
TaskHandle_t tc421TaskHandle = nullptr;

// ============================================================================
// ПРЕСЕТИ ПРИРОДНОГО ОСВІТЛЕННЯ
// ============================================================================

static TC421ScheduleConfig makePreset(TC421Preset preset) {
    TC421ScheduleConfig c;
    memset(&c, 0, sizeof(c));
    c.enabled = true;
    c.ecoMaxPercent = 15; // Мінімальна підтримка фотоперіоду під час блекауту

    switch (preset) {
        case TC421_PRESET_VEG: // Вегетація 18/6 (06:00 -> 00:00)
            strncpy(c.name, "Вегетація 18/6", sizeof(c.name) - 1);
            c.sunriseHour   = 6;  c.sunriseMin = 0;
            c.sunsetHour    = 0;  c.sunsetMin  = 0;
            c.sunriseDurMin = 45;
            c.sunsetDurMin  = 45;
            c.dayMax[0]     = 60; // CH1 Червоний
            c.dayMax[1]     = 90; // CH2 Білий
            c.dayMax[2]     = 75; // CH3 Жовтий
            c.dayMax[3]     = 25; // CH4 IR+UV (полудень)
            c.dawnIrDurMin  = 15;
            c.dawnIrPct     = 30;
            c.middayUvHours = 2;
            c.eodIrDurMin   = 15;
            c.eodIrPct      = 40;
            break;

        case TC421_PRESET_BLOOM: // Цвітіння 12/12 (07:00 -> 19:00)
            strncpy(c.name, "Цвітіння 12/12", sizeof(c.name) - 1);
            c.sunriseHour   = 7;  c.sunriseMin = 0;
            c.sunsetHour    = 19; c.sunsetMin  = 0;
            c.sunriseDurMin = 45;
            c.sunsetDurMin  = 45;
            c.dayMax[0]     = 95; // CH1 Червоний
            c.dayMax[1]     = 75; // CH2 Білий
            c.dayMax[2]     = 90; // CH3 Жовтий
            c.dayMax[3]     = 40; // CH4 IR+UV (полудень)
            c.dawnIrDurMin  = 15;
            c.dawnIrPct     = 35;
            c.middayUvHours = 3;
            c.eodIrDurMin   = 20;
            c.eodIrPct      = 50;
            break;

        case TC421_PRESET_CUSTOM: // Користувацький профіль (нічний тариф 23:00 -> 17:00, 18/6)
            strncpy(c.name, "Мій профіль", sizeof(c.name) - 1);
            c.sunriseHour   = 23; c.sunriseMin = 0;
            c.sunsetHour    = 17; c.sunsetMin  = 0;
            c.sunriseDurMin = 45;
            c.sunsetDurMin  = 45;
            c.dayMax[0]     = 75;
            c.dayMax[1]     = 85;
            c.dayMax[2]     = 80;
            c.dayMax[3]     = 30;
            c.dawnIrDurMin  = 15;
            c.dawnIrPct     = 35;
            c.middayUvHours = 3;
            c.eodIrDurMin   = 15;
            c.eodIrPct      = 45;
            break;

        case TC421_PRESET_NATURAL: // Природний 14/10 (06:30 -> 20:30)
        default:
            strncpy(c.name, "Природний 14/10", sizeof(c.name) - 1);
            c.sunriseHour   = 6;  c.sunriseMin = 30;
            c.sunsetHour    = 20; c.sunsetMin  = 30;
            c.sunriseDurMin = 60;
            c.sunsetDurMin  = 60;
            c.dayMax[0]     = 80; // CH1 Червоний
            c.dayMax[1]     = 85; // CH2 Білий
            c.dayMax[2]     = 85; // CH3 Жовтий
            c.dayMax[3]     = 30; // CH4 IR+UV (полудень)
            c.dawnIrDurMin  = 15;
            c.dawnIrPct     = 35;
            c.middayUvHours = 3;
            c.eodIrDurMin   = 15;
            c.eodIrPct      = 45;
            break;
    }
    c.name[sizeof(c.name) - 1] = '\0';
    return c;
}

static void sanitizeSchedule(TC421ScheduleConfig& c) {
    c.name[sizeof(c.name) - 1] = '\0';
    c.sunriseHour   = constrain(c.sunriseHour, 0, 23);
    c.sunriseMin    = constrain(c.sunriseMin, 0, 59);
    c.sunsetHour    = constrain(c.sunsetHour, 0, 23);
    c.sunsetMin     = constrain(c.sunsetMin, 0, 59);
    c.sunriseDurMin = constrain(c.sunriseDurMin, 5, 120);
    c.sunsetDurMin  = constrain(c.sunsetDurMin, 5, 120);
    for (int i = 0; i < 4; i++) {
        c.dayMax[i] = constrain(c.dayMax[i], 0, 100);
    }
    c.dawnIrDurMin  = constrain(c.dawnIrDurMin, 0, 45);
    c.dawnIrPct     = constrain(c.dawnIrPct, 0, 100);
    c.middayUvHours = constrain(c.middayUvHours, 0, 12);
    c.eodIrDurMin   = constrain(c.eodIrDurMin, 0, 45);
    c.eodIrPct      = constrain(c.eodIrPct, 0, 100);
    c.ecoMaxPercent = constrain(c.ecoMaxPercent, 0, 100);
}

// ============================================================================
// NVS ЗБЕРЕЖЕННЯ / ЗАВАНТАЖЕННЯ
// ============================================================================

static void tc421LoadNVS() {
    Preferences prefs;
    prefs.begin("tc421", true);
    String ip = prefs.getString("ip", "");
    String mac = prefs.getString("mac", "");
    for (int i = 0; i < TC421_CHANNELS; i++) {
        String key = "ch" + String(i);
        _state.channels[i] = prefs.getUChar(key.c_str(), 0);
    }

    _activeProfile = prefs.getUChar("actProf", TC421_PRESET_NATURAL);
    if (_activeProfile >= TC421_PROFILE_COUNT) _activeProfile = TC421_PRESET_NATURAL;

    for (uint8_t p = 0; p < TC421_PROFILE_COUNT; p++) {
        String key = "prof" + String(p);
        if (prefs.getBytesLength(key.c_str()) == sizeof(TC421ScheduleConfig)) {
            prefs.getBytes(key.c_str(), &_profiles[p], sizeof(TC421ScheduleConfig));
            sanitizeSchedule(_profiles[p]);
            if (strlen(_profiles[p].name) == 0) {
                TC421ScheduleConfig def = makePreset((TC421Preset)p);
                strncpy(_profiles[p].name, def.name, sizeof(_profiles[p].name) - 1);
            }
        } else {
            _profiles[p] = makePreset((TC421Preset)p);
        }
    }
    prefs.end();

    strncpy(_state.ip, ip.c_str(), sizeof(_state.ip) - 1);
    _state.ip[sizeof(_state.ip) - 1] = '\0';
    strncpy(_state.mac, mac.c_str(), sizeof(_state.mac) - 1);
    _state.mac[sizeof(_state.mac) - 1] = '\0';
}

static void tc421SaveNVS() {
    Preferences prefs;
    prefs.begin("tc421", false);
    prefs.putString("ip", _state.ip);
    prefs.putString("mac", _state.mac);
    for (int i = 0; i < TC421_CHANNELS; i++) {
        String key = "ch" + String(i);
        prefs.putUChar(key.c_str(), _state.channels[i]);
    }
    prefs.putUChar("actProf", _activeProfile);
    for (uint8_t p = 0; p < TC421_PROFILE_COUNT; p++) {
        String key = "prof" + String(p);
        prefs.putBytes(key.c_str(), &_profiles[p], sizeof(TC421ScheduleConfig));
    }
    prefs.end();
}

// ============================================================================
// МАТЕМАТИКА ПРИРОДНОГО ДНЯ (з підтримкою переходу через 00:00 та ЕКО-режиму)
// ============================================================================

static float smoothStep01(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return x * x * (3.0f - 2.0f * x);
}

void tc421CalculateAt(const TC421ScheduleConfig& cfg, uint16_t minuteOfDay, uint8_t out[4], bool applyEco) {
    out[0] = out[1] = out[2] = out[3] = 0;

    int startMin = (int)cfg.sunriseHour * 60 + (int)cfg.sunriseMin;
    int endMin   = (int)cfg.sunsetHour  * 60 + (int)cfg.sunsetMin;

    // Тривалість світлового дня (від початку світанку до кінця заходу)
    int dayLen = endMin - startMin;
    if (dayLen <= 0) dayLen += 1440;
    if (dayLen < 30) dayLen = 30;

    // Зміщення поточної хвилини відносно startMin у діапазоні [0..1439]
    int t = (int)minuteOfDay - startMin;
    if (t < 0) t += 1440;

    int srDur = (int)cfg.sunriseDurMin;
    int ssDur = (int)cfg.sunsetDurMin;
    if (srDur + ssDur > dayLen) {
        srDur = dayLen / 2;
        ssDur = dayLen - srDur;
    }
    int ssStart = dayLen - ssDur;

    float fRed = 0.0f, fWhite = 0.0f, fYellow = 0.0f, fIrUv = 0.0f;

    // 1. Основні канали (CH1 Червоний, CH2 Білий, CH3 Жовтий)
    if (t >= 0 && t < dayLen) {
        if (t < srDur) {
            // СВІТАНОК: спочатку Червоний + Жовтий, Білий заходить із затримкою 25%
            float p = (srDur > 0) ? ((float)t / (float)srDur) : 1.0f;
            float pWarm = smoothStep01(p);
            float pWhite = smoothStep01((p - 0.25f) / 0.75f);
            fRed    = cfg.dayMax[0] * pWarm;
            fYellow = cfg.dayMax[2] * pWarm;
            fWhite  = cfg.dayMax[1] * pWhite;
        } else if (t < ssStart) {
            // ДЕНЬ (пік)
            fRed    = cfg.dayMax[0];
            fWhite  = cfg.dayMax[1];
            fYellow = cfg.dayMax[2];
        } else {
            // ЗАХІД: Білий гасне першим (за перші 75% заходу), Червоний + Жовтий догорають
            float p = (ssDur > 0) ? ((float)(t - ssStart) / (float)ssDur) : 1.0f;
            float pWarm  = 1.0f - smoothStep01(p);
            float pWhite = 1.0f - smoothStep01(p / 0.75f);
            fRed    = cfg.dayMax[0] * pWarm;
            fYellow = cfg.dayMax[2] * pWarm;
            fWhite  = cfg.dayMax[1] * pWhite;
        }
    }

    // 2. CH4 (IR + UV): три біологічних вікна (беремо максимум із них)
    // 2а. Світанковий імпульс IR (ефект Емерсона)
    if (cfg.dawnIrDurMin > 0 && cfg.dawnIrPct > 0 && t >= 0 && t < (int)cfg.dawnIrDurMin) {
        float p = (float)t / (float)cfg.dawnIrDurMin;
        // Плавний купол (підйом 25%, плато 50%, спад 25%)
        float env = (p < 0.25f) ? smoothStep01(p / 0.25f)
                  : (p > 0.75f) ? smoothStep01((1.0f - p) / 0.25f)
                  : 1.0f;
        float v = cfg.dawnIrPct * env;
        if (v > fIrUv) fIrUv = v;
    }

    // 2б. Полуденне вікно UV+IR (у центрі світлового дня)
    if (cfg.middayUvHours > 0 && cfg.dayMax[3] > 0) {
        int winDur = (int)cfg.middayUvHours * 60;
        int coreLen = ssStart - srDur;
        if (winDur > coreLen && coreLen > 0) winDur = coreLen;
        if (winDur > 0) {
            int center = dayLen / 2;
            int wStart = center - winDur / 2;
            int wEnd   = wStart + winDur;
            if (t >= wStart && t < wEnd) {
                int ramp = (winDur >= 40) ? 15 : (winDur / 4);
                float env = 1.0f;
                if (ramp > 0 && t < wStart + ramp) {
                    env = smoothStep01((float)(t - wStart) / (float)ramp);
                } else if (ramp > 0 && t > wEnd - ramp) {
                    env = smoothStep01((float)(wEnd - t) / (float)ramp);
                }
                float v = cfg.dayMax[3] * env;
                if (v > fIrUv) fIrUv = v;
            }
        }
    }

    // 2в. Вечірній EOD Far-Red "сигнал сну" (останні 5 хв заходу + eodIrDurMin після заходу)
    if (cfg.eodIrDurMin > 0 && cfg.eodIrPct > 0) {
        int overlap = (ssDur >= 5) ? 5 : ssDur;
        int eodStart = dayLen - overlap;
        int eodEnd   = dayLen + (int)cfg.eodIrDurMin;
        int eodTotal = eodEnd - eodStart;
        if (t >= eodStart && t < eodEnd && eodTotal > 0) {
            float p = (float)(t - eodStart) / (float)eodTotal;
            float env = (p < 0.15f) ? smoothStep01(p / 0.15f)
                      : (p > 0.85f) ? smoothStep01((1.0f - p) / 0.15f)
                      : 1.0f;
            float v = cfg.eodIrPct * env;
            if (v > fIrUv) fIrUv = v;
        }
    }

    out[0] = (uint8_t)constrain((int)roundf(fRed),    0, 100);
    out[1] = (uint8_t)constrain((int)roundf(fWhite),  0, 100);
    out[2] = (uint8_t)constrain((int)roundf(fYellow), 0, 100);
    out[3] = (uint8_t)constrain((int)roundf(fIrUv),   0, 100);

    // 3. ЕКО-режим (під час блекауту / живлення від АКБ)
    if (applyEco) {
        if (cfg.ecoMaxPercent == 0) {
            out[0] = out[1] = out[2] = out[3] = 0;
        } else {
            uint8_t peak = 0;
            for (int i = 0; i < 4; i++) {
                if (out[i] > peak) peak = out[i];
            }
            if (peak > cfg.ecoMaxPercent) {
                float scale = (float)cfg.ecoMaxPercent / (float)peak;
                for (int i = 0; i < 4; i++) {
                    if (out[i] > 0) {
                        int scaled = (int)roundf(out[i] * scale);
                        out[i] = (uint8_t)constrain(scaled, 1, (int)cfg.ecoMaxPercent);
                    }
                }
            }
        }
    }
}

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ ПРОТОКОЛУ (64-байтні пакети TC421 WiFi)
// ============================================================================

static void tc421ApplyWifiFooter(uint8_t* pkt, const char* mac) {
    pkt[TC421_PACKET_SIZE - 3] = pkt[TC421_PACKET_SIZE - 1];
    pkt[TC421_PACKET_SIZE - 1] = 0;
    if (mac != nullptr && strlen(mac) == 12) {
        memcpy(&pkt[TC421_PACKET_SIZE - 15], mac, 12);
    }
}

static void tc421BuildSearchPacket(uint8_t* pkt) {
    memset(pkt, 0, TC421_PACKET_SIZE);
    pkt[0]  = 0x55;
    pkt[1]  = 0xAA;
    pkt[2]  = 0x17;
    pkt[5]  = 0x03;
    pkt[6]  = 0xDD;
    pkt[7]  = 0xEE;
    pkt[8]  = 0xFF;
    pkt[61] = 0xE3;
}

static void tc421BuildReadyPlayPacket(uint8_t* pkt, const char* name, const char* mac) {
    memset(pkt, 0, TC421_PACKET_SIZE);
    size_t len = strlen(name);
    if (len > 8) len = 4;

    pkt[0] = 0x55;
    pkt[1] = 0xAA;
    pkt[2] = 0x06;
    pkt[3] = 0x01;
    pkt[4] = 0x00;
    pkt[5] = (uint8_t)len;
    pkt[6] = 0x00;
    memcpy(&pkt[7], name, len);

    uint16_t sum = 0;
    for (size_t i = 0; i < 7 + len; i++) sum += pkt[i];
    pkt[TC421_PACKET_SIZE - 1] = (uint8_t)(sum & 0xFF);
    tc421ApplyWifiFooter(pkt, mac);
}

static void tc421BuildPlayValuesPacket(uint8_t* pkt, const uint8_t ch[TC421_CHANNELS], const char* mac) {
    memset(pkt, 0, TC421_PACKET_SIZE);
    pkt[0] = 0x55;
    pkt[1] = 0xAA;
    pkt[2] = 0x06;
    pkt[3] = 0x02;
    pkt[4] = 0x00;
    pkt[5] = TC421_CHANNELS;
    for (int i = 0; i < TC421_CHANNELS; i++) {
        pkt[6 + i] = ch[i];
    }

    uint16_t sum = 0;
    for (int i = 0; i < 6 + TC421_CHANNELS; i++) sum += pkt[i];
    pkt[TC421_PACKET_SIZE - 1] = (uint8_t)(sum & 0xFF);
    tc421ApplyWifiFooter(pkt, mac);
}

static void tc421BuildEndTxPacket(uint8_t* pkt, const char* mac) {
    memset(pkt, 0, TC421_PACKET_SIZE);
    pkt[0] = 0x55;
    pkt[1] = 0xAA;
    pkt[2] = 0x03;
    pkt[3] = 0x08;
    pkt[TC421_PACKET_SIZE - 1] = 0x0A;
    tc421ApplyWifiFooter(pkt, mac);
}

static void tc421BuildTimeSyncPacket(uint8_t* pkt, const struct tm* t, const char* mac) {
    memset(pkt, 0, TC421_PACKET_SIZE);
    int year = t->tm_year + 1900;
    pkt[0]  = 0x55;
    pkt[1]  = 0xAA;
    pkt[2]  = 0x02;
    pkt[3]  = 0x00;
    pkt[4]  = 0x00;
    pkt[5]  = 0x07;
    pkt[6]  = (uint8_t)((year >> 8) & 0xFF);
    pkt[7]  = (uint8_t)(year & 0xFF);
    pkt[8]  = (uint8_t)(t->tm_mon + 1);
    pkt[9]  = (uint8_t)t->tm_mday;
    pkt[10] = (uint8_t)t->tm_hour;
    pkt[11] = (uint8_t)t->tm_min;
    pkt[12] = (uint8_t)t->tm_sec;

    uint16_t sum = 0;
    for (int i = 0; i < 13; i++) sum += pkt[i];
    pkt[TC421_PACKET_SIZE - 1] = (uint8_t)(sum & 0xFF);
    tc421ApplyWifiFooter(pkt, mac);
}

static bool tc421NetworkReady() {
    wifi_mode_t m = WiFi.getMode();
    return (WiFi.status() == WL_CONNECTED) || (m == WIFI_AP) || (m == WIFI_AP_STA);
}

static bool tc421SendPacket(const uint8_t* pkt, uint8_t* replyBuf = nullptr, uint16_t waitMs = 0) {
    if (!tc421NetworkReady()) return false;

    IPAddress targetIp;
    if (strlen(_state.ip) == 0 || !targetIp.fromString(_state.ip)) {
        return false;
    }

    WiFiUDP udp;
    if (!udp.begin(0)) return false;

    udp.beginPacket(targetIp, TC421_UDP_PORT);
    udp.write(pkt, TC421_PACKET_SIZE);
    bool sent = (udp.endPacket() == 1);

    if (sent && replyBuf != nullptr && waitMs > 0) {
        unsigned long start = millis();
        bool got = false;
        while (millis() - start < waitMs) {
            int pSize = udp.parsePacket();
            if (pSize >= 16) {
                memset(replyBuf, 0, TC421_PACKET_SIZE);
                udp.read(replyBuf, TC421_PACKET_SIZE);
                got = true;
                break;
            }
            delay(10);
        }
        udp.stop();
        return got;
    }

    udp.stop();
    return sent;
}

static bool tc421DiscoverLocked(uint16_t timeoutMs) {
    if (!tc421NetworkReady()) return false;

    uint8_t searchPkt[TC421_PACKET_SIZE];
    tc421BuildSearchPacket(searchPkt);

    WiFiUDP udp;
    if (!udp.begin(TC421_UDP_PORT)) {
        if (!udp.begin(0)) return false;
    }

    // 1. Збережений IP (unicast)
    IPAddress savedIp;
    if (strlen(_state.ip) > 0 && savedIp.fromString(_state.ip)) {
        udp.beginPacket(savedIp, TC421_UDP_PORT);
        udp.write(searchPkt, TC421_PACKET_SIZE);
        udp.endPacket();
    }

    // 2. Власна AP-підмережа ESP32 (192.168.10.2..6 + .255)
    wifi_mode_t wmode = WiFi.getMode();
    if (wmode == WIFI_AP || wmode == WIFI_AP_STA) {
        IPAddress apIp = WiFi.softAPIP();
        if (apIp != IPAddress(0, 0, 0, 0)) {
            for (uint8_t host = 2; host <= 6; host++) {
                udp.beginPacket(IPAddress(apIp[0], apIp[1], apIp[2], host), TC421_UDP_PORT);
                udp.write(searchPkt, TC421_PACKET_SIZE);
                udp.endPacket();
            }
            udp.beginPacket(IPAddress(apIp[0], apIp[1], apIp[2], 255), TC421_UDP_PORT);
            udp.write(searchPkt, TC421_PACKET_SIZE);
            udp.endPacket();
        }
    }

    // 3. STA-підмережа
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress localIp = WiFi.localIP();
        IPAddress mask = WiFi.subnetMask();
        IPAddress subnetBcast(
            (uint8_t)(localIp[0] | ~mask[0]),
            (uint8_t)(localIp[1] | ~mask[1]),
            (uint8_t)(localIp[2] | ~mask[2]),
            (uint8_t)(localIp[3] | ~mask[3])
        );
        udp.beginPacket(subnetBcast, TC421_UDP_PORT);
        udp.write(searchPkt, TC421_PACKET_SIZE);
        udp.endPacket();
    }

    // 4. Глобальний broadcast
    udp.beginPacket(IPAddress(255, 255, 255, 255), TC421_UDP_PORT);
    udp.write(searchPkt, TC421_PACKET_SIZE);
    udp.endPacket();

    unsigned long start = millis();
    uint8_t resp[TC421_PACKET_SIZE];
    bool ok = false;

    while (millis() - start < timeoutMs) {
        int pSize = udp.parsePacket();
        if (pSize >= 16) {
            memset(resp, 0, sizeof(resp));
            udp.read(resp, sizeof(resp));
            if (resp[0] == 0x88 && resp[1] == 0x00 && resp[2] == 0x17) {
                IPAddress remIp = udp.remoteIP();
                strncpy(_state.ip, remIp.toString().c_str(), sizeof(_state.ip) - 1);
                _state.ip[sizeof(_state.ip) - 1] = '\0';

                memcpy(_state.mac, &resp[4], 12);
                _state.mac[12] = '\0';

                _state.found = true;
                _state.lastSeenMs = millis();
                tc421SaveNVS();
                Serial.printf("[TC421] Знайдено: IP=%s MAC=%s\n", _state.ip, _state.mac);
                ok = true;
                break;
            }
        }
        delay(15);
    }

    udp.stop();
    return ok;
}

static bool tc421SendChannelsLocked(const uint8_t vals[TC421_CHANNELS], bool saveToNvs) {
    if (strlen(_state.ip) == 0 || strlen(_state.mac) != 12) {
        tc421DiscoverLocked(900);
    }
    if (strlen(_state.ip) == 0 || strlen(_state.mac) != 12) {
        return false;
    }

    uint8_t pkt[TC421_PACKET_SIZE];
    if (!_state.liveMode) {
        tc421BuildReadyPlayPacket(pkt, "LIVE", _state.mac);
        tc421SendPacket(pkt);
        delay(80);
    }

    tc421BuildPlayValuesPacket(pkt, vals, _state.mac);
    bool ok = tc421SendPacket(pkt);
    if (ok) {
        memcpy(_state.channels, vals, TC421_CHANNELS);
        _state.liveMode = true;
        _state.lastSeenMs = millis();
        if (saveToNvs) tc421SaveNVS();
    }
    return ok;
}

// ============================================================================
// ЗАДАЧА АВТОПІЛОТА (кожну хвилину + миттєва реакція на ЕКО-режим кожні 5 с)
// ============================================================================

static void tc421Task(void* parameter) {
    (void)parameter;
    TickType_t lastWake = xTaskGetTickCount();
    bool prevEco = false;
    uint8_t tick = 0;

    // Невелика пауза після старту, щоб TC421 встиг підключитись до KlimatLight
    vTaskDelay(pdMS_TO_TICKS(4000));
    if (tc421NetworkReady()) {
        tc421Discover(1000);
        tc421SchedulerUpdate(true);
    }

    while (true) {
        bool curEco = isEcoModeActive();
        // Кожні 60 сек (12 * 5с) або миттєво при зміні ЕКО-режиму
        if (tick == 0 || curEco != prevEco) {
            prevEco = curEco;
            // Кожні 5 хв примусово підтверджуємо пакет (на випадок ребуту TC421)
            bool forceRefresh = (tick == 0 && (millis() / 60000UL) % 5 == 0);
            tc421SchedulerUpdate(forceRefresh || curEco != prevEco);
        }
        tick = (tick + 1) % 12;
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(5000));
    }
}

// ============================================================================
// ПУБЛІЧНИЙ API
// ============================================================================

void tc421Init() {
    if (_tc421Mutex == nullptr) {
        _tc421Mutex = xSemaphoreCreateMutex();
    }
    tc421LoadNVS();
    Serial.printf("[TC421] Ініціалізовано (IP='%s', MAC='%s', автопілот=%s, ЕКО=%u%%)\n",
                  _state.ip, _state.mac, _sched.enabled ? "ON" : "OFF", _sched.ecoMaxPercent);

    if (tc421TaskHandle == nullptr) {
        xTaskCreatePinnedToCore(tc421Task, "TC421Task", 4096, nullptr, 1, &tc421TaskHandle, 0);
    }
}

bool tc421Discover(uint16_t timeoutMs) {
    if (_tc421Mutex == nullptr) return false;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;
    bool ok = tc421DiscoverLocked(timeoutMs);
    xSemaphoreGive(_tc421Mutex);
    return ok;
}

bool tc421SetTarget(const String& ipStr, const String& macStr) {
    if (_tc421Mutex == nullptr) return false;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;

    String cleanIp = ipStr;
    cleanIp.trim();
    String cleanMac = macStr;
    cleanMac.trim();
    cleanMac.toUpperCase();

    IPAddress parsed;
    if (!parsed.fromString(cleanIp)) {
        xSemaphoreGive(_tc421Mutex);
        return false;
    }

    strncpy(_state.ip, cleanIp.c_str(), sizeof(_state.ip) - 1);
    _state.ip[sizeof(_state.ip) - 1] = '\0';

    if (cleanMac.length() == 12) {
        strncpy(_state.mac, cleanMac.c_str(), sizeof(_state.mac) - 1);
        _state.mac[12] = '\0';
    }
    tc421SaveNVS();

    bool ok = tc421DiscoverLocked(1000);
    if (!ok && strlen(_state.mac) == 12) ok = true;
    xSemaphoreGive(_tc421Mutex);
    return ok;
}

bool tc421SetChannels(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, bool saveNvs) {
    if (_tc421Mutex == nullptr) return false;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;

    uint8_t vals[TC421_CHANNELS] = {
        (uint8_t)constrain(c1, 0, 100),
        (uint8_t)constrain(c2, 0, 100),
        (uint8_t)constrain(c3, 0, 100),
        (uint8_t)constrain(c4, 0, 100),
        (uint8_t)constrain(c5, 0, 100)
    };

    bool ok = tc421SendChannelsLocked(vals, saveNvs);
    if (ok && saveNvs) {
        Serial.printf("[TC421] Ручні канали: R=%d%% W=%d%% Y=%d%% IR/UV=%d%%\n",
                      vals[0], vals[1], vals[2], vals[3]);
    }
    xSemaphoreGive(_tc421Mutex);
    return ok;
}

bool tc421Off() {
    return tc421SetChannels(0, 0, 0, 0, 0);
}

bool tc421ResumeSchedule() {
    if (_tc421Mutex == nullptr) return false;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;

    _sched.enabled = false;
    tc421SaveNVS();

    if (strlen(_state.ip) == 0 || strlen(_state.mac) != 12) {
        xSemaphoreGive(_tc421Mutex);
        return false;
    }

    uint8_t pkt[TC421_PACKET_SIZE];
    tc421BuildEndTxPacket(pkt, _state.mac);
    bool ok = tc421SendPacket(pkt);
    if (ok) {
        _state.liveMode = false;
        Serial.println("[TC421] Повернено на внутрішній розклад TC421");
    }

    xSemaphoreGive(_tc421Mutex);
    return ok;
}

bool tc421SyncTime() {
    if (_tc421Mutex == nullptr) return false;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return false;

    if (strlen(_state.ip) == 0 || strlen(_state.mac) != 12) {
        tc421DiscoverLocked(1000);
    }
    if (strlen(_state.ip) == 0 || strlen(_state.mac) != 12) {
        xSemaphoreGive(_tc421Mutex);
        return false;
    }

    struct tm* t = getTimeInfo();
    if (t == nullptr) {
        xSemaphoreGive(_tc421Mutex);
        return false;
    }

    uint8_t pkt[TC421_PACKET_SIZE];
    uint8_t resp[TC421_PACKET_SIZE];
    tc421BuildTimeSyncPacket(pkt, t, _state.mac);
    bool ok = tc421SendPacket(pkt, resp, 600);
    if (ok && resp[4] == 133) {
        _state.found = true;
        _state.lastSeenMs = millis();
        Serial.printf("[TC421] Час синхронізовано: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                      t->tm_hour, t->tm_min, t->tm_sec);
    }
    xSemaphoreGive(_tc421Mutex);
    return ok;
}

TC421ScheduleConfig tc421GetSchedule() {
    TC421ScheduleConfig copy;
    if (_tc421Mutex != nullptr && xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        copy = _sched;
        xSemaphoreGive(_tc421Mutex);
    } else {
        copy = _sched;
    }
    return copy;
}

void tc421SetSchedule(const TC421ScheduleConfig& cfg, bool applyNow) {
    if (_tc421Mutex == nullptr) return;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return;
    _sched = cfg;
    sanitizeSchedule(_sched);
    tc421SaveNVS();
    xSemaphoreGive(_tc421Mutex);

    if (applyNow && _sched.enabled) {
        tc421SchedulerUpdate(true);
    }
}

uint8_t tc421GetActiveProfile() {
    return _activeProfile;
}

String tc421GetProfileName(uint8_t idx) {
    if (idx >= TC421_PROFILE_COUNT) return "";
    return String(_profiles[idx].name);
}

void tc421SelectProfile(uint8_t idx) {
    if (idx >= TC421_PROFILE_COUNT || _tc421Mutex == nullptr) return;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return;
    _activeProfile = idx;
    _profiles[_activeProfile].enabled = true;
    tc421SaveNVS();
    Serial.printf("[TC421] Активовано профіль #%u: %s\n", idx, _profiles[idx].name);
    xSemaphoreGive(_tc421Mutex);

    tc421SchedulerUpdate(true);
}

void tc421ResetActiveProfile() {
    if (_tc421Mutex == nullptr) return;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return;
    _profiles[_activeProfile] = makePreset((TC421Preset)_activeProfile);
    tc421SaveNVS();
    Serial.printf("[TC421] Профіль #%u скинуто до заводських\n", _activeProfile);
    xSemaphoreGive(_tc421Mutex);

    tc421SchedulerUpdate(true);
}

void tc421ApplyPreset(TC421Preset preset) {
    tc421SelectProfile((uint8_t)preset);
}

void tc421SchedulerUpdate(bool forceSend) {
    if (_tc421Mutex == nullptr) return;
    if (xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(3000)) != pdTRUE) return;

    bool eco = isEcoModeActive();
    _state.ecoLimited = eco;

    if (!_sched.enabled) {
        // У ручному режимі теж страхуємо батарею при блекауті, якщо ecoMaxPercent < поточного
        if (eco && _state.liveMode) {
            uint8_t vals[TC421_CHANNELS] = {
                _state.channels[0], _state.channels[1], _state.channels[2], _state.channels[3], 0
            };
            bool changed = false;
            if (_sched.ecoMaxPercent == 0) {
                for (int i = 0; i < 4; i++) {
                    if (vals[i] > 0) { vals[i] = 0; changed = true; }
                }
            } else {
                for (int i = 0; i < 4; i++) {
                    if (vals[i] > _sched.ecoMaxPercent) {
                        vals[i] = _sched.ecoMaxPercent;
                        changed = true;
                    }
                }
            }
            if (changed) {
                tc421SendChannelsLocked(vals, false);
            }
        }
        xSemaphoreGive(_tc421Mutex);
        return;
    }

    struct tm* t = getTimeInfo();
    if (t == nullptr) {
        xSemaphoreGive(_tc421Mutex);
        return;
    }

    uint16_t minOfDay = (uint16_t)t->tm_hour * 60 + (uint16_t)t->tm_min;
    uint8_t calc[4];
    tc421CalculateAt(_sched, minOfDay, calc, eco);

    uint8_t vals[TC421_CHANNELS] = {calc[0], calc[1], calc[2], calc[3], 0};
    bool changed = forceSend || !_state.liveMode;
    for (int i = 0; i < 4; i++) {
        if (vals[i] != _state.channels[i]) changed = true;
    }

    if (changed) {
        if (tc421SendChannelsLocked(vals, false)) {
            Serial.printf("[TC421] Автопілот %02d:%02d%s -> R=%d%% W=%d%% Y=%d%% IR/UV=%d%%\n",
                          t->tm_hour, t->tm_min, eco ? " (ЕКО)" : "",
                          vals[0], vals[1], vals[2], vals[3]);
        }
    }

    xSemaphoreGive(_tc421Mutex);
}

TC421State tc421GetState() {
    TC421State copy;
    if (_tc421Mutex != nullptr && xSemaphoreTake(_tc421Mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        copy = _state;
        xSemaphoreGive(_tc421Mutex);
    } else {
        copy = _state;
    }
    return copy;
}
