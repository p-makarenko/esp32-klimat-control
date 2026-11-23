// ============================================================================
// WEB_PAGES_TC421.CPP - Окрема сторінка фітолампи TC421 (4 канали + Конструктор дня)
// ============================================================================

#include <WebServer.h>
#include "web_common.h"
#include "tc421_controller.h"
#include "energy_monitor.h"
#include "system_core.h"

extern WebServer server;
extern bool checkAuth();

static String fmtHHMM(uint8_t h, uint8_t m) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02u:%02u", h, m);
    return String(buf);
}

static String buildStatusJson() {
    TC421State tc = tc421GetState();
    TC421ScheduleConfig sc = tc421GetSchedule();
    EnergyMeasurements em = getEnergyMeasurements();
    bool ecoNow = isEcoModeActive();

    String j = "{";
    j += "\"found\":" + String(tc.found ? "true" : "false") + ",";
    j += "\"liveMode\":" + String(tc.liveMode ? "true" : "false") + ",";
    j += "\"autoEnabled\":" + String(sc.enabled ? "true" : "false") + ",";
    j += "\"ecoNow\":" + String(ecoNow ? "true" : "false") + ",";
    j += "\"ch\":[" + String(tc.channels[0]) + "," + String(tc.channels[1]) + "," +
                      String(tc.channels[2]) + "," + String(tc.channels[3]) + "],";
    j += "\"power\":" + String(em.power, 1) + ",";
    j += "\"voltage\":" + String(em.voltage, 1) + ",";
    j += "\"current\":" + String(em.current, 2) + ",";
    j += "\"pzemOk\":" + String(em.error ? "false" : "true");
    j += "}";
    return j;
}

// ============================================================================
// ГОЛОВНА СТОРІНКА /tc421
// ============================================================================
void handleTC421Page() {
    if (!checkAuth()) return;

    TC421State tc = tc421GetState();
    TC421ScheduleConfig sc = tc421GetSchedule();
    EnergyMeasurements em = getEnergyMeasurements();
    bool ecoNow = isEcoModeActive();

    struct tm* nowTm = getTimeInfo();
    int curMinute = (nowTm != nullptr) ? (nowTm->tm_hour * 60 + nowTm->tm_min) : -1;

    String html = getHtmlHead("Фітолампа TC421");
    html.reserve(18000);
    html += "<style>"
            ".tc-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(135px,1fr));gap:10px;margin:12px 0}"
            ".tc-tile{background:var(--s);border:1px solid var(--b);border-top:3px solid var(--a);border-radius:10px;padding:12px;text-align:center}"
            ".tc-tile .t-lbl{font-size:12.5px;color:var(--m);font-weight:600}"
            ".tc-tile .t-val{font-size:23px;font-weight:700;margin-top:4px;font-variant-numeric:tabular-nums}"
            ".tc-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(175px,1fr));gap:12px;margin:10px 0}"
            ".tc-field{background:var(--s);border:1px solid var(--b);border-radius:10px;padding:10px 12px}"
            ".tc-field label{display:flex;justify-content:space-between;align-items:center;gap:6px;font-size:12.5px;color:var(--m);margin-bottom:6px;font-weight:600}"
            ".tc-field input[type=time],.tc-field input[type=number]{width:100%}"
            ".tc-hint{font-size:11.5px;color:var(--m);margin-top:5px;line-height:1.35}"
            ".tc-q{display:inline-flex;align-items:center;justify-content:center;width:18px;height:18px;border-radius:50%;background:var(--s2);border:1px solid var(--b);color:var(--a);font-size:11px;font-weight:700;cursor:pointer;flex:0 0 auto}"
            ".tc-q:hover{background:var(--ah);color:#fff}"
            ".tc-helpbox{display:none;margin-top:6px;padding:7px 9px;background:var(--s2);border-left:3px solid var(--a);border-radius:6px;font-size:12px;color:var(--t);line-height:1.4}"
            ".tc-helpbox.open{display:block}"
            ".tc-svg-wrap{background:var(--s);border:1px solid var(--b);border-radius:10px;padding:10px;margin-top:10px}"
            ".tc-legend{display:flex;flex-wrap:wrap;gap:12px;font-size:12.5px;color:var(--m);margin-top:8px}"
            ".tc-dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:5px;vertical-align:middle}"
            ".tc-badge{display:inline-block;padding:3px 10px;border-radius:99px;font-size:12px;font-weight:600}"
            ".tc-watt-bar{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:10px;padding:10px 14px;background:var(--s);border:1px solid var(--b);border-left:3px solid #38bdf8;border-radius:10px;margin:10px 0}"
            ".tc-toast{display:inline-block;margin-left:8px;font-size:13px;font-weight:600;color:#10b981}"
            "</style>";

    html += "<div class='container'>";
    html += getNavHeader("🔆 ФІТОЛАМПА TC421");

    // ------------------------------------------------------------------------
    // КАРТКА 1: СТАТУС, ПОТУЖНІСТЬ ТА 4 ПІДПИСАНІ КАНАЛИ
    // ------------------------------------------------------------------------
    html += "<div class='card' style='border-left-color:#10b981'>";
    html += "<div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px'>";
    html += "<div><b>Стан контролера</b> ";
    if (tc.found) {
        html += "<span class='tc-badge' style='background:rgba(16,185,129,.2);color:#10b981'>На зв'язку</span> ";
    } else {
        html += "<span class='tc-badge' style='background:rgba(239,68,68,.2);color:#ef4444'>Не знайдено</span> ";
    }
    html += "<span id='modeBadge' class='tc-badge' style='background:rgba(56,189,248,.2);color:#38bdf8'>";
    if (sc.enabled)      html += "Автопілот ESP32";
    else if (tc.liveMode) html += "Ручний режим";
    else                 html += "Внутр. розклад TC421";
    html += "</span> ";
    if (ecoNow) {
        html += "<span class='tc-badge' style='background:rgba(239,68,68,.25);color:#fb7185'>🔋 ЕКО: макс " + String(sc.ecoMaxPercent) + "%</span>";
    }
    html += "</div>";
    html += "<div>";
    html += "<a href='/tc421/discover' class='btn' style='padding:7px 12px;font-size:13px'>🔍 Знайти</a>";
    html += "<a href='/tc421/synctime' class='btn btn-secondary' style='padding:7px 12px;font-size:13px'>🕒 Синхр. час</a>";
    html += "</div></div>";

    html += "<div style='font-size:13px;color:var(--m);margin-top:6px'>IP: <b>" +
            String(strlen(tc.ip) > 0 ? tc.ip : "—") + "</b> | MAC: <b>" +
            String(strlen(tc.mac) > 0 ? tc.mac : "—") + "</b> | Мережа: <b>" + String(LIGHT_AP_SSID) + "</b></div>";

    // 4 плитки каналів + 5-та плитка потужності (Вт)
    html += "<div class='tc-grid'>";
    html += "<div class='tc-tile' style='border-top-color:#ef4444'><div class='t-lbl'>🔴 CH1 Червоний</div><div id='tile0' class='t-val' style='color:#ef4444'>" + String(tc.channels[0]) + "%</div></div>";
    html += "<div class='tc-tile' style='border-top-color:#e2e8f0'><div class='t-lbl'>⚪ CH2 Білий</div><div id='tile1' class='t-val' style='color:var(--t)'>" + String(tc.channels[1]) + "%</div></div>";
    html += "<div class='tc-tile' style='border-top-color:#f59e0b'><div class='t-lbl'>🟡 CH3 Жовтий</div><div id='tile2' class='t-val' style='color:#f59e0b'>" + String(tc.channels[2]) + "%</div></div>";
    html += "<div class='tc-tile' style='border-top-color:#a855f7'><div class='t-lbl'>🟣 CH4 IR + UV</div><div id='tile3' class='t-val' style='color:#a855f7'>" + String(tc.channels[3]) + "%</div></div>";
    html += "<div class='tc-tile' style='border-top-color:#38bdf8'><div class='t-lbl'>⚡ Потужність</div><div id='tileW' class='t-val' style='color:#38bdf8'>" +
            (em.error ? String("—") : (String(em.power, 1) + " Вт")) + "</div></div>";
    html += "</div>";

    // Згорнутий блок ручного IP/MAC
    html += "<details style='margin-top:8px'><summary style='cursor:pointer;font-size:13px;color:var(--m)'>Налаштування IP / MAC вручну</summary>";
    html += "<form method='POST' action='/tc421/target' style='display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-top:8px'>";
    html += "<input type='text' name='ip' placeholder='IP (напр. 192.168.10.2)' value='" + String(tc.ip) + "' style='width:180px'>";
    html += "<input type='text' name='mac' placeholder='MAC (12 hex)' value='" + String(tc.mac) + "' style='width:160px'>";
    html += "<button type='submit' class='btn btn-secondary' style='padding:8px 12px'>Зберегти IP</button>";
    html += "</form></details>";
    html += "</div>";

    // ------------------------------------------------------------------------
    // КАРТКА 2: РУЧНЕ КЕРУВАННЯ В РЕАЛЬНОМУ ЧАСІ + ВАТИ (БЕЗ ПЕРЕЗАВАНТАЖЕННЯ!)
    // ------------------------------------------------------------------------
    html += "<div class='card' style='border-left-color:#a855f7'>";
    html += "<div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px'>";
    html += "<b>🎛️ Ручне керування (миттєва реакція)</b>";
    html += "<span id='liveToast' class='tc-toast'></span>";
    html += "</div>";
    html += "<div class='tc-hint'>Тягни будь-який повзунок — яскравість лампи змінюється одразу без перезавантаження сторінки. Автопілот при цьому стає на паузу.</div>";

    // Панель поточної потужності в Ватах + дельта
    html += "<div class='tc-watt-bar'>";
    html += "<div>⚡ Зараз по мережі: <b id='liveWatt' style='font-size:1.25em;color:#38bdf8'>" +
            (em.error ? String("—") : (String(em.power, 1) + " Вт")) + "</b> "
            "<span id='liveDelta' style='font-size:13px;color:#10b981;font-weight:600'></span></div>";
    html += "<div style='display:flex;gap:6px;flex-wrap:wrap'>";
    html += "<button type='button' class='btn btn-secondary' style='padding:6px 10px;font-size:12.5px' onclick='setWattZero()' title='Запам&#39;ятати поточні Вати як 0, щоб бачити чисте споживання лампи'>🎯 Зафіксувати фон</button>";
    html += "<button type='button' class='btn btn-danger' style='padding:6px 10px;font-size:12.5px' onclick='liveOff()'>Вимкнути все (0%)</button>";
    html += "<button type='button' class='btn btn-secondary' style='padding:6px 10px;font-size:12.5px' onclick='liveInternal()'>На внутр. розклад TC421</button>";
    html += "</div></div>";

    html += "<div class='tc-row'>";
    const char* mNames[4] = {"🔴 CH1 Червоний", "⚪ CH2 Білий", "🟡 CH3 Жовтий", "🟣 CH4 IR + UV"};
    const char* mHints[4] = {
        "Основний спектр для фотосинтезу та цвітіння (~660 нм)",
        "Холодний/повний білий — компактний ріст і вегетація",
        "Теплий спектр — природний відтінок і набір біомаси",
        "Далекий червоний (сон/пробудження) + УФ (смоли/захист)"
    };
    for (int i = 0; i < 4; i++) {
        String id = "m_c" + String(i);
        html += "<div class='tc-field'><label><span>" + String(mNames[i]) + "</span><b id='" + id + "v'>" + String(tc.channels[i]) + "%</b></label>";
        html += "<input class='slider' style='margin:6px 0' type='range' id='" + id + "' min='0' max='100' value='" + String(tc.channels[i]) + "' oninput='onManualSlide(" + String(i) + ",this.value)' onchange='onManualCommit()'>";
        html += "<div class='tc-hint'>" + String(mHints[i]) + "</div>";
        html += "</div>";
    }
    html += "</div></div>";

    // ------------------------------------------------------------------------
    // КАРТКА 3: ЖИВИЙ ГРАФІК ДОБИ (SVG 00:00 - 24:00)
    // ------------------------------------------------------------------------
    html += "<div class='card' style='border-left-color:#38bdf8'>";
    html += "<b>📈 Крива освітлення протягом доби (00:00 – 24:00)</b>";
    html += "<div class='tc-svg-wrap'><svg id='tcChart' viewBox='0 0 720 220' style='width:100%;height:auto;display:block'></svg></div>";
    html += "<div class='tc-legend'>"
            "<span><i class='tc-dot' style='background:#ef4444'></i>CH1 Червоний</span>"
            "<span><i class='tc-dot' style='background:#cbd5e1'></i>CH2 Білий</span>"
            "<span><i class='tc-dot' style='background:#f59e0b'></i>CH3 Жовтий</span>"
            "<span><i class='tc-dot' style='background:#a855f7'></i>CH4 IR + UV</span>"
            "<span><i class='tc-dot' style='background:#22d3ee'></i>Зараз</span>"
            "<span><i class='tc-dot' style='background:#fb7185'></i>Ліміт ЕКО</span>"
            "</div>";
    html += "</div>";

    // ------------------------------------------------------------------------
    // КАРТКА 4: КОНСТРУКТОР ПРИРОДНОГО ДНЯ З ПІДКАЗКАМИ ТА 4 ПРОФІЛЯМИ
    // ------------------------------------------------------------------------
    uint8_t actProf = tc421GetActiveProfile();
    const char* profIcons[4] = {"🌱 ", "🌸 ", "☀️ ", "⭐ "};
    html += "<div class='card' style='border-left-color:#f59e0b'>";
    html += "<div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:10px'>";
    html += "<b>🌅 Конструктор природного дня <span class='tc-q' onclick='togH(\"h_main\")'>?</span></b>";
    html += "<div style='display:flex;flex-wrap:wrap;gap:6px'>";
    for (uint8_t p = 0; p < TC421_PROFILE_COUNT; p++) {
        String cls = (p == actProf) ? "btn" : "btn btn-secondary";
        html += "<a href='/tc421/preset?p=" + String(p) + "' class='" + cls + "' style='padding:6px 10px;font-size:12.5px'>" +
                String(profIcons[p]) + tc421GetProfileName(p) + "</a>";
    }
    html += "</div></div>";
    html += "<div id='h_main' class='tc-helpbox'>Кожен із 4 профілів (<b>Вегетація</b>, <b>Цвітіння</b>, <b>Природний</b> і <b>⭐ Мій профіль</b>) зберігає твої зміни окремо в пам'яті ESP32! Перемикаючись між профілями, ти не втрачаєш свої налаштування (наприклад, нічний тариф о 23:00). За потреби поточний профіль можна скинути до заводських.</div>";

    html += "<form method='POST' action='/tc421/schedule/save' id='tcForm' onsubmit='return saveScheduleAjax(event)'>";
    html += "<div style='display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:10px;margin:10px 0 12px'>";
    html += "<label style='display:inline-flex;align-items:center;gap:8px;font-size:15px;font-weight:600;cursor:pointer'>";
    html += "<input type='checkbox' id='f_en' name='enabled' value='1' " + String(sc.enabled ? "checked" : "") + "> Автопілот ESP32 активний</label>";
    html += "<div style='display:inline-flex;align-items:center;gap:6px'><span style='font-size:12.5px;color:var(--m)'>Назва профілю:</span>";
    html += "<input type='text' name='prof_name' maxlength='26' value='" + String(sc.name) + "' style='width:170px;padding:6px 10px;font-size:13px'></div>";
    html += "</div>";

    // Час світанку/заходу
    html += "<div class='tc-row'>";
    html += "<div class='tc-field'><label><span>🌅 Початок світанку</span><span class='tc-q' onclick='togH(\"h_time\")'>?</span></label><input type='time' id='f_sr' name='sr_time' value='" + fmtHHMM(sc.sunriseHour, sc.sunriseMin) + "' oninput='drawChart()'></div>";
    html += "<div class='tc-field'><label><span>🌇 Кінець заходу</span></label><input type='time' id='f_ss' name='ss_time' value='" + fmtHHMM(sc.sunsetHour, sc.sunsetMin) + "' oninput='drawChart()'></div>";
    html += "<div class='tc-field'><label><span>⏱ Світанок (хв)</span><span class='tc-q' onclick='togH(\"h_ramp\")'>?</span></label><input type='number' id='f_srd' name='sr_dur' min='5' max='120' value='" + String(sc.sunriseDurMin) + "' oninput='drawChart()'></div>";
    html += "<div class='tc-field'><label><span>⏱ Захід (хв)</span></label><input type='number' id='f_ssd' name='ss_dur' min='5' max='120' value='" + String(sc.sunsetDurMin) + "' oninput='drawChart()'></div>";
    html += "</div>";
    html += "<div id='h_time' class='tc-helpbox'><b>Фотоперіод:</b> задай час початку світанку та повного завершення заходу. Підтримується перехід через північ (напр. 06:00 → 00:00 для 18/6 або нічний графік 20:00 → 08:00).</div>";
    html += "<div id='h_ramp' class='tc-helpbox'><b>Природний каскад:</b> на світанку спершу плавно вмикаються Червоний і Жовтий, а Білий підключається із затримкою 25% — без світлового шоку. На заході Білий гасне першим, залишаючи теплий вечірній спектр. Рекомендовано 30–60 хв.</div>";

    // 4 повзунки денного максимуму
    html += "<h3 style='margin:14px 0 6px'>Денний максимум каналів <span class='tc-q' onclick='togH(\"h_day\")'>?</span></h3>";
    html += "<div id='h_day' class='tc-helpbox'>Яскравість кожного каналу в розпал дня (пік). Для вегетації зазвичай більше <b>Білого</b> (75–90%), для цвітіння — більше <b>Червоного</b> (85–100%) і <b>Жовтого</b>. <b>CH4 (IR+UV)</b> вдень працює тільки в «Полуденному вікні».</div>";
    html += "<div class='tc-row'>";
    const char* chNames[4] = {"🔴 CH1 Червоний", "⚪ CH2 Білий", "🟡 CH3 Жовтий", "🟣 CH4 IR+UV (полудень)"};
    const char* chCols[4]  = {"#ef4444", "#cbd5e1", "#f59e0b", "#a855f7"};
    for (int i = 0; i < 4; i++) {
        String id = "f_d" + String(i);
        html += "<div class='tc-field'><label><span>" + String(chNames[i]) + "</span><b id='" + id + "v' style='color:" + String(chCols[i]) + "'>" + String(sc.dayMax[i]) + "%</b></label>";
        html += "<input class='slider' style='margin:6px 0' type='range' id='" + id + "' name='d" + String(i) + "' min='0' max='100' value='" + String(sc.dayMax[i]) + "' oninput='document.getElementById(\"" + id + "v\").textContent=this.value+\"%\";drawChart()'>";
        html += "</div>";
    }
    html += "</div>";

    // Біологічні імпульси CH4 (IR+UV)
    html += "<h3 style='margin:14px 0 6px'>🟣 Імпульси сходу / полудня / заходу (CH4 IR+UV) <span class='tc-q' onclick='togH(\"h_ir\")'>?</span></h3>";
    html += "<div id='h_ir' class='tc-helpbox'>"
            "• <b>Світанок IR (10–20 хв, 25–40%):</b> м'який імпульс на старті дня (ефект Емерсона) — допомагає рослині плавно «прокинутись».<br>"
            "• <b>Полуденне вікно UV/IR (2–4 год, 0=вимк):</b> вмикає CH4 в самому центрі дня (як сонце в зеніті), не перевантажуючи листя ультрафіолетом цілий день.<br>"
            "• <b>Захід EOD Far-Red (15–20 хв після заходу, 40–50%):</b> «сигнал сну» — швидко переводить фітохроми у нічну фазу. Не став більше 30 хв, щоб рослини не витягувались."
            "</div>";
    html += "<div class='tc-row'>";
    html += "<div class='tc-field'><label><span>Світанок IR (хв)</span></label><input type='number' id='f_dird' name='dir_dur' min='0' max='45' value='" + String(sc.dawnIrDurMin) + "' oninput='drawChart()'><div class='tc-hint'>Реком. 15 хв (0 = вимк)</div></div>";
    html += "<div class='tc-field'><label><span>Світанок IR (%)</span></label><input type='number' id='f_dirp' name='dir_pct' min='0' max='100' value='" + String(sc.dawnIrPct) + "' oninput='drawChart()'><div class='tc-hint'>Реком. 30–35%</div></div>";
    html += "<div class='tc-field'><label><span>Полудень UV/IR (год)</span></label><input type='number' id='f_muv' name='mid_uv' min='0' max='12' value='" + String(sc.middayUvHours) + "' oninput='drawChart()'><div class='tc-hint'>У центрі дня (0 = вимк)</div></div>";
    html += "<div class='tc-field'><label><span>Захід EOD IR (хв)</span></label><input type='number' id='f_eodd' name='eod_dur' min='0' max='45' value='" + String(sc.eodIrDurMin) + "' oninput='drawChart()'><div class='tc-hint'>Після заходу: 15–20 хв</div></div>";
    html += "<div class='tc-field'><label><span>Захід EOD IR (%)</span></label><input type='number' id='f_eodp' name='eod_pct' min='0' max='100' value='" + String(sc.eodIrPct) + "' oninput='drawChart()'><div class='tc-hint'>Реком. 40–50%</div></div>";
    html += "</div>";

    // ЕКО-режим при блекауті
    html += "<div class='tc-field' style='margin-top:10px;border-left:3px solid #fb7185'>";
    html += "<label><span>🔋 ЕКО-режим (під час блекауту / АКБ) — макс. потужність</span><b id='f_ecov' style='color:#fb7185'>" + String(sc.ecoMaxPercent) + "%</b></label>";
    html += "<input class='slider' style='margin:6px 0' type='range' id='f_eco' name='eco_max' min='0' max='100' value='" + String(sc.ecoMaxPercent) + "' oninput='document.getElementById(\"f_ecov\").textContent=this.value+\"%\";drawChart()'>";
    html += "<div class='tc-hint'>Коли зникає 220В, лампа за 5 сек знижує потужність до цього ліміту (зберігаючи пропорції спектру). <b>0%</b> = повністю гасити; <b>10–20%</b> = мінімум вдень, щоб рослини не збили свій біоритм (вночі завжди 0%).</div>";
    html += "</div>";

    html += "<div style='margin-top:14px;display:flex;align-items:center;flex-wrap:wrap;gap:8px'>";
    html += "<button type='submit' class='btn btn-success'>💾 Зберегти профіль</button>";
    html += "<a href='/tc421/preset/reset' class='btn btn-secondary' style='font-size:13px' onclick='return confirm(\"Скинути поточний профіль до заводських налаштувань?\")'>↺ Скинути до заводських</a>";
    html += "<span id='saveToast' class='tc-toast'></span>";
    html += "</div></form></div>";

    // ------------------------------------------------------------------------
    // JS: LIVE ПОВЗУНКИ (AJAX), ПОТУЖНІСТЬ У ВАТАХ, ПІДКАЗКИ ТА SVG-ГРАФІК
    // ------------------------------------------------------------------------
    html += "<script>";
    html += "const CUR_MIN=" + String(curMinute) + ";";
    html += "let baseWatt=null, lastPower=0, slideTimer=null, lastSendMs=0;";

    // Перемикач підказок ?
    html += "function togH(id){const el=document.getElementById(id);if(el)el.classList.toggle('open');}";

    // Оновлення UI з JSON статусу
    html += "function applyStatus(d){"
            "if(!d)return;"
            "if(d.ch){for(let i=0;i<4;i++){const t=document.getElementById('tile'+i);if(t)t.textContent=d.ch[i]+'%';}}"
            "const b=document.getElementById('modeBadge');"
            "if(b){b.textContent=d.autoEnabled?'Автопілот ESP32':(d.liveMode?'Ручний режим':'Внутр. розклад TC421');}"
            "const en=document.getElementById('f_en');if(en&&document.activeElement!==en)en.checked=!!d.autoEnabled;"
            "if(d.pzemOk){"
            "  lastPower=d.power;"
            "  const wStr=d.power.toFixed(1)+' Вт';"
            "  const tw=document.getElementById('tileW');if(tw)tw.textContent=wStr;"
            "  const lw=document.getElementById('liveWatt');if(lw)lw.textContent=wStr;"
            "  const ld=document.getElementById('liveDelta');"
            "  if(ld&&baseWatt!==null){const df=d.power-baseWatt;ld.textContent=' ('+(df>=0?'+':'')+df.toFixed(1)+' Вт від фону)';}"
            "}}";

    html += "function setWattZero(){baseWatt=lastPower;const ld=document.getElementById('liveDelta');if(ld)ld.textContent=' (+0.0 Вт від фону)';}";

    html += "function pollStatus(){fetch('/tc421/api/status').then(r=>r.json()).then(applyStatus).catch(()=>{});}";
    html += "setInterval(pollStatus, 1500);";

    // Live-повзунки (throttle 120ms, без перезавантаження сторінки!)
    html += "function sendManual(saveFlag){"
            "const c0=document.getElementById('m_c0').value,c1=document.getElementById('m_c1').value,"
            "c2=document.getElementById('m_c2').value,c3=document.getElementById('m_c3').value;"
            "fetch(`/tc421/api/manual?ch1=${c0}&ch2=${c1}&ch3=${c2}&ch4=${c3}&save=${saveFlag?1:0}`)"
            ".then(r=>r.json()).then(d=>{applyStatus(d);const t=document.getElementById('liveToast');if(t){t.textContent=saveFlag?'✅ Застосовано':'⚡ Live';}"
            "setTimeout(pollStatus, 450);}).catch(()=>{});}";

    html += "function onManualSlide(idx,val){"
            "document.getElementById('m_c'+idx+'v').textContent=val+'%';"
            "document.getElementById('tile'+idx).textContent=val+'%';"
            "const now=Date.now();"
            "if(now-lastSendMs>=120){lastSendMs=now;sendManual(false);}"
            "else{clearTimeout(slideTimer);slideTimer=setTimeout(()=>{lastSendMs=Date.now();sendManual(false);},120);}}";

    html += "function onManualCommit(){clearTimeout(slideTimer);sendManual(true);}";

    html += "function liveOff(){"
            "for(let i=0;i<4;i++){document.getElementById('m_c'+i).value=0;document.getElementById('m_c'+i+'v').textContent='0%';}"
            "sendManual(true);}";

    html += "function liveInternal(){"
            "fetch('/tc421/internal').then(()=>{pollStatus();const t=document.getElementById('liveToast');if(t)t.textContent='✅ Внутр. розклад TC421';});}";

    // AJAX збереження Конструктора дня (без стрибка сторінки вгору!)
    html += "function saveScheduleAjax(e){"
            "e.preventDefault();"
            "const fd=new FormData(document.getElementById('tcForm'));"
            "const body=new URLSearchParams(fd).toString();"
            "fetch('/tc421/schedule/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})"
            ".then(()=>{const t=document.getElementById('saveToast');if(t){t.textContent='✅ Збережено та застосовано!';setTimeout(()=>{t.textContent='';},3000);}setTimeout(pollStatus,400);})"
            ".catch(()=>{document.getElementById('tcForm').submit();});"
            "return false;}";

    // SVG-графік
    html += "function ss01(x){return x<=0?0:(x>=1?1:x*x*(3-2*x));}";
    html += "function parseHM(s){if(!s||s.length<5)return 0;const p=s.split(':');return (+p[0])*60+(+p[1]);}";
    html += "function calcAt(cfg,m){"
            "let dayLen=cfg.end-cfg.start;if(dayLen<=0)dayLen+=1440;if(dayLen<30)dayLen=30;"
            "let t=m-cfg.start;if(t<0)t+=1440;"
            "let sr=cfg.srDur,ss=cfg.ssDur;if(sr+ss>dayLen){sr=Math.floor(dayLen/2);ss=dayLen-sr;}"
            "const ssStart=dayLen-ss;"
            "let r=0,w=0,y=0,ir=0;"
            "if(t>=0&&t<dayLen){"
            "  if(t<sr){const p=sr>0?t/sr:1;const pw=ss01(p),pwh=ss01((p-0.25)/0.75);r=cfg.d0*pw;y=cfg.d2*pw;w=cfg.d1*pwh;}"
            "  else if(t<ssStart){r=cfg.d0;w=cfg.d1;y=cfg.d2;}"
            "  else{const p=ss>0?(t-ssStart)/ss:1;const pw=1-ss01(p),pwh=1-ss01(p/0.75);r=cfg.d0*pw;y=cfg.d2*pw;w=cfg.d1*pwh;}"
            "}"
            "if(cfg.dirDur>0&&cfg.dirPct>0&&t>=0&&t<cfg.dirDur){"
            "  const p=t/cfg.dirDur;const e=p<0.25?ss01(p/0.25):(p>0.75?ss01((1-p)/0.25):1);ir=Math.max(ir,cfg.dirPct*e);"
            "}"
            "if(cfg.midUv>0&&cfg.d3>0){"
            "  let wd=cfg.midUv*60,cl=ssStart-sr;if(cl>0&&wd>cl)wd=cl;"
            "  if(wd>0){const ws=Math.floor(dayLen/2-wd/2),we=ws+wd;"
            "    if(t>=ws&&t<we){const rm=wd>=40?15:Math.floor(wd/4);let e=1;"
            "      if(rm>0&&t<ws+rm)e=ss01((t-ws)/rm);else if(rm>0&&t>we-rm)e=ss01((we-t)/rm);"
            "      ir=Math.max(ir,cfg.d3*e);}"
            "  }"
            "}"
            "if(cfg.eodDur>0&&cfg.eodPct>0){"
            "  const ov=ss>=5?5:ss,es=dayLen-ov,ee=dayLen+cfg.eodDur,et=ee-es;"
            "  if(t>=es&&t<ee&&et>0){const p=(t-es)/et;const e=p<0.15?ss01(p/0.15):(p>0.85?ss01((1-p)/0.15):1);ir=Math.max(ir,cfg.eodPct*e);}"
            "}"
            "return [r,w,y,ir];}";
    html += "function drawChart(){"
            "const cfg={start:parseHM(document.getElementById('f_sr').value),end:parseHM(document.getElementById('f_ss').value),"
            "srDur:+document.getElementById('f_srd').value||0,ssDur:+document.getElementById('f_ssd').value||0,"
            "d0:+document.getElementById('f_d0').value,d1:+document.getElementById('f_d1').value,"
            "d2:+document.getElementById('f_d2').value,d3:+document.getElementById('f_d3').value,"
            "dirDur:+document.getElementById('f_dird').value||0,dirPct:+document.getElementById('f_dirp').value||0,"
            "midUv:+document.getElementById('f_muv').value||0,"
            "eodDur:+document.getElementById('f_eodd').value||0,eodPct:+document.getElementById('f_eodp').value||0,"
            "eco:+document.getElementById('f_eco').value||0};"
            "const W=720,H=220,PL=34,PR=10,PT=12,PB=24,PW=W-PL-PR,PH=H-PT-PB;"
            "let s='';"
            "for(let p=0;p<=100;p+=25){const y=PT+PH*(1-p/100);s+=`<line x1='${PL}' y1='${y}' x2='${W-PR}' y2='${y}' stroke='#334155' stroke-dasharray='3,3' stroke-width='1'/><text x='2' y='${y+4}' fill='#94a3b8' font-size='10'>${p}%</text>`;}"
            "for(let h=0;h<=24;h+=3){const x=PL+PW*(h/24);s+=`<line x1='${x}' y1='${PT}' x2='${x}' y2='${PT+PH}' stroke='#334155' stroke-width='1'/><text x='${x-10}' y='${H-6}' fill='#94a3b8' font-size='10'>${String(h).padStart(2,'0')}:00</text>`;}"
            "if(cfg.eco>0){const ey=PT+PH*(1-cfg.eco/100);s+=`<line x1='${PL}' y1='${ey}' x2='${W-PR}' y2='${ey}' stroke='#fb7185' stroke-dasharray='5,4' stroke-width='1.3'/>`;}"
            "const cols=['#ef4444','#cbd5e1','#f59e0b','#a855f7'];"
            "const pts=[[],[],[],[]];"
            "for(let m=0;m<=1440;m+=5){const v=calcAt(cfg,m%1440);const x=(PL+PW*(m/1440)).toFixed(1);"
            "  for(let c=0;c<4;c++){const y=(PT+PH*(1-v[c]/100)).toFixed(1);pts[c].push(x+','+y);}}"
            "for(let c=0;c<4;c++){s+=`<polyline fill='none' stroke='${cols[c]}' stroke-width='2.2' points='${pts[c].join(' ')}'/>`;}"
            "if(CUR_MIN>=0){const cx=(PL+PW*(CUR_MIN/1440)).toFixed(1);s+=`<line x1='${cx}' y1='${PT}' x2='${cx}' y2='${PT+PH}' stroke='#22d3ee' stroke-width='2'/>`;}"
            "document.getElementById('tcChart').innerHTML=s;}"
            "drawChart();";
    html += "</script>";

    html += "</div>"; // container
    html += getNavFooter();
    html += getHtmlFooter();
    server.send(200, "text/html; charset=utf-8", html);
}

// ============================================================================
// ХЕНДЛЕРИ /tc421/*
// ============================================================================

void handleTC421Discover() {
    if (!checkAuth()) return;
    tc421Discover(1500);
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421Target() {
    if (!checkAuth()) return;
    tc421SetTarget(server.arg("ip"), server.arg("mac"));
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421ScheduleSave() {
    if (!checkAuth()) return;

    TC421ScheduleConfig sc = tc421GetSchedule();
    sc.enabled = (server.arg("enabled") == "1");

    String pname = server.arg("prof_name");
    pname.trim();
    if (pname.length() > 0) {
        strncpy(sc.name, pname.c_str(), sizeof(sc.name) - 1);
        sc.name[sizeof(sc.name) - 1] = '\0';
    }

    String sr = server.arg("sr_time");
    if (sr.length() >= 5) {
        sc.sunriseHour = (uint8_t)sr.substring(0, 2).toInt();
        sc.sunriseMin  = (uint8_t)sr.substring(3, 5).toInt();
    }
    String ss = server.arg("ss_time");
    if (ss.length() >= 5) {
        sc.sunsetHour = (uint8_t)ss.substring(0, 2).toInt();
        sc.sunsetMin  = (uint8_t)ss.substring(3, 5).toInt();
    }

    sc.sunriseDurMin = (uint8_t)server.arg("sr_dur").toInt();
    sc.sunsetDurMin  = (uint8_t)server.arg("ss_dur").toInt();
    sc.dayMax[0]     = (uint8_t)server.arg("d0").toInt();
    sc.dayMax[1]     = (uint8_t)server.arg("d1").toInt();
    sc.dayMax[2]     = (uint8_t)server.arg("d2").toInt();
    sc.dayMax[3]     = (uint8_t)server.arg("d3").toInt();
    sc.dawnIrDurMin  = (uint8_t)server.arg("dir_dur").toInt();
    sc.dawnIrPct     = (uint8_t)server.arg("dir_pct").toInt();
    sc.middayUvHours = (uint8_t)server.arg("mid_uv").toInt();
    sc.eodIrDurMin   = (uint8_t)server.arg("eod_dur").toInt();
    sc.eodIrPct      = (uint8_t)server.arg("eod_pct").toInt();
    sc.ecoMaxPercent = (uint8_t)server.arg("eco_max").toInt();

    tc421SetSchedule(sc, true);
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421Preset() {
    if (!checkAuth()) return;
    String p = server.arg("p");
    if (p == "0" || p == "veg")         tc421SelectProfile(TC421_PRESET_VEG);
    else if (p == "1" || p == "bloom")  tc421SelectProfile(TC421_PRESET_BLOOM);
    else if (p == "2" || p == "natural") tc421SelectProfile(TC421_PRESET_NATURAL);
    else if (p == "3" || p == "custom") tc421SelectProfile(TC421_PRESET_CUSTOM);
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421ResetProfile() {
    if (!checkAuth()) return;
    tc421ResetActiveProfile();
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421Manual() {
    if (!checkAuth()) return;
    TC421ScheduleConfig sc = tc421GetSchedule();
    if (sc.enabled) {
        sc.enabled = false;
        tc421SetSchedule(sc, false);
    }
    uint8_t c1 = (uint8_t)constrain(server.arg("ch1").toInt(), 0, 100);
    uint8_t c2 = (uint8_t)constrain(server.arg("ch2").toInt(), 0, 100);
    uint8_t c3 = (uint8_t)constrain(server.arg("ch3").toInt(), 0, 100);
    uint8_t c4 = (uint8_t)constrain(server.arg("ch4").toInt(), 0, 100);
    tc421SetChannels(c1, c2, c3, c4, 0, true);
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421ApiManual() {
    if (!checkAuth()) return;
    TC421ScheduleConfig sc = tc421GetSchedule();
    if (sc.enabled) {
        sc.enabled = false;
        tc421SetSchedule(sc, false);
    }
    uint8_t c1 = (uint8_t)constrain(server.arg("ch1").toInt(), 0, 100);
    uint8_t c2 = (uint8_t)constrain(server.arg("ch2").toInt(), 0, 100);
    uint8_t c3 = (uint8_t)constrain(server.arg("ch3").toInt(), 0, 100);
    uint8_t c4 = (uint8_t)constrain(server.arg("ch4").toInt(), 0, 100);
    bool saveNvs = (server.arg("save") == "1");
    tc421SetChannels(c1, c2, c3, c4, 0, saveNvs);
    server.send(200, "application/json", buildStatusJson());
}

void handleTC421ApiStatus() {
    if (!checkAuth()) return;
    server.send(200, "application/json", buildStatusJson());
}

void handleTC421Off() {
    if (!checkAuth()) return;
    TC421ScheduleConfig sc = tc421GetSchedule();
    if (sc.enabled) {
        sc.enabled = false;
        tc421SetSchedule(sc, false);
    }
    tc421Off();
    server.sendHeader("Location", "/tc421");
    server.send(303);
}

void handleTC421Internal() {
    if (!checkAuth()) return;
    tc421ResumeSchedule();
    server.send(200, "application/json", buildStatusJson());
}

void handleTC421SyncTime() {
    if (!checkAuth()) return;
    tc421SyncTime();
    server.sendHeader("Location", "/tc421");
    server.send(303);
}
