// ============================================================================
// WEB_COMMON.H - Єдина сучасна дизайн-система та шаблони для всіх сторінок
// Темна тема (за замовчуванням) + Світла тема через data-theme, без CDN-шрифтів
// ============================================================================

#ifndef WEB_COMMON_H
#define WEB_COMMON_H

#include <Arduino.h>

extern String getUkraineMarquee();

// ============================================================================
// ЄДИНИЙ СУЧАСНИЙ CSS (Mobile-first, CSS-змінні Dark / Light)
// ============================================================================

inline String getCommonCSS() {
    String css = "";
    css.reserve(4600);
    css += ":root{--bg:#0b0f17;--s:#131926;--s2:#1b2335;--b:#243047;--t:#f1f5f9;--m:#94a3b8;--a:#38bdf8;--ah:#0ea5e9;--ok:#10b981;--w:#f59e0b;--e:#ef4444;--r:12px;--sh:0 4px 20px rgba(0,0,0,.35)}";
    css += "[data-theme='light']{--bg:#f1f5f9;--s:#ffffff;--s2:#f8fafc;--b:#e2e8f0;--t:#0f172a;--m:#64748b;--a:#0284c7;--ah:#0369a1;--ok:#059669;--w:#d97706;--e:#dc2626;--sh:0 2px 12px rgba(15,23,42,.08)}";
    css += "*,*::before,*::after{box-sizing:border-box}";
    css += "body{margin:0;padding:14px;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--t);line-height:1.5;-webkit-font-smoothing:antialiased}";
    css += "h1,h2,h3{margin:0 0 12px;color:var(--t);font-weight:600;letter-spacing:-.01em}";
    css += "h1{font-size:1.45rem}h2{font-size:1.15rem}h3{font-size:1rem}";
    css += "a{color:var(--a)}";
    css += ".container{max-width:1180px;margin:0 auto;background:var(--s);border:1px solid var(--b);padding:22px;border-radius:14px;box-shadow:var(--sh)}";
    css += ".header{text-align:center;margin-bottom:22px}";
    css += ".page-title{margin:0 0 18px}";

    // Єдина липка навігація (посилання прокручуються, перемикач теми завжди закріплений праворуч)
    css += ".topnav{position:sticky;top:0;z-index:100;display:flex;align-items:center;gap:6px;padding:7px 8px;margin:-6px -6px 18px;background:var(--s2);border:1px solid var(--b);border-radius:12px;box-shadow:var(--sh)}";
    css += ".topnav-links{display:flex;align-items:center;gap:4px;flex:1 1 auto;min-width:0;overflow-x:auto;white-space:nowrap;scrollbar-width:thin;-webkit-overflow-scrolling:touch}";
    css += ".topnav-links::-webkit-scrollbar{height:4px}.topnav-links::-webkit-scrollbar-thumb{background:var(--b);border-radius:4px}";
    css += ".topnav a,.theme-btn{flex:0 0 auto;display:inline-flex;align-items:center;gap:4px;padding:6px 9px;color:var(--m);text-decoration:none;border-radius:8px;font-size:13px;font-weight:500;border:1px solid transparent;background:transparent;cursor:pointer;transition:.15s;white-space:nowrap}";
    css += ".topnav a:hover,.theme-btn:hover{background:var(--s);color:var(--t);border-color:var(--b)}";
    css += ".topnav a.active{background:var(--ah);color:#fff;border-color:var(--a)}";
    css += ".theme-btn{background:var(--s);color:var(--t);border-color:var(--b);font-weight:600}";

    // Кнопки
    css += ".btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;background:var(--ah);color:#fff;padding:10px 16px;border:1px solid transparent;border-radius:9px;cursor:pointer;margin:4px;transition:.15s;font-weight:600;font-size:14px;text-decoration:none}";
    css += ".btn:hover{filter:brightness(1.1)}.btn:active{transform:scale(.98)}";
    css += ".btn-success{background:var(--ok);color:#fff}";
    css += ".btn-danger{background:var(--e);color:#fff}";
    css += ".btn-secondary{background:var(--s2);color:var(--t);border-color:var(--b)}";

    // Сітка навігації (головна)
    css += ".nav{display:grid;grid-template-columns:repeat(auto-fit,minmax(175px,1fr));gap:10px;margin:20px 0}";
    css += ".nav-btn{background:var(--s2);color:var(--t)!important;border:1px solid var(--b);border-left:3px solid var(--a);padding:13px 14px;text-align:center;text-decoration:none;border-radius:10px;display:block;transition:.15s;font-weight:600;font-size:14px}";
    css += ".nav-btn:hover{background:var(--ah)!important;color:#fff!important;border-color:var(--a);transform:translateY(-2px)}";

    // Картки та KPI
    css += ".card,.control-section,.section{background:var(--s2);padding:18px;border-radius:var(--r);border:1px solid var(--b);border-left:3px solid var(--a);margin-bottom:16px}";
    css += ".status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:14px;margin:18px 0}";
    css += ".status-value{font-size:1.25em;font-weight:700;font-variant-numeric:tabular-nums}";
    css += ".temp-status{color:#fb7185}.hum-status{color:#38bdf8}.sys-status{color:var(--t)}";

    // Індикатори потужності (A/B/C)
    css += ".power-indicators{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin:18px 0;background:var(--s2);border:1px solid var(--b);padding:14px;border-radius:var(--r)}";
    css += ".power-item{text-align:center;padding:10px;background:var(--s);border:1px solid var(--b);border-radius:10px}";
    css += ".power-label{font-size:.85em;color:var(--m);margin-bottom:4px}";
    css += ".power-value{font-size:1.7em;font-weight:700;color:var(--a);font-variant-numeric:tabular-nums}";

    // Форми, слайдери, вкладки, режими
    css += "input,select,textarea{padding:10px 12px;border:1px solid var(--b);border-radius:8px;font-size:14px;background:var(--s);color:var(--t);outline:none}";
    css += "input:focus,select:focus,textarea:focus{border-color:var(--a);box-shadow:0 0 0 3px rgba(56,189,248,.18)}";
    css += "input[type=checkbox],input[type=radio]{width:auto;accent-color:var(--ah)}";
    css += ".slider{-webkit-appearance:none;appearance:none;width:100%;height:8px;border-radius:99px;background:var(--s);border:1px solid var(--b);outline:none;margin:14px 0}";
    css += ".slider::-webkit-slider-thumb{-webkit-appearance:none;width:26px;height:26px;border-radius:50%;background:var(--ah);border:3px solid var(--s2);cursor:pointer}";
    css += ".slider::-moz-range-thumb{width:26px;height:26px;border-radius:50%;background:var(--ah);border:3px solid var(--s2);cursor:pointer}";
    css += ".slider-value{display:inline-block;min-width:52px;font-weight:700;font-size:1.25rem;color:var(--a);font-variant-numeric:tabular-nums}";
    css += ".mode-buttons{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}";
    css += ".mode-btn{padding:13px 16px;border:1px solid var(--b);background:var(--s);color:var(--t);border-radius:10px;cursor:pointer;font-size:15px;font-weight:600;transition:.15s}";
    css += ".mode-btn:hover{border-color:var(--a)}.mode-btn.active{background:var(--ah);color:#fff;border-color:var(--a)}";
    css += ".tab-btn{padding:8px 14px;border:1px solid var(--b);background:var(--s2);color:var(--m);border-radius:8px;cursor:pointer;font-size:14px;font-weight:500;transition:.15s;text-decoration:none;display:inline-flex;align-items:center;gap:5px}";
    css += ".tab-btn:hover{color:var(--t);border-color:var(--a)}.tab-btn.active{background:var(--ah);color:#fff;border-color:var(--a);font-weight:600}";

    // Консоль, банери, таблиці, графіки
    css += ".command-section{background:var(--s2);border:1px solid var(--b);padding:18px;border-radius:var(--r);margin:18px 0}";
    css += ".quick-buttons{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}";
    css += ".quick-btn{background:var(--s);color:var(--t);border:1px solid var(--b);padding:7px 11px;border-radius:7px;cursor:pointer;font-size:13px;transition:.15s}";
    css += ".quick-btn:hover{border-color:var(--a);color:var(--a)}";
    css += "#commandOutput{background:#070a10;color:#cbd5e1;padding:12px;border-radius:8px;font-family:ui-monospace,Consolas,monospace;font-size:13px;min-height:60px;white-space:pre-wrap;overflow-y:auto;max-height:210px;border:1px solid var(--b)}";
    css += ".mode-badge{display:inline-block;padding:7px 16px;background:var(--ok);color:#fff;border-radius:99px;font-weight:600;font-size:14px;margin-top:8px}";
    css += ".emergency-alert{background:rgba(239,68,68,.16);border:1px solid var(--e);padding:14px 16px;border-radius:var(--r);margin:14px 0;color:var(--t)}";
    css += ".emergency-alert h3{color:var(--e);margin:0 0 6px}";
    css += ".info-banner{background:var(--s2);border:1px solid var(--b);border-left:3px solid var(--ok);padding:10px 14px;border-radius:10px;margin:10px 0;font-size:.88em;color:var(--m)}";
    css += ".android-tip{background:var(--s2);border:1px solid var(--w);border-left:4px solid var(--w);padding:12px 14px;border-radius:10px;margin:10px 0;display:flex;align-items:center;gap:14px;flex-wrap:wrap;color:var(--t)}";
    css += ".update-info{text-align:center;color:var(--m);margin-top:18px;padding-top:14px;border-top:1px solid var(--b);font-size:.88em}";
    css += ".chart-container{position:relative;height:300px;margin:20px 0;background:var(--s2);border:1px solid var(--b);border-radius:var(--r);padding:12px}";
    css += "table{width:100%;border-collapse:collapse}th,td{padding:9px 10px;border-bottom:1px solid var(--b);text-align:left;color:var(--t)}th{background:var(--s);color:var(--m);font-size:.85em}";
    css += ".info-item,.network-item{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;background:var(--s);border:1px solid var(--b);border-radius:8px;margin-bottom:8px}";
    css += ".code{background:#070a10;color:#86efac;padding:10px 12px;border-radius:8px;font-family:ui-monospace,Consolas,monospace;border:1px solid var(--b);margin:8px 0;overflow-x:auto}";
    css += "code{background:var(--s);color:var(--a);padding:2px 6px;border-radius:4px;border:1px solid var(--b);font-family:ui-monospace,Consolas,monospace}";
    css += ".tooltip{position:relative;display:inline-block;cursor:help}";
    css += ".tooltip .tooltiptext{visibility:hidden;width:240px;background:var(--s);color:var(--t);border:1px solid var(--b);text-align:left;border-radius:8px;padding:10px;position:absolute;z-index:20;bottom:125%;left:50%;margin-left:-120px;opacity:0;transition:opacity .2s;font-size:.82em;box-shadow:var(--sh)}";
    css += ".tooltip:hover .tooltiptext{visibility:visible;opacity:1}";
    css += ".sensor-tile{padding:12px;background:var(--s);border:1px solid var(--b);border-top:3px solid var(--a);border-radius:10px;text-align:center}";
    css += ".sensor-tile.err{border-top-color:var(--e)}.sensor-tile .lbl{font-size:12px;color:var(--m)}.sensor-tile .val{font-size:22px;font-weight:700;font-variant-numeric:tabular-nums;margin-top:4px}";

    return css;
}

// ============================================================================
// МОБІЛЬНІ СТИЛІ
// ============================================================================

inline String getMobileCSS() {
    String css = "";
    css += "@media(max-width:768px){";
    css += "body{padding:8px}.container{padding:14px}h1{font-size:1.25rem}";
    css += ".nav{grid-template-columns:repeat(2,1fr);gap:8px}.nav-btn{padding:11px 8px;font-size:13px}";
    css += ".status-grid{grid-template-columns:1fr 1fr;gap:10px}.card,.control-section,.section{padding:14px}";
    css += ".power-value{font-size:1.4em}.quick-btn{flex:1 1 calc(50% - 6px);text-align:center}";
    css += "input,select,textarea{font-size:16px}.chart-container{height:240px}";
    css += "}";
    css += "@media(max-width:480px){";
    css += ".status-grid{grid-template-columns:1fr}.quick-btn{flex:1 1 calc(33.33% - 6px);font-size:12px}";
    css += ".chart-container{height:200px}";
    css += "}";
    return css;
}

// ============================================================================
// НАВІГАЦІЙНІ ЕЛЕМЕНТИ
// ============================================================================

inline String getNavHeader(const String& title, const String& backUrl = "/") {
    (void)backUrl;
    String html = "";
    html += "<nav class='topnav' id='topnav'>"
            "<div class='topnav-links'>"
            "<a href='/'>🏠 Головна</a>"
            "<a href='/control'>🎛️ Керування</a>"
            "<a href='/settings'>⚙️ Налаштування</a>"
            "<a href='/tc421'>🔆 Фітолампа</a>"
            "<a href='/light'>💡 DMX</a>"
            "<a href='/energy'>⚡ Енергія</a>"
            "<a href='/audit'>🔬 Аудит</a>"
            "<a href='/history'>📈 Графіки</a>"
            "<a href='/wifi'>📶 Wi-Fi</a>"
            "<a href='/ota'>🔄 OTA</a>"
            "<a href='/tablet'>🎙️ Степан</a>"
            "<a href='/help'>📖 Довідка</a>"
            "<a href='/debug'>🔧 Відладка</a>"
            "</div>"
            "<button type='button' class='theme-btn' onclick='kkToggleTheme()' title='Змінити тему'>☀️ Світла</button>"
            "</nav>";
    html += "<script>(function(){var p=location.pathname,l=document.querySelectorAll('#topnav a');"
            "for(var i=0;i<l.length;i++){var h=l[i].getAttribute('href');"
            "if((h==='/'&&p==='/')||(h!=='/'&&p.indexOf(h)===0)){l[i].classList.add('active');l[i].scrollIntoView({inline:'center',block:'nearest'});}}"
            "if(window.kkSyncTheme)kkSyncTheme();})();</script>";
    if (title.length() > 0) {
        html += "<h1 class='page-title'>" + title + "</h1>";
    }
    return html;
}

inline String getNavFooter(const String& backUrl = "/", const String& backText = "На головну") {
    (void)backUrl; (void)backText;
    return "";
}

inline String getMainNavigation() {
    String html = "";
    html += "<div class='nav'>";
    html += "<a href='/control' class='nav-btn'>🎛️ Керування</a>";
    html += "<a href='/settings' class='nav-btn'>⚙️ Налаштування</a>";
    html += "<a href='/energy' class='nav-btn' style='border-left-color:#f59e0b'>⚡ Енергоконтролер</a>";
    html += "<a href='/audit' class='nav-btn' style='border-left-color:#14b8a6'>🔬 Енергоаудит</a>";
    html += "<a href='/tc421' class='nav-btn' style='border-left-color:#10b981'>🔆 Фітолампа TC421</a>";
    html += "<a href='/light' class='nav-btn' style='border-left-color:#fbbf24'>💡 DMX Освітлення</a>";
    html += "<a href='/history' class='nav-btn' style='border-left-color:#ec4899'>📈 Графіки</a>";
    html += "<a href='/wifi' class='nav-btn' style='border-left-color:#38bdf8'>📶 Wi-Fi</a>";
    html += "<a href='/ota' class='nav-btn' style='border-left-color:#06b6d4'>🔄 OTA та Backup</a>";
    html += "<a href='/tablet' class='nav-btn' style='border-left-color:#34d399'>🎙️ Екран «Степан»</a>";
    html += "<a href='/help' class='nav-btn' style='border-left-color:#a855f7'>📖 Довідка</a>";
    html += "<a href='/debug' class='nav-btn' style='border-left-color:#64748b'>🔧 Відладка</a>";
    html += "</div>";
    return html;
}

// ============================================================================
// HTML ШАБЛОНИ
// ============================================================================

inline String getHtmlHead(const String& title, bool includeChartJS = false) {
    String html = "<!DOCTYPE html><html lang='uk' data-theme='dark'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=5.0, user-scalable=yes'>";
    html += "<title>" + title + "</title>";
    html += "<script>"
            "(function(){var t=localStorage.getItem('kk_theme')||'dark';"
            "if(t!=='dark'&&t!=='light')t='dark';"
            "document.documentElement.setAttribute('data-theme',t);"
            "window.kkSyncTheme=function(){var c=document.documentElement.getAttribute('data-theme')||'dark';"
            "document.querySelectorAll('.theme-btn').forEach(function(b){b.textContent=c==='dark'?'☀️ Світла':'🌙 Темна';});};"
            "window.kkToggleTheme=function(){var c=document.documentElement.getAttribute('data-theme')==='dark'?'light':'dark';"
            "document.documentElement.setAttribute('data-theme',c);localStorage.setItem('kk_theme',c);kkSyncTheme();};"
            "document.addEventListener('DOMContentLoaded',kkSyncTheme);})();"
            "</script>";
    html += "<style>";
    html += getCommonCSS();
    html += getMobileCSS();
    html += "</style>";

    if (includeChartJS) {
        html += "<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0'></script>";
        html += "<script src='https://cdn.jsdelivr.net/npm/hammerjs@2.0.8'></script>";
        html += "<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.1'></script>";
    }

    html += "</head><body>";
    return html;
}

inline String getHtmlFooter() {
    return "</body></html>";
}

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================================

inline String htmlEscape(const String& str) {
    String escaped = "";
    escaped.reserve(str.length() * 1.2);
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        switch (c) {
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '&':  escaped += "&amp;";  break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&#x27;"; break;
            default:   escaped += c;        break;
        }
    }
    return escaped;
}

#endif // WEB_COMMON_H
