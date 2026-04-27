// ============================================================================
// WEB_PAGES_LIGHT.CPP - Вебсторінка керування освітленням
// ============================================================================

#include <WebServer.h>
#include <ArduinoJson.h>
#include "web_common.h"
#include "light_scheduler.h"
#include "dmx_controller.h"
#include "global_declarations.h"

extern WebServer server;
extern bool checkAuth();

// ============================================================================
// ГОЛОВНА СТОРІНКА ОСВІТЛЕННЯ
// ============================================================================
void handleLightPage() {
    if (!checkAuth()) return;

    LightScheduleConfig& cfg = lightSchedulerGetConfig();
    RGBWColor cur = dmxGetCurrent();

    String html = getHtmlHead("Освітлення");
    html += "<div class='container'>";
    html += getNavHeader("💡 КЕРУВАННЯ ОСВІТЛЕННЯМ");

    // Поточний стан
    html += "<div class='card' style='border-left-color:#f59e0b'>";
    html += "<b>Поточний стан:</b> ";
    html += "R=" + String(cur.r) + " G=" + String(cur.g) +
            " B=" + String(cur.b) + " Master=" + String(cur.master);
    html += "<div style='display:inline-block;width:40px;height:20px;margin-left:10px;border-radius:4px;vertical-align:middle;background:rgb(";
    html += String(cur.r) + "," + String(cur.g) + "," + String(cur.b) + ")'></div>";
    html += "</div>";

    // Вмикач розкладу
    html += "<div class='card'>";
    html += "<form method='POST' action='/light/toggle' style='display:inline'>";
    html += "<label style='font-size:16px;font-weight:600'>";
    html += "<input type='checkbox' name='enabled' value='1' onchange='this.form.submit()' ";
    html += cfg.enabled ? "checked" : "";
    html += "> Автоматичний розклад активний</label>";
    html += "</form>";
    html += "</div>";

    // Ручне керування
    html += "<div class='card' style='border-left-color:#2196F3'>";
    html += "<b>Ручне керування</b>";
    html += "<form method='POST' action='/light/manual' style='margin-top:10px'>";
    html += "<div style='display:grid;grid-template-columns:repeat(4,1fr);gap:10px;max-width:500px'>";
    html += "<div><label>Яскравість (CH1)<br><input type='range' name='w' min='0' max='255' value='" + String(cur.master) + "' oninput='document.getElementById(\"wv\").textContent=this.value'> <span id='wv'>" + String(cur.master) + "</span></label></div>";
    html += "<div><label>Червоний (CH2)<br><input type='range' name='r' min='0' max='255' value='" + String(cur.r) + "' oninput='document.getElementById(\"rv\").textContent=this.value'> <span id='rv'>" + String(cur.r) + "</span></label></div>";
    html += "<div><label>Зелений (CH3)<br><input type='range' name='g' min='0' max='255' value='" + String(cur.g) + "' oninput='document.getElementById(\"gv\").textContent=this.value'> <span id='gv'>" + String(cur.g) + "</span></label></div>";
    html += "<div><label>Синій (CH4)<br><input type='range' name='b' min='0' max='255' value='" + String(cur.b) + "' oninput='document.getElementById(\"bv\").textContent=this.value'> <span id='bv'>" + String(cur.b) + "</span></label></div>";
    html += "</div>";
    html += "<button type='submit' class='btn btn-success' style='margin-top:10px'>Застосувати</button>";
    html += "<button type='button' class='btn btn-danger' onclick='document.location=\"/light/off\"'>Вимкнути</button>";
    html += "</form>";
    html += "</div>";

    // Таблиця keyframe-ів
    html += "<div class='card'>";
    html += "<b>Розклад (ключові точки)</b>";
    html += "<form method='POST' action='/light/save'>";
    html += "<table style='width:100%;border-collapse:collapse;margin-top:10px'>";
    html += "<tr style='background:#f0f0f0'><th style='padding:8px;text-align:left'>Час</th>";
    html += "<th>R</th><th>G</th><th>B</th><th>W</th><th>Колір</th></tr>";

    for (int i = 0; i < LIGHT_KEYFRAME_COUNT; i++) {
        String pre = "kf" + String(i) + "_";
        uint8_t h = (i < cfg.keyframeCount) ? cfg.keyframes[i].hour   : 0;
        uint8_t m = (i < cfg.keyframeCount) ? cfg.keyframes[i].minute  : 0;
        uint8_t r = (i < cfg.keyframeCount) ? cfg.keyframes[i].r       : 0;
        uint8_t g = (i < cfg.keyframeCount) ? cfg.keyframes[i].g       : 0;
        uint8_t b = (i < cfg.keyframeCount) ? cfg.keyframes[i].b       : 0;
        uint8_t w = (i < cfg.keyframeCount) ? cfg.keyframes[i].master   : 255;
        bool active = (i < cfg.keyframeCount);

        String rowStyle = active ? "" : "opacity:0.4";
        html += "<tr style='border-top:1px solid #eee;" + rowStyle + "'>";

        // Час
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", h, m);
        html += "<td style='padding:6px'><input type='time' name='" + pre + "time' value='" + String(timeBuf) + "' style='width:90px'></td>";

        // RGBW
        html += "<td><input type='number' name='" + pre + "r' value='" + String(r) + "' min='0' max='255' style='width:55px'></td>";
        html += "<td><input type='number' name='" + pre + "g' value='" + String(g) + "' min='0' max='255' style='width:55px'></td>";
        html += "<td><input type='number' name='" + pre + "b' value='" + String(b) + "' min='0' max='255' style='width:55px'></td>";
        html += "<td><input type='number' name='" + pre + "w' value='" + String(w) + "' min='0' max='255' style='width:55px'></td>";

        // Preview кольору (тільки RGB, W не відображається браузером)
        html += "<td><div style='width:30px;height:30px;border-radius:4px;background:rgb(";
        html += String(r) + "," + String(g) + "," + String(b);
        html += ");border:1px solid #ccc'></div></td>";
        html += "</tr>";
    }

    html += "</table>";
    html += "<input type='hidden' name='kfCount' value='" + String(LIGHT_KEYFRAME_COUNT) + "'>";
    html += "<div style='margin-top:10px'>";
    html += "<button type='submit' class='btn btn-success'>Зберегти розклад</button>";
    html += "<a href='/light/preset' class='btn' style='background:#9c27b0;color:white'>Скинути пресет</a>";
    html += "</div></form></div>";

    html += getNavFooter();
    html += getHtmlFooter();
    server.send(200, "text/html; charset=utf-8", html);
}

// ============================================================================
// ЗБЕРЕЖЕННЯ РОЗКЛАДУ
// ============================================================================
void handleLightSave() {
    if (!checkAuth()) return;

    LightScheduleConfig cfg = lightSchedulerGetConfig();
    int count = 0;

    for (int i = 0; i < LIGHT_KEYFRAME_COUNT; i++) {
        String pre = "kf" + String(i) + "_";
        String timeStr = server.arg(pre + "time");
        if (timeStr.length() < 5) continue;

        uint8_t h = timeStr.substring(0, 2).toInt();
        uint8_t m = timeStr.substring(3, 5).toInt();
        uint8_t r = (uint8_t)constrain(server.arg(pre + "r").toInt(), 0, 255);
        uint8_t g = (uint8_t)constrain(server.arg(pre + "g").toInt(), 0, 255);
        uint8_t b = (uint8_t)constrain(server.arg(pre + "b").toInt(), 0, 255);
        uint8_t w = (uint8_t)constrain(server.arg(pre + "w").toInt(), 0, 255);

        cfg.keyframes[count++] = {h, m, r, g, b, w};
    }

    // Сортуємо за часом
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            uint16_t ti = cfg.keyframes[i].hour * 60 + cfg.keyframes[i].minute;
            uint16_t tj = cfg.keyframes[j].hour * 60 + cfg.keyframes[j].minute;
            if (ti > tj) {
                LightKeyframe tmp = cfg.keyframes[i];
                cfg.keyframes[i] = cfg.keyframes[j];
                cfg.keyframes[j] = tmp;
            }
        }
    }

    cfg.keyframeCount = count;
    lightSchedulerSetConfig(cfg);
    server.sendHeader("Location", "/light");
    server.send(303);
}

// ============================================================================
// РУЧНЕ КЕРУВАННЯ
// ============================================================================
void handleLightManual() {
    if (!checkAuth()) return;
    uint8_t r = (uint8_t)constrain(server.arg("r").toInt(), 0, 255);
    uint8_t g = (uint8_t)constrain(server.arg("g").toInt(), 0, 255);
    uint8_t b = (uint8_t)constrain(server.arg("b").toInt(), 0, 255);
    uint8_t w = (uint8_t)constrain(server.arg("w").toInt(), 0, 255);
    dmxSetRGBW(r, g, b, w);
    server.sendHeader("Location", "/light");
    server.send(303);
}

void handleLightOff() {
    if (!checkAuth()) return;
    dmxSetRGBW(0, 0, 0, 0);
    server.sendHeader("Location", "/light");
    server.send(303);
}

// ============================================================================
// ВМИКАЧ РОЗКЛАДУ
// ============================================================================
void handleLightToggle() {
    if (!checkAuth()) return;
    LightScheduleConfig cfg = lightSchedulerGetConfig();
    cfg.enabled = (server.arg("enabled") == "1");
    lightSchedulerSetConfig(cfg);
    server.sendHeader("Location", "/light");
    server.send(303);
}

// ============================================================================
// СКИНУТИ ПРЕСЕТ
// ============================================================================
void handleLightPreset() {
    if (!checkAuth()) return;
    lightSchedulerLoadDefaultPreset();
    lightSchedulerSave();
    server.sendHeader("Location", "/light");
    server.send(303);
}
