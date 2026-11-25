// ============================================================================
// WEB_PAGES_AUDIT.CPP - Вбудований енерго- та світловий аудит (/audit)
// Працює автономно зі смартфона (через Phyphox) без ноутбука
// ============================================================================

#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include "web_common.h"
#include "tc421_controller.h"
#include "energy_monitor.h"
#include "system_core.h"

extern WebServer server;
extern bool checkAuth();

// ============================================================================
// ПРОКСІ ДЛЯ PHYPHOX (/audit/phyphox?host=10.183.20.114:8080)
// Обходить CORS-обмеження браузера смартфона
// ============================================================================
void handleAuditPhyphoxProxy() {
    if (!checkAuth()) return;

    String host = server.hasArg("host") ? server.arg("host") : "";
    host.trim();
    if (host.startsWith("http://")) host = host.substring(7);
    if (host.startsWith("https://")) host = host.substring(8);
    int slashIdx = host.indexOf('/');
    if (slashIdx > 0) host = host.substring(0, slashIdx);

    if (host.length() == 0) {
        IPAddress gw = WiFi.gatewayIP();
        if (gw[0] != 0) {
            host = gw.toString() + ":8080";
        } else {
            server.send(400, "application/json", "{\"ok\":false,\"err\":\"Вкажіть IP:порт Phyphox\"}");
            return;
        }
    }

    // Базова валідація хоста (дозволяємо лише IP/домен і порт)
    for (size_t i = 0; i < host.length(); i++) {
        char c = host[i];
        bool valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                     (c >= 'A' && c <= 'Z') || c == '.' || c == ':' || c == '-';
        if (!valid) {
            server.send(400, "application/json", "{\"ok\":false,\"err\":\"Некоректна адреса\"}");
            return;
        }
    }

    HTTPClient http;
    http.setConnectTimeout(1500);
    http.setTimeout(1800);
    String url = "http://" + host + "/get?illum";
    if (!http.begin(url)) {
        server.send(200, "application/json", "{\"ok\":false,\"err\":\"HTTP begin failed\",\"host\":\"" + host + "\"}");
        return;
    }

    int code = http.GET();
    if (code != 200) {
        http.end();
        server.send(200, "application/json",
                    "{\"ok\":false,\"err\":\"Phyphox не відповідає (HTTP " + String(code) + ")\",\"host\":\"" + host + "\"}");
        return;
    }

    String body = http.getString();
    http.end();

    // Парсимо значення lux із "buffer":[123.45]
    float lux = 0.0f;
    bool hasLux = false;
    int bufPos = body.indexOf("\"buffer\":[");
    if (bufPos >= 0) {
        int start = bufPos + 10;
        int end = body.indexOf(']', start);
        if (end > start) {
            String valStr = body.substring(start, end);
            valStr.trim();
            // Якщо в масиві кілька значень — беремо останнє
            int lastComma = valStr.lastIndexOf(',');
            if (lastComma >= 0) valStr = valStr.substring(lastComma + 1);
            valStr.trim();
            if (valStr.length() > 0 && valStr != "null") {
                lux = valStr.toFloat();
                hasLux = true;
            }
        }
    }

    bool measuring = (body.indexOf("\"measuring\":true") >= 0);

    // Якщо Phyphox відкритий, але на паузі — спробуємо запустити дистанційно
    if (!measuring && server.arg("autostart") == "1") {
        HTTPClient httpStart;
        httpStart.setConnectTimeout(800);
        httpStart.setTimeout(1000);
        if (httpStart.begin("http://" + host + "/control?cmd=start")) {
            httpStart.GET();
            httpStart.end();
        }
    }

    String j = "{\"ok\":true,\"host\":\"" + host + "\",\"lux\":" + String(lux, 2) +
               ",\"hasLux\":" + String(hasLux ? "true" : "false") +
               ",\"measuring\":" + String(measuring ? "true" : "false") + "}";
    server.send(200, "application/json", j);
}

// ============================================================================
// ЗБЕРЕЖЕННЯ / ЧИТАННЯ РЕКОМЕНДАЦІЙ АУДИТУ В NVS (/audit/profile)
// ============================================================================
void handleAuditProfileGet() {
    if (!checkAuth()) return;
    Preferences prefs;
    prefs.begin("audit", true);
    String json = prefs.getString("profile", "");
    prefs.end();
    if (json.length() == 0) {
        server.send(200, "application/json", "{\"saved\":false}");
    } else {
        server.send(200, "application/json", json);
    }
}

void handleAuditProfilePost() {
    if (!checkAuth()) return;
    String body = server.hasArg("plain") ? server.arg("plain") : "";
    body.trim();
    if (body.length() < 5 || body.length() > 1400 || body[0] != '{') {
        server.send(400, "application/json", "{\"ok\":false,\"err\":\"Invalid profile JSON\"}");
        return;
    }
    Preferences prefs;
    prefs.begin("audit", false);
    prefs.putString("profile", body);
    prefs.end();
    server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================================
// ЗБЕРЕЖЕННЯ / ЧИТАННЯ ПОВНОГО ГРАФІКА ОСТАННЬОГО ТЕСТУ В SPIFFS (/audit/data)
// ============================================================================
static const char* AUDIT_DATA_FILE = "/audit_last.json";

void handleAuditDataGet() {
    if (!checkAuth()) return;
    if (!SPIFFS.exists(AUDIT_DATA_FILE)) {
        server.send(200, "application/json", "{\"rows\":[]}");
        return;
    }
    File f = SPIFFS.open(AUDIT_DATA_FILE, "r");
    if (!f) {
        server.send(200, "application/json", "{\"rows\":[]}");
        return;
    }
    server.streamFile(f, "application/json");
    f.close();
}

void handleAuditDataPost() {
    if (!checkAuth()) return;
    String body = server.hasArg("plain") ? server.arg("plain") : "";
    if (body.length() < 5 || body.length() > 24000 || body[0] != '{') {
        server.send(400, "application/json", "{\"ok\":false,\"err\":\"Invalid audit data\"}");
        return;
    }
    File f = SPIFFS.open(AUDIT_DATA_FILE, "w");
    if (!f) {
        server.send(500, "application/json", "{\"ok\":false,\"err\":\"SPIFFS open failed\"}");
        return;
    }
    f.print(body);
    f.close();
    server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================================
// СТОРІНКА /audit
// ============================================================================
void handleAuditPage() {
    if (!checkAuth()) return;

    String defaultPhyphox = "";
    IPAddress gw = WiFi.gatewayIP();
    if (gw[0] != 0) {
        defaultPhyphox = gw.toString() + ":8080";
    } else {
        defaultPhyphox = "192.168.4.2:8080";
    }

    float curHmin = config.humidityConfig.minHumidity;
    float curHmax = config.humidityConfig.maxHumidity;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");

    String html = getHtmlHead("Енерго- та світловий аудит");
    auto flushChunk = [&]() {
        if (html.length() > 0) {
            server.sendContent(html);
            html = "";
        }
    };
    flushChunk();

    html += "<style>"
            ".au-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:10px;margin:12px 0}"
            ".au-tile{background:var(--s);border:1px solid var(--b);border-top:3px solid var(--a);border-radius:10px;padding:11px;text-align:center}"
            ".au-tile .lbl{font-size:12px;color:var(--m);font-weight:600}"
            ".au-tile .val{font-size:21px;font-weight:700;margin-top:4px;font-variant-numeric:tabular-nums}"
            ".au-checks{display:grid;grid-template-columns:repeat(auto-fit,minmax(165px,1fr));gap:8px;margin:10px 0}"
            ".au-chk{display:flex;align-items:center;gap:8px;padding:9px 11px;background:var(--s);border:1px solid var(--b);border-radius:9px;cursor:pointer;font-size:13.5px;font-weight:500}"
            ".au-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px;margin:10px 0}"
            ".au-field{background:var(--s);border:1px solid var(--b);border-radius:10px;padding:10px 12px}"
            ".au-field label{display:block;font-size:12.5px;color:var(--m);margin-bottom:5px;font-weight:600}"
            ".au-field input,.au-field select{width:100%}"
            ".prog-wrap{width:100%;height:12px;background:var(--s);border:1px solid var(--b);border-radius:99px;overflow:hidden;margin:10px 0}"
            ".prog-bar{height:100%;width:0%;background:linear-gradient(90deg,var(--ah),var(--ok));transition:width .25s}"
            ".au-tbl-wrap{overflow-x:auto;margin-top:10px}"
            ".au-tbl{font-size:13px;font-variant-numeric:tabular-nums}"
            ".au-best{background:rgba(16,185,129,.16);font-weight:700}"
            "#auChart{width:100%;height:260px;background:var(--s);border:1px solid var(--b);border-radius:10px;display:block;margin-top:10px}"
            ".au-log{background:#070a10;color:#cbd5e1;padding:10px;border-radius:8px;font-family:ui-monospace,Consolas,monospace;font-size:12px;max-height:170px;overflow-y:auto;white-space:pre-wrap;border:1px solid var(--b);margin-top:10px}"
            "</style>";

    html += "<div class='container'>";
    html += getNavHeader("🔬 Енерго- та світловий аудит");
    flushChunk();

    // Інструкція для смартфона без ПК
    html += "<div class='info-banner'>"
            "<b>📱 Працює без ноутбука:</b> відкрийте на смартфоні додаток <b>Phyphox → Світло → ⋮ Дозволити віддалений доступ → ▶ Play</b>, "
            "покладіть телефон екраном угору на висоті рослин і натисніть <b>«Запустити аудит»</b>. "
            "Після завершення (або натискання <b>Стоп</b>) система сама повернеться в <b>АВТО</b>."
            "</div>";

    // Налаштування тесту
    html += "<div class='card'><h2>⚙️ Параметри тесту</h2>";
    html += "<div class='au-row'>";
    html += "<div class='au-field'><label>📱 Адреса Phyphox (IP:порт)</label>"
            "<div style='display:flex;gap:6px'>"
            "<input type='text' id='phyHost' value='" + defaultPhyphox + "' placeholder='10.183.20.114:8080'>"
            "<button type='button' class='btn btn-secondary' style='margin:0;padding:8px 11px;white-space:nowrap' onclick='testPhyphox()'>🔌 Тест</button>"
            "</div>"
            "<div id='phyStatus' style='font-size:12px;color:var(--m);margin-top:5px'>Не перевірено</div></div>";

    html += "<div class='au-field'><label>⏱️ Пауза стабілізації PZEM (сек)</label>"
            "<input type='number' id='settleSec' min='3' max='15' step='1' value='6'>"
            "<div style='font-size:11.5px;color:var(--m);margin-top:4px'>Рекомендовано 6 с (PZEM оновлюється кожні 2 с)</div></div>";

    html += "<div class='au-field'><label>📊 Крок тесту світла</label>"
            "<select id='stepMode'>"
            "<option value='10' selected>Швидкий: 5, 10, 20..100% (11 точок)</option>"
            "<option value='5'>Детальний: 5, 10, 15..100% (20 точок)</option>"
            "<option value='20'>Експрес: 10, 20, 40, 60, 80, 100% (6 точок)</option>"
            "</select></div>";

    html += "<div class='au-field'><label>📏 Висота до лампи / Зсув (см)</label>"
            "<div style='display:flex;gap:6px'>"
            "<input type='number' id='lampH' min='5' max='300' step='1' value='40' title='Висота від датчика до лампи (см)' placeholder='Висота'>"
            "<input type='number' id='lampX' min='0' max='200' step='1' value='0' title='Зсув убік від центру (0 = центр)' placeholder='Зсув'>"
            "</div>"
            "<div style='font-size:11.5px;color:var(--m);margin-top:4px'>Висота H і зсув X для розрахунку кута світла</div></div>";
    html += "</div>";

    html += "<label style='display:block;font-size:13px;color:var(--m);font-weight:600;margin:8px 0 4px'>Що тестувати:</label>";
    html += "<div class='au-checks'>"
            "<label class='au-chk'><input type='checkbox' id='tCh1' checked>🔴 CH1 Червоний</label>"
            "<label class='au-chk'><input type='checkbox' id='tCh2' checked>⚪ CH2 Білий</label>"
            "<label class='au-chk'><input type='checkbox' id='tCh3' checked>🟡 CH3 Жовтий</label>"
            "<label class='au-chk'><input type='checkbox' id='tCh4' checked>🟣 CH4 IR+UV</label>"
            "<label class='au-chk'><input type='checkbox' id='tPump'>💧 Насос (A)</label>"
            "<label class='au-chk'><input type='checkbox' id='tFan'>🔥 Вентилятор (B)</label>"
            "<label class='au-chk'><input type='checkbox' id='tExt'>💨 Витяжка (C)</label>"
            "<label class='au-chk'><input type='checkbox' id='tPlant'>🌿 Вітер рослин</label>"
            "<label class='au-chk'><input type='checkbox' id='tHum'>🌫️ Зволожувач (100%)</label>"
            "</div>";

    html += "<label class='au-chk' style='margin-top:8px;border-left:3px solid var(--ok)'>"
            "<input type='checkbox' id='muteClimate' checked>"
            "<span>🔇 Вимикати кліматику (Quiet + Насос 0%) на час тесту для чистого фону</span></label>";

    html += "<div style='display:flex;flex-wrap:wrap;gap:8px;margin-top:12px'>"
            "<button type='button' id='btnStart' class='btn btn-success' onclick='startAudit()'>▶ Запустити аудит</button>"
            "<button type='button' id='btnStop' class='btn btn-danger' onclick='stopAudit()' disabled>⛔ Стоп і повернути АВТО</button>"
            "<button type='button' id='btnCsv' class='btn btn-secondary' onclick='downloadCsv()' disabled>📥 Завантажити CSV</button>"
            "<button type='button' id='btnClr' class='btn btn-secondary' onclick='clearSavedAudit()'>🗑️ Очистити</button>"
            "</div>";
    html += "</div>";
    flushChunk();

    // Живий стан
    html += "<div class='card'><h2>📡 Поточний стан тесту</h2>";
    html += "<div id='stepText' style='font-weight:600;color:var(--a)'>Готовий до запуску</div>";
    html += "<div class='prog-wrap'><div id='progBar' class='prog-bar'></div></div>";
    html += "<div class='au-grid'>"
            "<div class='au-tile' style='border-top-color:#f59e0b'><div class='lbl'>⚡ Всього / Чисті</div><div class='val' id='vPow'>—</div></div>"
            "<div class='au-tile' style='border-top-color:#38bdf8'><div class='lbl'>☀️ Освітленість</div><div class='val' id='vLux'>—</div></div>"
            "<div class='au-tile' style='border-top-color:#10b981'><div class='lbl'>📈 Ефективність</div><div class='val' id='vEff'>—</div></div>"
            "<div class='au-tile' style='border-top-color:#94a3b8'><div class='lbl'>🔌 Фон (Idle)</div><div class='val' id='vBase'>—</div></div>"
            "</div>";
    html += "<div id='auLog' class='au-log'>Лог тесту з'явиться тут після запуску...</div>";
    html += "</div>";

    // Графік та підсумки
    html += "<div class='card' id='resCard' style='display:none'><h2>📊 Результати та ефективний діапазон</h2>";
    html += "<div id='summaryBox' class='info-banner'></div>";
    html += "<div style='display:flex;gap:6px;flex-wrap:wrap;margin-top:8px'>"
            "<button type='button' class='tab-btn active' id='mEff' onclick=\"setChartMode('eff')\">📈 Люкс / Вт</button>"
            "<button type='button' class='tab-btn' id='mPow' onclick=\"setChartMode('pow')\">⚡ Чисті Вати</button>"
            "<button type='button' class='tab-btn' id='mLux' onclick=\"setChartMode('lux')\">☀️ Люкси</button>"
            "</div>";
    html += "<div style='display:flex;gap:5px;flex-wrap:wrap;margin-top:6px' id='chFilterBar'>"
            "<button type='button' class='quick-btn' style='border-color:var(--a);color:var(--a)' data-f='ALL' onclick=\"setChFilter('ALL',this)\">Усі</button>"
            "<button type='button' class='quick-btn' data-f='CH1 Червоний' onclick=\"setChFilter('CH1 Червоний',this)\">🔴 CH1</button>"
            "<button type='button' class='quick-btn' data-f='CH2 Білий' onclick=\"setChFilter('CH2 Білий',this)\">⚪ CH2</button>"
            "<button type='button' class='quick-btn' data-f='CH3 Жовтий' onclick=\"setChFilter('CH3 Жовтий',this)\">🟡 CH3</button>"
            "<button type='button' class='quick-btn' data-f='CH4 IR+UV' onclick=\"setChFilter('CH4 IR+UV',this)\">🟣 CH4</button>"
            "<button type='button' class='quick-btn' data-f='ACT' onclick=\"setChFilter('ACT',this)\">⚙️ Пристрої</button>"
            "</div>";
    html += "<div id='rangeBadge' style='font-size:13px;color:var(--ok);font-weight:600;margin-top:6px'></div>";
    html += "<canvas id='auChart' width='900' height='280'></canvas>";
    html += "<div class='au-tbl-wrap'><table class='au-tbl' id='resTbl'>"
            "<thead><tr><th>Пристрій</th><th>%</th><th>Фон Вт</th><th>Всього Вт</th><th>Чисті Вт</th><th>Люкс</th><th>Люкс/Вт</th></tr></thead>"
            "<tbody id='resBody'></tbody></table></div>";
    html += "</div>";
    flushChunk();

    // JavaScript логіка автономного тесту
    html += "<script>\n"
            "const ORIG_HMIN=" + String(curHmin, 1) + ",ORIG_HMAX=" + String(curHmax, 1) + ";\n"
            "let running=false,abortFlag=false,rows=[],chartMode='eff',chFilter='ALL',wakeLock=null;\n"
            "const CH_COLORS={'CH1 Червоний':'#ef4444','CH2 Білий':'#e2e8f0','CH3 Жовтий':'#f59e0b','CH4 IR+UV':'#a855f7','Насос (A)':'#38bdf8','Вентилятор (B)':'#fb7185','Витяжка (C)':'#10b981','Вітер рослин':'#34d399'};\n"
            "function log(m){const el=document.getElementById('auLog');const t=new Date().toLocaleTimeString('uk-UA');el.textContent+=`[${t}] ${m}\\n`;el.scrollTop=el.scrollHeight;}\n"
            "function sleep(ms){return new Promise(r=>setTimeout(r,ms));}\n"
            "async function sleepAbort(ms){const end=Date.now()+ms;while(Date.now()<end&&!abortFlag){await sleep(200);}}\n"
            "async function cmd(c){const f=new URLSearchParams();f.append('cmd',c);const r=await fetch('/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:f.toString()});return await r.text();}\n"
            "async function setQuiet(on,ttl=900){await fetch(`/api/quiet?on=${on?1:0}&ttl=${ttl}`);}\n"
            "async function setTC(c1,c2,c3,c4){const r=await fetch(`/tc421/api/manual?ch1=${c1}&ch2=${c2}&ch3=${c3}&ch4=${c4}&save=0`);return await r.json();}\n"
            "async function readEnergy(){const r=await fetch('/energy/api');return await r.json();}\n"
            "async function readPhyphox(autostart=0){const h=encodeURIComponent(document.getElementById('phyHost').value.trim());const r=await fetch(`/audit/phyphox?host=${h}&autostart=${autostart}`);return await r.json();}\n"
            "async function testPhyphox(){\n"
            "  const st=document.getElementById('phyStatus');st.textContent='Перевірка...';st.style.color='var(--m)';\n"
            "  try{const d=await readPhyphox(1);\n"
            "    if(d.ok){st.textContent=`✅ OK: ${d.lux.toFixed(1)} lx (${d.measuring?'йде замір':'на паузі'})`;st.style.color='var(--ok)';}\n"
            "    else{st.textContent=`❌ ${d.err}`;st.style.color='var(--e)';}\n"
            "  }catch(e){st.textContent='❌ Помилка запиту';st.style.color='var(--e)';}\n"
            "}\n"
            "function median(a){const s=[...a].sort((x,y)=>x-y);const m=Math.floor(s.length/2);return s.length%2?s[m]:(s[m-1]+s[m])/2;}\n"
            "async function measure(settleSec,withLux){\n"
            "  await sleepAbort(settleSec*1000);\n"
            "  if(abortFlag)return null;\n"
            "  const ps=[],ls=[];\n"
            "  for(let i=0;i<3;i++){\n"
            "    if(abortFlag)return null;\n"
            "    const e=await readEnergy();ps.push(Number(e.p)||0);\n"
            "    if(withLux){const ph=await readPhyphox(0);ls.push(ph.ok?(Number(ph.lux)||0):0);}\n"
            "    if(i<2)await sleepAbort(700);\n"
            "  }\n"
            "  return {p:Math.round(median(ps)*100)/100,lux:withLux?Math.round(median(ls)*10)/10:0};\n"
            "}\n";
    flushChunk();

    html += "function addRow(r){\n"
            "  rows.push(r);\n"
            "  document.getElementById('resCard').style.display='block';\n"
            "  document.getElementById('btnCsv').disabled=false;\n"
            "  const tb=document.getElementById('resBody');\n"
            "  const tr=document.createElement('tr');\n"
            "  tr.innerHTML=`<td>${r.dev}</td><td>${r.pct}%</td><td>${r.baseP.toFixed(1)}</td><td>${r.rawP.toFixed(1)}</td><td><b>${r.netP.toFixed(1)}</b></td><td>${r.cat==='light'?r.netL.toFixed(1):'—'}</td><td>${r.cat==='light'&&r.eff>0?r.eff.toFixed(1):'—'}</td>`;\n"
            "  tb.appendChild(tr);\n"
            "  drawChart();\n"
            "}\n"
            "function setChartMode(m){\n"
            "  chartMode=m;\n"
            "  ['mEff','mPow','mLux'].forEach(id=>document.getElementById(id).classList.remove('active'));\n"
            "  document.getElementById(m==='eff'?'mEff':m==='pow'?'mPow':'mLux').classList.add('active');\n"
            "  drawChart();\n"
            "}\n"
            "function setChFilter(f,btn){\n"
            "  chFilter=f;\n"
            "  document.querySelectorAll('#chFilterBar button').forEach(b=>{b.style.borderColor='';b.style.color='';});\n"
            "  if(btn){btn.style.borderColor='var(--a)';btn.style.color='var(--a)';}\n"
            "  if(f==='ACT'&&chartMode!=='pow')setChartMode('pow'); else drawChart();\n"
            "}\n"
            "function calcEffRange(arr){\n"
            "  const valid=arr.filter(x=>x.netP>=1.5&&x.netL>=5&&x.eff>0);\n"
            "  if(!valid.length)return null;\n"
            "  const maxEff=Math.max(...valid.map(x=>x.eff));\n"
            "  const peak=valid.reduce((a,b)=>b.eff>a.eff?b:a);\n"
            "  const good=valid.filter(x=>x.eff>=maxEff*0.75);\n"
            "  return {minPct:Math.min(...good.map(x=>x.pct)),maxPct:Math.max(...good.map(x=>x.pct)),peak:peak,maxEff:maxEff};\n"
            "}\n"
            "function drawChart(){\n"
            "  const c=document.getElementById('auChart');const ctx=c.getContext('2d');\n"
            "  const W=c.width,H=c.height;ctx.clearRect(0,0,W,H);\n"
            "  const pad={l:54,r:18,t:24,b:30},pw=W-pad.l-pad.r,ph=H-pad.t-pad.b;\n"
            "  const groups={};\n"
            "  rows.forEach(r=>{\n"
            "    if(chFilter==='ACT'){if(r.cat!=='actuator')return;}\n"
            "    else if(chFilter!=='ALL'){if(r.dev!==chFilter)return;}\n"
            "    else if(chartMode!=='pow'&&r.cat!=='light')return;\n"
            "    if(!groups[r.dev])groups[r.dev]=[];\n"
            "    groups[r.dev].push(r);\n"
            "  });\n"
            "  const rb=document.getElementById('rangeBadge');rb.textContent='';\n"
            "  const keys=Object.keys(groups);\n"
            "  if(keys.length===1&&groups[keys[0]][0].cat==='light'&&chartMode==='eff'){\n"
            "    const rng=calcEffRange(groups[keys[0]]);\n"
            "    if(rng){\n"
            "      const x1=pad.l+pw*(rng.minPct/100),x2=pad.l+pw*(rng.maxPct/100);\n"
            "      ctx.fillStyle='rgba(16,185,129,0.18)';ctx.fillRect(x1,pad.t,Math.max(8,x2-x1),ph);\n"
            "      ctx.strokeStyle='rgba(16,185,129,0.55)';ctx.setLineDash([4,4]);\n"
            "      ctx.beginPath();ctx.moveTo(x1,pad.t);ctx.lineTo(x1,pad.t+ph);ctx.moveTo(x2,pad.t);ctx.lineTo(x2,pad.t+ph);ctx.stroke();ctx.setLineDash([]);\n"
            "      rb.textContent=`🟢 Ефективний діапазон ${keys[0]}: ${rng.minPct}% – ${rng.maxPct}% (пік ${rng.peak.pct}% → ${rng.maxEff.toFixed(1)} lx/Вт)`;\n"
            "    }\n"
            "  }else if(keys.length>1&&chartMode==='eff'){\n"
            "    rb.textContent='💡 Натисніть на окремий канал (🔴 CH1 / ⚪ CH2 / 🟡 CH3 / 🟣 CH4), щоб підсвітити його зелений ефективний діапазон у повному масштабі.';\n"
            "  }\n"
            "  let maxY=1;\n"
            "  Object.values(groups).forEach(arr=>arr.forEach(r=>{\n"
            "    const v=chartMode==='eff'?r.eff:(chartMode==='pow'?r.netP:r.netL);\n"
            "    if(v>maxY)maxY=v;\n"
            "  }));\n"
            "  maxY*=1.12;\n"
            "  ctx.strokeStyle='#243047';ctx.lineWidth=1;ctx.font='11px system-ui';ctx.fillStyle='#94a3b8';\n"
            "  for(let i=0;i<=4;i++){\n"
            "    const y=pad.t+ph*(1-i/4);ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(W-pad.r,y);ctx.stroke();\n"
            "    ctx.fillText((maxY*i/4).toFixed(0),6,y+4);\n"
            "  }\n"
            "  for(let p=0;p<=100;p+=20){\n"
            "    const x=pad.l+pw*(p/100);ctx.fillText(p+'%',x-10,H-8);\n"
            "  }\n"
            "  let legX=pad.l;\n"
            "  Object.entries(groups).forEach(([name,arr])=>{\n"
            "    const col=CH_COLORS[name]||'#38bdf8';\n"
            "    ctx.strokeStyle=col;ctx.fillStyle=col;ctx.lineWidth=2.5;ctx.beginPath();\n"
            "    arr.forEach((r,i)=>{\n"
            "      const x=pad.l+pw*(r.pct/100);\n"
            "      const v=chartMode==='eff'?r.eff:(chartMode==='pow'?r.netP:r.netL);\n"
            "      const y=pad.t+ph*(1-v/maxY);\n"
            "      if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);\n"
            "    });\n"
            "    ctx.stroke();\n"
            "    arr.forEach(r=>{\n"
            "      const x=pad.l+pw*(r.pct/100);\n"
            "      const v=chartMode==='eff'?r.eff:(chartMode==='pow'?r.netP:r.netL);\n"
            "      const y=pad.t+ph*(1-v/maxY);\n"
            "      ctx.beginPath();ctx.arc(x,y,3.8,0,Math.PI*2);ctx.fill();\n"
            "    });\n"
            "    ctx.fillRect(legX,5,10,10);ctx.fillStyle='#cbd5e1';ctx.fillText(name,legX+14,14);legX+=Math.max(92,name.length*7+22);\n"
            "  });\n"
            "}\n";
    flushChunk();

    html += "function buildSummary(){\n"
            "  const box=document.getElementById('summaryBox');\n"
            "  const byDev={};rows.forEach(r=>{if(!byDev[r.dev])byDev[r.dev]=[];byDev[r.dev].push(r);});\n"
            "  let s='<b>🏆 Підсумки та оптимальні точки:</b><br>';\n"
            "  Object.entries(byDev).forEach(([dev,arr])=>{\n"
            "    if(arr[0].cat==='light'){\n"
            "      const rng=calcEffRange(arr);\n"
            "      const maxP=Math.max(...arr.map(x=>x.netP));\n"
            "      s+=rng?`• <b>${dev}:</b> ефективний діапазон <b>${rng.minPct}%–${rng.maxPct}%</b> (пік ${rng.peak.pct}%: ${rng.maxEff.toFixed(1)} lx/Вт | ${rng.peak.netL.toFixed(0)} lx / ${rng.peak.netP.toFixed(1)} Вт), макс. ${maxP.toFixed(1)} Вт<br>`:\n"
            "             `• <b>${dev}:</b> макс. ${maxP.toFixed(1)} Вт<br>`;\n"
            "    }else{\n"
            "      const maxR=arr[arr.length-1];\n"
            "      s+=`• <b>${dev}:</b> на ${maxR.pct}% бере <b>${maxR.netP.toFixed(1)} Вт</b><br>`;\n"
            "    }\n"
            "  });\n"
            "  box.innerHTML=s;\n"
            "  saveRunAndCalcOptics(byDev);\n"
            "  saveProfileToEsp(byDev);\n"
            "}\n"
            "async function saveProfileToEsp(byDev){\n"
            "  let prof={saved:true,ts:new Date().toLocaleString('uk-UA'),tc:{},dev:{}};\n"
            "  try{const prev=await (await fetch('/audit/profile')).json();if(prev&&prev.saved){prof.tc=prev.tc||{};prof.dev=prev.dev||{};}}catch(e){}\n"
            "  const chMap={'CH1 Червоний':'ch1','CH2 Білий':'ch2','CH3 Жовтий':'ch3','CH4 IR+UV':'ch4'};\n"
            "  Object.entries(byDev).forEach(([name,arr])=>{\n"
            "    if(arr[0].cat==='light'&&chMap[name]){\n"
            "      const k=chMap[name];\n"
            "      const valid=arr.filter(x=>x.netP>=1.5&&x.netL>=5);\n"
            "      let best=valid.length?valid.reduce((a,b)=>b.eff>a.eff?b:a):null;\n"
            "      if(k==='ch4'){const safe=arr.filter(x=>x.pct>=25&&x.pct<=45);best=safe.length?safe[Math.floor(safe.length/2)]:best;}\n"
            "      const maxP=Math.max(...arr.map(x=>x.netP));\n"
            "      if(best){prof.tc[k]={pct:best.pct,w:best.netP,eff:best.eff,maxW:maxP};}\n"
            "    }else if(name==='Насос (A)'){\n"
            "      const maxW=Math.max(...arr.map(x=>x.netP));\n"
            "      let opt=arr[0];\n"
            "      for(const r of arr){if(r.netP<=Math.max(8,maxW*0.45))opt=r;}\n"
            "      prof.dev.pump={optPct:opt.pct,optW:opt.netP,maxW:maxW};\n"
            "    }else if(name==='Витяжка (C)'){\n"
            "      const maxR=arr[arr.length-1];\n"
            "      const mid=arr.find(x=>x.pct>=40)||maxR;\n"
            "      const flat=(maxR.netP>0&&(maxR.netP-mid.netP)/maxR.netP<0.18);\n"
            "      prof.dev.ext={optPct:flat?80:mid.pct,optW:flat?maxR.netP:mid.netP,maxW:maxR.netP,flat:flat};\n"
            "    }else if(name==='Вентилятор (B)'){\n"
            "      const maxR=arr[arr.length-1];prof.dev.fan={maxW:maxR.netP};\n"
            "    }else if(name==='Вітер рослин'){\n"
            "      const maxR=arr[arr.length-1];prof.dev.plant={maxW:maxR.netP};\n"
            "    }else if(name==='Зволожувач (ON)'){\n"
            "      prof.dev.hum={w:arr[0].netP};\n"
            "    }\n"
            "  });\n"
            "  try{\n"
            "    await fetch('/audit/profile',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(prof)});\n"
            "    document.getElementById('summaryBox').innerHTML+='<br>💾 <b>Рекомендації збережено на ESP32</b> — тепер доступні на сторінках <a href=\"/tc421\">🔆 Фітолампа</a> та <a href=\"/settings\">⚙️ Налаштування</a>.';\n"
            "  }catch(e){}\n"
            "}\n"
            "function saveRunAndCalcOptics(byDev){\n"
            "  const H=Number(document.getElementById('lampH').value)||0;\n"
            "  const X=Number(document.getElementById('lampX').value)||0;\n"
            "  if(H<=0)return;\n"
            "  const maxLux={};\n"
            "  Object.entries(byDev).forEach(([dev,arr])=>{if(arr[0].cat==='light'){maxLux[dev]=Math.max(...arr.map(x=>x.netL));}});\n"
            "  if(Object.keys(maxLux).length===0)return;\n"
            "  let hist=[];try{hist=JSON.parse(localStorage.getItem('kk_audit_runs')||'[]');}catch(e){}\n"
            "  hist.push({ts:new Date().toLocaleTimeString('uk-UA'),H,X,maxLux});\n"
            "  if(hist.length>6)hist=hist.slice(-6);\n"
            "  try{localStorage.setItem('kk_audit_runs',JSON.stringify(hist));}catch(e){}\n"
            "  const box=document.getElementById('summaryBox');\n"
            "  let opt='<br><b>📐 Геометрія світла (H='+H+' см, зсув X='+X+' см):</b><br>';\n"
            "  const centerRuns=hist.filter(r=>r.X===0);\n"
            "  const sideRuns=hist.filter(r=>r.X>0);\n"
            "  if(centerRuns.length>0&&sideRuns.length>0){\n"
            "    const cRun=centerRuns[centerRuns.length-1],sRun=sideRuns[sideRuns.length-1];\n"
            "    if(Math.abs(cRun.H-sRun.H)<=3){\n"
            "      const ang=Math.atan(sRun.X/sRun.H);\n"
            "      Object.keys(cRun.maxLux).forEach(dev=>{\n"
            "        const e0=cRun.maxLux[dev],ex=sRun.maxLux[dev];\n"
            "        if(e0>10&&ex>1&&ex<e0){\n"
            "          const cosA=Math.cos(ang);\n"
            "          const m=Math.log(ex/e0)/Math.log(cosA);\n"
            "          if(m>0.2&&m<40){\n"
            "            const halfRad=Math.acos(Math.pow(0.5,1/m));\n"
            "            const beamDeg=Math.round(2*halfRad*180/Math.PI);\n"
            "            opt+=`• ${dev}: орієнтовний кут променя (50%) ≈ <b>${beamDeg}°</b> (центр ${e0.toFixed(0)} lx → зсув ${sRun.X} см: ${ex.toFixed(0)} lx)<br>`;\n"
            "          }\n"
            "        }\n"
            "      });\n"
            "    }else{opt+='• Для розрахунку кута зробіть замір у центрі (X=0) та збоку (X>0) на однаковій висоті H.<br>';}\n"
            "  }else if(centerRuns.length>=2){\n"
            "    const r1=centerRuns[centerRuns.length-2],r2=centerRuns[centerRuns.length-1];\n"
            "    if(r1.H!==r2.H){\n"
            "      Object.keys(r1.maxLux).forEach(dev=>{\n"
            "        const e1=r1.maxLux[dev],e2=r2.maxLux[dev];\n"
            "        if(e1>10&&e2>10){\n"
            "          const n=Math.log(e1/e2)/Math.log(r2.H/r1.H);\n"
            "          opt+=`• ${dev}: спад з висотою H=${r1.H}→${r2.H} см: <b>E ~ 1/H^${n.toFixed(2)}</b> (${e1.toFixed(0)}→${e2.toFixed(0)} lx)<br>`;\n"
            "        }\n"
            "      });\n"
            "      opt+='• Щоб визначити саме <b>кут розкриття променя</b>, зробіть ще 1 замір на тій же висоті H зі зсувом убік X (наприклад, 20–30 см).<br>';\n"
            "    }\n"
            "  }else{opt+='• Збережено базову точку. Для кута променя зробіть 2-й тест на тій же висоті H зі зсувом X>0, або на іншій висоті H для кривої спаду.<br>';}\n"
            "  box.innerHTML+=opt;\n"
            "}\n"
            "async function restoreAll(){\n"
            "  log('Відновлення штатного АВТО-режиму...');\n"
            "  try{await setQuiet(0,0);}catch(e){}\n"
            "  try{await cmd('plant auto');}catch(e){}\n"
            "  try{await cmd('hmin '+ORIG_HMIN);await cmd('hmax '+ORIG_HMAX);}catch(e){}\n"
            "  try{await cmd('auto');}catch(e){}\n"
            "  try{await fetch('/tc421/api/auto');}catch(e){}\n"
            "  if(wakeLock){try{await wakeLock.release();}catch(e){}wakeLock=null;}\n"
            "  log('✅ Повернуто в АВТО-режим (клімат + вітер + TC421 + вологість).');\n"
            "}\n"
            "async function stopAudit(){abortFlag=true;document.getElementById('stepText').textContent='Зупинка та повернення в АВТО...';}\n"
            "function downloadCsv(){\n"
            "  let csv='category,device,pct,base_w,raw_w,net_w,net_lux,lux_per_w\\n';\n"
            "  rows.forEach(r=>{csv+=`${r.cat},\"${r.dev}\",${r.pct},${r.baseP},${r.rawP},${r.netP},${r.netL},${r.eff}\\n`;});\n"
            "  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8'}));\n"
            "  a.download='klimat_audit_'+new Date().toISOString().slice(0,16).replace(/[:T]/g,'-')+'.csv';a.click();\n"
            "}\n";
    flushChunk();

    html += "async function startAudit(){\n"
            "  if(running)return;\n"
            "  const settleSec=Math.max(3,Math.min(15,Number(document.getElementById('settleSec').value)||6));\n"
            "  const stepMode=Number(document.getElementById('stepMode').value)||10;\n"
            "  const lightLvls=stepMode===5?[5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100]:(stepMode===20?[10,20,40,60,80,100]:[5,10,20,30,40,50,60,70,80,90,100]);\n"
            "  const muteClim=document.getElementById('muteClimate').checked;\n"
            "  const lightTasks=[];\n"
            "  if(document.getElementById('tCh1').checked)lightTasks.push([1,'CH1 Червоний']);\n"
            "  if(document.getElementById('tCh2').checked)lightTasks.push([2,'CH2 Білий']);\n"
            "  if(document.getElementById('tCh3').checked)lightTasks.push([3,'CH3 Жовтий']);\n"
            "  if(document.getElementById('tCh4').checked)lightTasks.push([4,'CH4 IR+UV']);\n"
            "  const actTasks=[];\n"
            "  if(document.getElementById('tPump').checked)actTasks.push(['Насос (A)','a',[30,40,50,60,70,80,90,100]]);\n"
            "  if(document.getElementById('tFan').checked)actTasks.push(['Вентилятор (B)','b',[20,40,60,80,100]]);\n"
            "  if(document.getElementById('tExt').checked)actTasks.push(['Витяжка (C)','c',[10,20,40,60,80,100]]);\n"
            "  if(document.getElementById('tPlant').checked)actTasks.push(['Вітер рослин','plant ',[20,40,60,80,100]]);\n"
            "  const doHum=document.getElementById('tHum').checked;\n"
            "  const totalSteps=lightTasks.length*lightLvls.length+actTasks.reduce((s,t)=>s+t[2].length,0)+(doHum?1:0);\n"
            "  if(totalSteps===0){alert('Оберіть хоча б один пункт для тесту');return;}\n"
            "  running=true;abortFlag=false;rows=[];\n"
            "  document.getElementById('resBody').innerHTML='';document.getElementById('auLog').textContent='';\n"
            "  document.getElementById('btnStart').disabled=true;document.getElementById('btnStop').disabled=false;\n"
            "  try{if('wakeLock' in navigator)wakeLock=await navigator.wakeLock.request('screen');}catch(e){}\n"
            "  let done=0;\n"
            "  const updProg=(txt)=>{document.getElementById('stepText').textContent=txt;document.getElementById('progBar').style.width=Math.round(done*100/totalSteps)+'%';};\n"
            "  try{\n"
            "    if(lightTasks.length>0){\n"
            "      const ph=await readPhyphox(1);\n"
            "      if(!ph.ok){log('⚠️ Phyphox недоступний: '+ph.err+' (люкси будуть 0, але Вати заміряються)');}\n"
            "      else{log('✅ Phyphox на зв’язку: '+ph.lux.toFixed(1)+' lx');}\n"
            "    }\n"
            "    if(muteClim){\n"
            "      updProg('Вимикаємо кліматику (Quiet + Насос 0% + Вітер 0%)...');\n"
            "      await setQuiet(1,300);await cmd('a0');await cmd('b0');await cmd('c0');await cmd('plant 0');\n"
            "    }\n"
            "    let lastQuietKeepAlive=Date.now();\n"
            "    for(const [idx,name] of lightTasks){\n"
            "      if(abortFlag)break;\n"
            "      if(muteClim){await setQuiet(1,300);lastQuietKeepAlive=Date.now();}\n"
            "      updProg(`Замір фону перед ${name}...`);\n"
            "      await setTC(0,0,0,0);\n"
            "      const base=await measure(settleSec,true);if(!base)break;\n"
            "      document.getElementById('vBase').textContent=base.p.toFixed(1)+' Вт';\n"
            "      log(`--- ${name} (фон ${base.p.toFixed(1)} Вт, ${base.lux.toFixed(1)} lx) ---`);\n"
            "      for(const lvl of lightLvls){\n"
            "        if(abortFlag)break;\n"
            "        if(muteClim&&Date.now()-lastQuietKeepAlive>90000){await setQuiet(1,300);lastQuietKeepAlive=Date.now();}\n"
            "        updProg(`${name}: ${lvl}% (стабілізація ${settleSec} с)...`);\n"
            "        const v=[0,0,0,0];v[idx-1]=lvl;await setTC(v[0],v[1],v[2],v[3]);\n"
            "        const m=await measure(settleSec,true);if(!m)break;\n"
            "        const netP=Math.max(0,Math.round((m.p-base.p)*100)/100);\n"
            "        const netL=Math.max(0,Math.round((m.lux-base.lux)*10)/10);\n"
            "        const eff=netP>=0.5?Math.round((netL/netP)*10)/10:0;\n"
            "        document.getElementById('vPow').textContent=`${m.p.toFixed(1)} / ${netP.toFixed(1)} Вт`;\n"
            "        document.getElementById('vLux').textContent=netL.toFixed(1)+' lx';\n"
            "        document.getElementById('vEff').textContent=eff.toFixed(1)+' lx/Вт';\n"
            "        addRow({cat:'light',dev:name,pct:lvl,baseP:base.p,rawP:m.p,netP,netL,eff});\n"
            "        log(`${name} ${lvl}%: ${netP.toFixed(1)} Вт | ${netL.toFixed(1)} lx | ${eff.toFixed(1)} lx/Вт`);\n"
            "        done++;updProg(`${name}: ${lvl}% готово`);\n"
            "      }\n"
            "    }\n"
            "    if((actTasks.length>0||doHum)&&!abortFlag){\n"
            "      await setTC(0,0,0,0);await setQuiet(0,0);await cmd('hmin 1');await cmd('hmax 2');\n"
            "      await cmd('a0');await cmd('b0');await cmd('c0');await cmd('plant 0');\n"
            "    }\n"
            "    for(const [name,pref,lvls] of actTasks){\n"
            "      if(abortFlag)break;\n"
            "      updProg(`Замір фону перед ${name}...`);\n"
            "      await cmd('a0');await cmd('b0');await cmd('c0');await cmd('plant 0');\n"
            "      const base=await measure(settleSec,false);if(!base)break;\n"
            "      document.getElementById('vBase').textContent=base.p.toFixed(1)+' Вт';\n"
            "      log(`--- ${name} (фон ${base.p.toFixed(1)} Вт) ---`);\n"
            "      for(const lvl of lvls){\n"
            "        if(abortFlag)break;\n"
            "        updProg(`${name}: ${lvl}%...`);\n"
            "        await cmd(pref+lvl);\n"
            "        const m=await measure(settleSec,false);if(!m)break;\n"
            "        const netP=Math.max(0,Math.round((m.p-base.p)*100)/100);\n"
            "        document.getElementById('vPow').textContent=`${m.p.toFixed(1)} / ${netP.toFixed(1)} Вт`;\n"
            "        addRow({cat:'actuator',dev:name,pct:lvl,baseP:base.p,rawP:m.p,netP,netL:0,eff:0});\n"
            "        log(`${name} ${lvl}%: чисті ${netP.toFixed(1)} Вт (всього ${m.p.toFixed(1)} Вт)`);\n"
            "        done++;updProg(`${name}: ${lvl}% готово`);\n"
            "      }\n"
            "      await cmd(pref+'0');\n"
            "    }\n"
            "    if(doHum&&!abortFlag){\n"
            "      updProg('Тест зволожувача (100% ON)...');\n"
            "      const base=await measure(settleSec,false);\n"
            "      if(base){\n"
            "        await cmd('hmin 95');await cmd('hmax 99');\n"
            "        const m=await measure(Math.max(settleSec,6),false);\n"
            "        if(m){\n"
            "          const netP=Math.max(0,Math.round((m.p-base.p)*100)/100);\n"
            "          addRow({cat:'actuator',dev:'Зволожувач (ON)',pct:100,baseP:base.p,rawP:m.p,netP,netL:0,eff:0});\n"
            "          log(`Зволожувач 100%: чисті ${netP.toFixed(1)} Вт (всього ${m.p.toFixed(1)} Вт)`);\n"
            "          done++;\n"
            "        }\n"
            "      }\n"
            "    }\n"
            "    if(!abortFlag){buildSummary();updProg('✅ Аудит успішно завершено!');saveSessionLocal();}\n"
            "    else{if(rows.length){buildSummary();saveSessionLocal();}updProg('⛔ Аудит зупинено користувачем');}\n"
            "  }catch(e){log('❌ Помилка: '+e);updProg('❌ Помилка під час тесту');}\n"
            "  finally{\n"
            "    await restoreAll();\n"
            "    running=false;document.getElementById('btnStart').disabled=false;document.getElementById('btnStop').disabled=true;\n"
            "  }\n"
            "}\n"
            "async function saveSessionLocal(){\n"
            "  const payload={ts:new Date().toLocaleString('uk-UA'),rows:rows,log:document.getElementById('auLog').textContent,summary:document.getElementById('summaryBox').innerHTML};\n"
            "  try{localStorage.setItem('kk_audit_last',JSON.stringify(payload));}catch(e){}\n"
            "  try{await fetch('/audit/data',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ts:payload.ts,rows:rows})});}catch(e){}\n"
            "}\n"
            "function clearSavedAudit(){\n"
            "  if(running)return;\n"
            "  rows=[];document.getElementById('resBody').innerHTML='';\n"
            "  document.getElementById('auLog').textContent='Лог очищено.';\n"
            "  document.getElementById('summaryBox').innerHTML='';\n"
            "  document.getElementById('resCard').style.display='none';\n"
            "  document.getElementById('btnCsv').disabled=true;\n"
            "  try{localStorage.removeItem('kk_audit_last');}catch(e){}\n"
            "}\n"
            "(async function restoreSessionOnLoad(){\n"
            "  let restored=false;\n"
            "  try{\n"
            "    const d=await(await fetch('/audit/data')).json();\n"
            "    if(d&&Array.isArray(d.rows)&&d.rows.length>0){\n"
            "      d.rows.forEach(r=>addRow(r));\n"
            "      buildSummary();\n"
            "      document.getElementById('stepText').textContent='📂 Завантажено збережений графік з ESP32 ('+(d.ts||'')+')';\n"
            "      restored=true;\n"
            "    }\n"
            "  }catch(e){}\n"
            "  if(!restored){\n"
            "    try{\n"
            "      const s=JSON.parse(localStorage.getItem('kk_audit_last')||'null');\n"
            "      if(s&&Array.isArray(s.rows)&&s.rows.length>0){\n"
            "        s.rows.forEach(r=>addRow(r));\n"
            "        if(s.log)document.getElementById('auLog').textContent=s.log;\n"
            "        if(s.summary)document.getElementById('summaryBox').innerHTML=s.summary;\n"
            "        document.getElementById('stepText').textContent='📂 Відновлено останній тест ('+(s.ts||'')+')';\n"
            "        restored=true;\n"
            "      }\n"
            "    }catch(e){}\n"
            "  }\n"
            "  if(!restored){\n"
            "    try{\n"
            "      const p=await(await fetch('/audit/profile')).json();\n"
            "      if(p&&p.saved){\n"
            "        let s=`<b>💾 Останній збережений профіль на ESP32 (${p.ts||''}):</b><br>`;\n"
            "        if(p.tc){['ch1','ch2','ch3','ch4'].forEach(k=>{if(p.tc[k])s+=`• ${k.toUpperCase()}: оптимум <b>${p.tc[k].pct}%</b> (${p.tc[k].w} Вт, макс. ${p.tc[k].maxW} Вт)<br>`;});}\n"
            "        if(p.dev){if(p.dev.pump)s+=`• Насос: оптимум ${p.dev.pump.optPct}% (${p.dev.pump.optW} Вт)<br>`;if(p.dev.ext)s+=`• Витяжка: реком. ${p.dev.ext.optPct}% (${p.dev.ext.optW} Вт)<br>`;if(p.dev.fan)s+=`• Вентилятор: ${p.dev.fan.maxW} Вт<br>`;if(p.dev.plant)s+=`• Вітер рослин: ${p.dev.plant.maxW} Вт<br>`;if(p.dev.hum)s+=`• Зволожувач: ${p.dev.hum.w} Вт<br>`;}\n"
            "        document.getElementById('summaryBox').innerHTML=s;\n"
            "        document.getElementById('resCard').style.display='block';\n"
            "      }\n"
            "    }catch(e){}\n"
            "  }\n"
            "})();\n"
            "</script>";

    html += "</div></body></html>";
    flushChunk();
    server.sendContent("");
}
