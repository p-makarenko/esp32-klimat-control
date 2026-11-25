// ============================================================================
// WEB_PAGES_SETTINGS.CPP - Сторінки налаштувань
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Налаштування системи (handleSettingsPage, handleSaveSettings)
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include "web_common.h"
#include "global_declarations.h"
#include "co2_sensor.h"

extern WebServer server;
extern SystemConfig config;
extern VentilationState ventState;

// Forward declarations
extern bool checkAuth();
extern bool checkCSRF();
extern void saveConfiguration();
extern String getUkraineMarquee();
extern String getTimeString();
extern String getDateString();
extern String getFormattedTime();
extern bool isTimeSynced();

// ============================================================================
// СТОРІНКА НАЛАШТУВАНЬ (з вкладками)
// ============================================================================

void handleSettingsPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) return;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    String html;
    html.reserve(4096);
    auto flushChunk = [&]() {
        if (html.length() > 0) {
            server.sendContent(html);
            html = "";
            yield();
        }
    };

    html = getHtmlHead("Налаштування системи");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ СИСТЕМИ");

    // Стилі для категорій та вкладок
    html += "<style>";
    html += ".cat-bar { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 12px; padding: 5px; background: var(--s2); border: 1px solid var(--b); border-radius: 10px; }";
    html += ".cat-btn { padding: 11px 14px; border: none; border-radius: 8px; background: transparent; color: var(--m); font-size: 15px; font-weight: 600; cursor: pointer; transition: .15s; }";
    html += ".cat-btn:hover { color: var(--t); background: var(--s); }";
    html += ".cat-btn.active { background: var(--ah); color: #fff; }";
    html += ".tabs { display: none; gap: 6px; margin-bottom: 18px; flex-wrap: wrap; padding-bottom: 10px; border-bottom: 1px solid var(--b); }";
    html += ".tabs.active { display: flex; }";
    html += ".tab { padding: 9px 14px; background: var(--s2); border: 1px solid var(--b); border-radius: 8px; cursor: pointer; font-size: 14px; font-weight: 500; transition: .15s; color: var(--m); }";
    html += ".tab:hover { background: var(--s); color: var(--t); border-color: var(--a); }";
    html += ".tab.active { background: var(--ok); color: #fff; border-color: var(--ok); font-weight: 600; }";
    html += ".tab-content { display: none; animation: fadeIn 0.2s; }";
    html += ".tab-content.active { display: block; }";
    html += "@keyframes fadeIn { from { opacity: 0; transform: translateY(-6px); } to { opacity: 1; transform: translateY(0); } }";
    html += ".section { background: var(--s2); padding: 18px; border-radius: 12px; margin-bottom: 18px; border: 1px solid var(--b); border-left: 3px solid var(--a); }";
    html += ".section h3 { margin-top: 0; color: var(--a); }";
    html += ".form-group { margin-bottom: 16px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: 600; color: var(--t); font-size: 14px; }";
    html += "input[type='number'], input[type='text'], input[type='password'], select { width: 100%; padding: 10px; border: 1px solid var(--b); border-radius: 8px; box-sizing: border-box; font-size: 16px; background: var(--s); color: var(--t); }";
    html += ".tip { display:inline-flex; align-items:center; justify-content:center; width:18px; height:18px; margin-left:6px; border-radius:50%; background:rgba(56,189,248,.18); color:var(--a); font-size:12px; font-weight:700; cursor:help; position:relative; vertical-align:middle; }";
    html += ".tip::after { content: attr(data-tip); position:absolute; left:50%; bottom:calc(100% + 8px); transform:translateX(-50%); min-width:220px; max-width:300px; padding:8px 10px; border-radius:8px; background:#0f172a; color:#f8fafc; border:1px solid var(--a); font-size:12.5px; font-weight:400; line-height:1.35; white-space:normal; box-shadow:0 6px 20px rgba(0,0,0,.35); opacity:0; pointer-events:none; transition:opacity .15s; z-index:99; }";
    html += ".tip:hover::after, .tip:focus::after { opacity:1; }";
    html += "@media (max-width: 768px) { .cat-btn { font-size: 13.5px; padding: 10px 8px; } .tab { padding: 8px 11px; font-size: 13px; } .section { padding: 14px; } }";
    html += "@media (max-width: 480px) { .tab { flex: 1 1 calc(33.33% - 6px); text-align: center; font-size: 12px; padding: 8px 6px; } }";
    html += "</style>";

    // КАТЕГОРІЇ: Робочі (клімат/пристрої) vs Системні
    html += "<div class='cat-bar'>";
    html += "<button type='button' class='cat-btn active' data-cat='work' onclick=\"switchCat('work')\">🌿 Клімат і пристрої</button>";
    html += "<button type='button' class='cat-btn' data-cat='sys' onclick=\"switchCat('sys')\">🛠️ Системні налаштування</button>";
    html += "</div>";

    // РОБОЧІ ВКЛАДКИ
    html += "<div class='tabs active' id='cat-work'>";
    html += "<button type='button' class='tab active' data-tab='0' data-cat='work' onclick='switchTab(0)'>🌡️ Клімат</button>";
    html += "<button type='button' class='tab' data-tab='1' data-cat='work' onclick='switchTab(1)'>🔧 Пристрої</button>";
    html += "<button type='button' class='tab' data-tab='2' data-cat='work' onclick='switchTab(2)'>🌀 Витяжка</button>";
    html += "<button type='button' class='tab' data-tab='11' data-cat='work' onclick='switchTab(11)'>💨 Вітер</button>";
    html += "<button type='button' class='tab' data-tab='3' data-cat='work' onclick='switchTab(3)'>🔄 Сезон</button>";
    html += "<button type='button' class='tab' data-tab='4' data-cat='work' onclick='switchTab(4)'>🚨 Аварія</button>";
    html += "<button type='button' class='tab' data-tab='12' data-cat='work' onclick='switchTab(12)'>🔋 Економія</button>";
    html += "</div>";

    // СИСТЕМНІ ВКЛАДКИ
    html += "<div class='tabs' id='cat-sys'>";
    html += "<button type='button' class='tab' data-tab='5' data-cat='sys' onclick='switchTab(5)'>💨 Датчики</button>";
    html += "<button type='button' class='tab' data-tab='7' data-cat='sys' onclick='switchTab(7)'>🎯 Серво</button>";
    html += "<button type='button' class='tab' data-tab='8' data-cat='sys' onclick='switchTab(8)'>📶 Wi-Fi</button>";
    html += "<button type='button' class='tab' data-tab='9' data-cat='sys' onclick='switchTab(9)'>📊 Sync</button>";
    html += "<button type='button' class='tab' data-tab='10' data-cat='sys' onclick='switchTab(10)'>🕒 Час</button>";
    html += "<button type='button' class='tab' data-tab='6' data-cat='sys' onclick='switchTab(6)'>⚙️ Система</button>";
    html += "</div>";
    html += "<form method='POST' action='/settings'>";

    // TAB 0: КЛІМАТ
    html += "<div class='tab-content active' id='tab0'>";
    html += "<div class='section'>";
    html += "<h3>🌡️ ТЕМПЕРАТУРА</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна температура (°C):</label>";
    html += "<input type='number' step='0.1' name='tempMin' value='" + String(config.tempMin, 1) + "' min='10' max='40'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальна температура (°C):</label>";
    html += "<input type='number' step='0.1' name='tempMax' value='" + String(config.tempMax, 1) + "' min='10' max='40'>";
    html += "</div>";
    html += "</div>";

    html += "<div class='section'>";
    html += "<h3>💧 ВОЛОГІСТЬ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна вологість (%):</label>";
    html += "<input type='number' step='0.1' name='humMin' value='" + String(config.humidityConfig.minHumidity, 1) + "' min='30' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальна вологість (%):</label>";
    html += "<input type='number' step='0.1' name='humMax' value='" + String(config.humidityConfig.maxHumidity, 1) + "' min='30' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Гістерезис вологості (%):</label>";
    html += "<input type='number' name='humHyst' value='" + String(config.humidityConfig.hysteresis) + "' min='1' max='10'>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab0

    // TAB 1: ПРИСТРОЇ
    html += "<div class='tab-content' id='tab1'>";
    html += "<p style='color:var(--m);font-size:13px;margin:0 0 12px 0;'>💡 Наведіть курсор (або натисніть) на значок <span class='tip' tabindex='0' data-tip='Тут зʼявляються детальні пояснення до кожного параметра пристроїв'>?</span> біля будь-якого пункту, щоб побачити детальне пояснення.</p>";

    // ========================================================================
    // СЕКЦІЯ 1: УСІ НАЛАШТУВАННЯ НАСОСА В ОДНОМУ МІСЦІ
    // ========================================================================
    html += "<div id='auditSetBox' class='info-banner' style='display:none;border-left-color:#14b8a6;margin-bottom:14px'></div>";
    html += "<script>(async function(){try{const p=await(await fetch('/audit/profile')).json();if(!p||!p.saved||!p.dev||Object.keys(p.dev).length===0)return;"
            "let s=`<b>🔬 Рекомендації енергоаудиту (${p.ts||''}):</b><br>`;"
            "if(p.dev.pump)s+=`• 💧 Насос: економний макс. <b>${p.dev.pump.optPct}%</b> (~${p.dev.pump.optW} Вт; на 100% = ${p.dev.pump.maxW} Вт) <button type='button' class='quick-btn' style='padding:3px 8px' onclick=\"var el=document.querySelector('input[name=pumpMax]');if(el){el.value=${p.dev.pump.optPct};el.focus();}\">✨ Підставити в Макс.</button><br>`;"
            "if(p.dev.ext)s+=`• 💨 Витяжка: реком. <b>${p.dev.ext.optPct}%</b> (~${p.dev.ext.optW} Вт${p.dev.ext.flat?', після 40% Вати майже не ростуть — вигідніше швидше й коротше':''}) <button type='button' class='quick-btn' style='padding:3px 8px' onclick=\"var el=document.querySelector('input[name=extPower]');if(el){el.value=${p.dev.ext.optPct};el.focus();}\">✨ Підставити в Потужн.</button><br>`;"
            "if(p.dev.fan)s+=`• 🔥 Вентилятор (100%): ~${p.dev.fan.maxW} Вт `;"
            "if(p.dev.plant)s+=`• 🌿 Вітер рослин (100%): ~${p.dev.plant.maxW} Вт `;"
            "if(p.dev.hum)s+=`• 🌫️ Зволожувач: ~${p.dev.hum.w} Вт`;"
            "const b=document.getElementById('auditSetBox');b.innerHTML=s;b.style.display='block';"
            "}catch(e){}})();</script>";
    html += "<div class='section' style='border-left-color: #38bdf8;'>";
    html += "<h3>💧 НАСОС (потужність, пуск та розумне керування)</h3>";

    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='pumpDisabled' " + String(config.pumpDisabled ? "checked" : "") + "> ⛔ Повністю вимкнути насос (Блокування) <span class='tip' tabindex='0' data-tip='Повне апаратне блокування: насос завжди триматиметься на 0% у всіх режимах (авто, форсаж, аварія, ЕКО та ручний). Зручно для обслуговування або теплої пори року.'>?</span></label>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум насоса (%) <span class='tip' tabindex='0' data-tip='Нижня межа ШІМ в автоматичному режимі. Якщо автоматика подає >0%, але менше цього порогу, потужність підтягнеться до цього мінімуму, щоб насос не зупинявся і не гудів.'>?</span>:</label>";
    html += "<input type='number' name='pumpMin' value='" + String(config.pumpMinPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум насоса (%) <span class='tip' tabindex='0' data-tip='Верхня стеля потужності насоса в автоматичному режимі. Обмежує максимальний потік, шум та споживання електроенергії.'>?</span>:</label>";
    html += "<input type='number' name='pumpMax' value='" + String(config.pumpMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>🚀 Пускова потужність (%) <span class='tip' tabindex='0' data-tip='Стартовий поштовх (Kickstart) при запуску насоса з 0%. Наприклад, якщо робоча потужність 33%, спочатку подається 45%, щоб крильчатка гарантовано зрушила з місця, а потім плавно знижується до робочої.'>?</span>:</label>";
    html += "<input type='number' name='pumpStartPct' value='" + String(config.pumpStartPercent) + "' min='20' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>⏱️ Час пуску (сек) <span class='tip' tabindex='0' data-tip='Скільки секунд тримати пускову потужність при старті з 0% перед переходом на робочі оберти. Зазвичай достатньо 2–3 сек. 0 = вимкнути пусковий імпульс.'>?</span>:</label>";
    html += "<input type='number' name='pumpStartSec' value='" + String(config.pumpStartDurationSec) + "' min='0' max='15'>";
    html += "</div>";
    html += "</div>";

    html += "<hr style='border:0;border-top:1px solid var(--b);margin:14px 0;'>";
    html += "<p style='color: var(--m); font-size: 13.5px; margin: 0 0 12px 0;'><strong>🧠 Розумний режим за швидкістю нагріву:</strong> насос не вмикається зайвий раз, якщо кімната і так набирає температуру з потрібною швидкістю. Підключається лише коли тепла в радіаторі вже не вистачає.</p>";

    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='spEn' " + String(config.smartPump.enabled ? "checked" : "") + "> Увімкнути розумний режим насоса (по швидкості нагріву) <span class='tip' tabindex='0' data-tip='Коли УВІМКНЕНО: під час обігріву спочатку працює лише вентилятор. Якщо температура кімнати росте достатньо швидко — насос стоїть на 0%. Якщо приріст за вікно оцінки менший за поріг — насос підключається. Коли ВИМКНЕНО: працює простий режим по порогу теплоносія нижче.'>?</span></label>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>⚙️ Робоча потужність (%) <span class='tip' tabindex='0' data-tip='Швидкість, на якій насос підкачує теплоносій у штатному режимі (типово 33%). Автоматично обмежується межами «Мінімум/Максимум насоса» вище та використовує пусковий імпульс.'>?</span>:</label>";
    html += "<input type='number' name='spWorkPct' value='" + String(config.smartPump.workPercent) + "' min='10' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>⏱️ Вікно оцінки нагріву (сек) <span class='tip' tabindex='0' data-tip='Період спостереження за температурою кімнати без насоса (типово 180 сек = 3 хв). Після старту вентилятора або зупинки насоса система чекає цей час, а потім перевіряє, на скільки виросла температура кімнати.'>?</span>:</label>";
    html += "<input type='number' name='spEvalSec' value='" + String(config.smartPump.evalWindowSec) + "' min='30' max='900'>";
    html += "</div>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>📈 Мін. приріст T за вікно (°C) <span class='tip' tabindex='0' data-tip='Мінімальне зростання температури кімнати за «Вікно оцінки», якого достатньо, щоб НЕ вмикати насос (напр. 0.15°C за 3 хв = 0.5°C за 10 хв). Якщо виросло менше — вмикається насос. Якщо кімната падає на ≥0.25°C — насос підхоплює достроково через 30с.'>?</span>:</label>";
    html += "<input type='number' step='0.05' name='spMinRise' value='" + String(config.smartPump.minTempRise, 2) + "' min='0.02' max='2.00'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>🛑 Стоп по теплоносію (°C) <span class='tip' tabindex='0' data-tip='Коли насос підкачав гарячу воду і датчик теплоносія досяг цієї температури (та минув «Мін. час роботи»), насос вимикається, щоб уникнути перегріву (синусоїди) і дати вентилятору зняти тепло. Вкажіть 0, щоб насос працював аж до досягнення макс. температури кімнати.'>?</span>:</label>";
    html += "<input type='number' step='0.5' name='spCarStop' value='" + String(config.smartPump.carrierStopTemp, 1) + "' min='0' max='70'>";
    html += "</div>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>⏳ Мін. час роботи (сек) <span class='tip' tabindex='0' data-tip='Мінімальний час безперервної роботи насоса після вмикання (типово 60 с), щоб встигнути прогнати гарячу воду в радіатор і не смикати мотор короткими імпульсами. (При досягненні макс. T кімнати вимикається одразу).'>?</span>:</label>";
    html += "<input type='number' name='spMinRun' value='" + String(config.smartPump.minRunSec) + "' min='10' max='600'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>⏸️ Мін. пауза (сек) <span class='tip' tabindex='0' data-tip='Мінімальний час відпочинку насоса після вимкнення перед можливим повторним пуском (антициклювання, типово 120 с). Захищає насос від частих стартів.'>?</span>:</label>";
    html += "<input type='number' name='spMinOff' value='" + String(config.smartPump.minOffSec) + "' min='10' max='900'>";
    html += "</div>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label>🌡️ Поріг теплоносія для простого режиму (°C) <span class='tip' tabindex='0' data-tip='Працює ТІЛЬКИ якщо знята галочка «Увімкнути розумний режим насоса». Насос вмикається, коли теплоносій остигне нижче цієї температури (типово 30°C), і вимикається, коли прогріється на +2°C вище.'>?</span>:</label>";
    html += "<input type='number' step='0.5' name='spCarLow' value='" + String(config.smartPump.carrierLowTemp, 1) + "' min='15' max='60'>";
    html += "</div>";
    html += "</div>"; // end pump section

    // ========================================================================
    // СЕКЦІЯ 2: ВЕНТИЛЯТОР ОБІГРІВУ (обмеження, реле та плавний режим)
    // ========================================================================
    html += "<div class='section' style='border-left-color: #10b981;'>";
    html += "<h3>🌊 ВЕНТИЛЯТОР ОБІГРІВУ (обмеження та плавна робота)</h3>";
    html += "<p style='color: var(--m); font-size: 13.5px; margin-top: 0;'>Налаштування обертів, реле живлення 220В та плавного розгону/гальмування для економії ресурсу мотора.</p>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум вентилятора (%) <span class='tip' tabindex='0' data-tip='Найнижчі оберти, на яких вентилятор стабільно крутиться без зупинки та гулу в автоматичному режимі.'>?</span>:</label>";
    html += "<input type='number' name='fanMin' value='" + String(config.fanMinPercent) + "' min='0' max='30'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум вентилятора (%) <span class='tip' tabindex='0' data-tip='Максимальна дозволена швидкість вентилятора обігріву в автоматичному режимі.'>?</span>:</label>";
    html += "<input type='number' name='fanMax' value='" + String(config.fanMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='heaterFanRelay' " + String(config.heaterFanRelayEnabled ? "checked" : "") + "> 🔌 Увімкнути живлення 220В вентилятора (Реле Pin 7) <span class='tip' tabindex='0' data-tip='Дозвіл на вмикання реле живлення 220В вентилятора обігріву. Якщо зняти галочку — реле завжди буде вимкнене (безпечне блокування мотора).'>?</span></label>";
    html += "</div>";

    html += "<hr style='border:0;border-top:1px solid var(--b);margin:14px 0;'>";

    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='fsEn' " + String(config.fanSmooth.enabled ? "checked" : "") + "> Увімкнути плавний режим вентилятора <span class='tip' tabindex='0' data-tip='Запобігає різким стрибкам ШІМ та частим пускам/зупинкам вентилятора у всіх автоматичних режимах.'>?</span></label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='fsKeepLow' " + String(config.fanSmooth.keepLowInsteadOfStop ? "checked" : "") + "> Тримати низькі оберти замість стопу біля верхньої межі <span class='tip' tabindex='0' data-tip='У зоні від T_макс до T_макс + гістерезис вентилятор тримає мінімальні оберти замість повної зупинки. Це дає рівнішу температуру та менше пускових струмів.'>?</span></label>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>Гістерезис вентилятора (°C) <span class='tip' tabindex='0' data-tip='Зона нечутливості біля верхньої межі температури (типово 0.5°C), щоб вентилятор не клацав туди-сюди при коливанні температури на 0.1°C.'>?</span>:</label>";
    html += "<input type='number' step='0.1' name='fsHyst' value='" + String(config.fanSmooth.hysteresis, 1) + "' min='0.1' max='3.0'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Макс. зміна обертів (%/сек) <span class='tip' tabindex='0' data-tip='Обмеження швидкості розгону та гальмування вентилятора (рекомендовано 3–8 %/сек). Менше значення = плавніша робота.'>?</span>:</label>";
    html += "<input type='number' name='fsStep' value='" + String(config.fanSmooth.maxStepPerSec) + "' min='1' max='50'>";
    html += "</div>";
    html += "</div>";

    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>Мін. час роботи перед стопом (сек) <span class='tip' tabindex='0' data-tip='Після запуску вентилятор працює не менше цього часу (окрім критичного перегріву +1.5°C вище норми).'>?</span>:</label>";
    html += "<input type='number' name='fsMinRun' value='" + String(config.fanSmooth.minRunSec) + "' min='0' max='600'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мін. пауза перед пуском (сек) <span class='tip' tabindex='0' data-tip='Мінімальна пауза після зупинки вентилятора перед наступним увімкненням (антициклювання).'>?</span>:</label>";
    html += "<input type='number' name='fsMinOff' value='" + String(config.fanSmooth.minOffSec) + "' min='0' max='600'>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // end fan section

    // ========================================================================
    // СЕКЦІЯ 3: ВИТЯЖКА (обмеження потужності)
    // ========================================================================
    html += "<div class='section'>";
    html += "<h3>🌀 ВИТЯЖКА (обмеження потужності)</h3>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group'>";
    html += "<label>Мінімум витяжки (%) <span class='tip' tabindex='0' data-tip='Мінімальна потужність ШІМ для стабільного старту та роботи витяжки в автоматичному режимі.'>?</span>:</label>";
    html += "<input type='number' name='extractorMin' value='" + String(config.extractorMinPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимум витяжки (%) <span class='tip' tabindex='0' data-tip='Максимальна дозволена потужність витяжки в автоматичному режимі.'>?</span>:</label>";
    html += "<input type='number' name='extractorMax' value='" + String(config.extractorMaxPercent) + "' min='0' max='100'>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // end extractor section
    html += "</div>"; // tab1
    flushChunk();

    // TAB 2: ТАЙМЕР
    html += "<div class='tab-content' id='tab2'>";
    html += "<div class='section'>";
    html += "<h3>⏰ ТАЙМЕР ВИТЯЖКИ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Час роботи (хвилини):</label>";
    html += "<input type='number' name='extOnMin' value='" + String(config.extractorTimer.onMinutes) + "' min='0' max='120'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час роботи (секунди):</label>";
    html += "<input type='number' name='extOnSec' value='" + String(config.extractorTimer.onSeconds) + "' min='0' max='59'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час паузи (хвилини):</label>";
    html += "<input type='number' name='extOffMin' value='" + String(config.extractorTimer.offMinutes) + "' min='0' max='120'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час паузи (секунди):</label>";
    html += "<input type='number' name='extOffSec' value='" + String(config.extractorTimer.offSeconds) + "' min='0' max='59'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Потужність таймера (%):</label>";
    html += "<input type='number' name='extPower' value='" + String(config.extractorTimer.powerPercent) + "' min='10' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='extEnabled' " + String(config.extractorTimer.enabled ? "checked" : "") + "> Включити таймер</label>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab2

    // TAB 3: СЕЗОН
    html += "<div class='tab-content' id='tab3'>";
    html += "<div class='section'>";
    html += "<h3>🔄 СЕЗОННІ РЕЖИМИ</h3>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='coolingMode' " + String(config.coolingMode ? "checked" : "") + "> ❄️ Режим охолодження (літо: холодна вода в теплоносії)</label>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Інверсна логіка: вентилятор працює на максимум при перегріві, насос вимкнений</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='seasonalDisable' " + String(config.seasonalHeatingDisable ? "checked" : "") + "> 🌞 Автоматичне відключення обігріву (травень-вересень)</label>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Система автоматично відключить обігрів в теплі місяці</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab3

    // TAB 4: АВАРІЯ
    html += "<div class='tab-content' id='tab4'>";

    // Пороги температури для форсажу та аварії
    html += "<div class='section'>";
    html += "<h3>⚡ ПОРОГИ ФОРСАЖУ ТА АВАРІЇ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Поріг ФОРСАЖУ - критично низька температура (°C):</label>";
    html += "<input type='number' step='0.5' name='tempCriticalLow' value='" + String(config.tempCriticalLow, 1) + "'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Якщо температура кімнати ≤ цього порогу, вмикається ФОРСАЖ (насос 80%, вентилятор 80%)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Поріг АВАРІЇ - аварійна температура (°C):</label>";
    html += "<input type='number' step='0.5' name='tempEmergencyLow' value='" + String(config.tempEmergencyLow, 1) + "'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Якщо температура кімнати ≤ цього порогу, вмикається АВАРІЯ (насос 100%, вентилятор 100%)</small>";
    html += "</div>";
    html += "<div style='background: rgba(245,158,11,.14); padding: 10px; border-radius: 5px; margin-top: 10px; border-left: 4px solid #ffc107;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Поріг аварії має бути НИЖЧЕ за поріг форсажу!";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // tab4

    // TAB 5: ДАТЧИКИ
    html += "<div class='tab-content' id='tab5'>";
    html += "<div class='section'>";
    html += "<h3>💨 ДАТЧИК SCD30 (CO2)</h3>";
    html += "<div class='form-group'>";
    html += "<label>Інтервал вимірювання SCD30 (секунди):</label>";
    html += "<input type='number' name='scd30Interval' value='" + String(config.scd30MeasurementInterval > 0 ? config.scd30MeasurementInterval : 2) + "' min='2' max='1800'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>2-1800 сек. За замовчуванням 2 сек для отримання даних кожні 2 секунди</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab5

    // TAB 6: СИСТЕМА
    html += "<div class='tab-content' id='tab6'>";
    html += "<div class='section'>";
    html += "<h3>📊 МОНІТОРИНГ ТА ЛОГУВАННЯ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Період виведення статусу (секунди):</label>";
    html += "<input type='number' name='statusPeriod' value='" + String(config.statusPeriod / 1000) + "' min='10' max='600'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Як часто виводити статус у Serial Monitor</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='autoStatus'" + String(config.autoStatusEnabled ? " checked" : "") + "> Автоматичний вивід статусу в Serial</label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='use24h'" + String(config.use24hFormat ? " checked" : "") + "> Використовувати 24-годинний формат часу</label>";
    html += "</div>";
    html += "</div>";

    html += "<div class='section'>";
    html += "<h3>🔧 РОЗШИРЕНІ ПАРАМЕТРИ</h3>";
    html += "<div style='background: rgba(245,158,11,.14); padding: 10px; border-radius: 5px; margin-bottom: 15px;'>";
    html += "<small style='color: #856404;'>⚠️ <strong>Увага:</strong> Змінюйте ці параметри тільки якщо розумієте що вони роблять!</small>";
    html += "</div>";
    html += "<div class='form-group' title='Час (у хвилинах) протягом якого температура або вологість не можуть досягти заданих порогів, після чого вмикається адаптивний режим (зниження цілі)'>";
    html += "<label>Час до активації адаптації (хвилини):</label>";
    html += "<input type='number' name='adaptiveActTime' value='" + String(config.adaptive_activation_time_min) + "' min='1' max='240'>";
    html += "</div>";
    html += "<div class='form-group' title='Час (у хвилинах) після якого система спробує відновити оригінальні пороги (якщо температура почала стабілізуватися)'>";
    html += "<label>Час до спроби відновлення порогів (хвилини):</label>";
    html += "<input type='number' name='adaptiveRecTime' value='" + String(config.adaptive_recovery_time_min) + "' min='1' max='240'>";
    html += "</div>";
    html += "<div class='form-group' title='На скільки десятих градуса (наприклад, 1 = 0.1°C) знижувати цільову температуру при активації адаптивного режиму'>";
    html += "<label>Крок адаптації температури (0.1°C):</label>";
    html += "<input type='number' name='adaptiveTempStep' value='" + String(config.adaptive_temp_step) + "' min='1' max='20'>";
    html += "</div>";
    html += "<div class='form-group' title='На скільки відсотків (%) знижувати цільову вологість при активації адаптивного режиму'>";
    html += "<label>Крок адаптації вологості (%):</label>";
    html += "<input type='number' name='adaptiveHumStep' value='" + String(config.adaptive_hum_step) + "' min='1' max='20'>";
    html += "</div>";
    html += "</div>"; // tab6
    flushChunk();
    html += "</div>"; // tab6
    flushChunk();

    // TAB 7: СЕРВО
    html += "<div class='tab-content' id='tab7'>";
    html += "<div class='section'>";
    html += "<h3>🎯 КАЛІБРУВАННЯ СЕРВО</h3>";

    // Поточний стан
    html += "<div style='background: rgba(56,189,248,.14); padding: 15px; border-radius: 5px; margin-bottom: 20px; border-left: 4px solid #2196F3;'>";
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px;'>";
    html += "<div><strong>Поточний кут:</strong><br><span id='servoCurrentAngle' style='font-size: 24px; color: #2196F3;'>" + String(ventState.currentAngle) + "°</span></div>";
    html += "<div><strong>Закрито:</strong><br><span id='servoClosedVal'>" + String(config.servoClosedAngle) + "°</span></div>";
    html += "<div><strong>Відкрито:</strong><br><span id='servoOpenVal'>" + String(config.servoOpenAngle) + "°</span></div>";
    html += "<div><strong>Режим:</strong><br><span id='servoMode' style='font-weight:600;color:";
    html += ventState.calibrationMode ? "#FF9800'>КАЛІБРУВАННЯ" : "#2196F3'>НОРМАЛЬНИЙ";
    html += "</span></div>";
    html += "</div></div>";

    // Кнопки управління
    html += "<div style='margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;width:100%;' onclick='toggleServoCalibration()' id='servoCalibBtn'>";
    html += ventState.calibrationMode ? "🔓 ВИЙТИ З КАЛІБРУВАННЯ" : "🔒 УВІЙТИ В КАЛІБРУВАННЯ";
    html += "</button>";
    html += "</div>";

    // Кнопки руху
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn' style='background:#2196F3;color:white;' onclick='moveServo(\"+1\")'>▲ +1°</button>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;' onclick='moveServo(\"+5\")'>▲▲ +5°</button>";
    html += "<button type='button' class='btn' style='background:#2196F3;color:white;' onclick='moveServo(\"-1\")'>▼ -1°</button>";
    html += "<button type='button' class='btn' style='background:#FF9800;color:white;' onclick='moveServo(\"-5\")'>▼▼ -5°</button>";
    html += "</div>";

    // Швидкі позиції
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn btn-success' onclick='gotoServoPosition(\"open\")'>➤ Відкрити</button>";
    html += "<button type='button' class='btn btn-danger' onclick='gotoServoPosition(\"closed\")'>➤ Закрити</button>";
    html += "</div>";

    // Збереження позицій
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px;'>";
    html += "<button type='button' class='btn btn-success' onclick='saveServoPosition(\"closed\")'>💾 Зберегти як ЗАКРИТО</button>";
    html += "<button type='button' class='btn btn-success' onclick='saveServoPosition(\"open\")'>💾 Зберегти як ВІДКРИТО</button>";
    html += "</div>";

    // Тест
    html += "<button type='button' class='btn' style='background:#9C27B0;color:white;width:100%;margin-bottom:20px;' onclick='testServo()'>🔧 ТЕСТ (відкрити→закрити)</button>";

    // Ручне введення та налаштування
    html += "<div class='form-group'>";
    html += "<label>Кут закритої заслонки (градуси):</label>";
    html += "<input type='number' id='manualServoClosed' name='servoClosed' value='" + String(config.servoClosedAngle) + "' min='0' max='180'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Кут відкритої заслонки (градуси):</label>";
    html += "<input type='number' id='manualServoOpen' name='servoOpen' value='" + String(config.servoOpenAngle) + "' min='0' max='180'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Швидкість руху серво (мс/градус):</label>";
    html += "<input type='number' name='servoSpeed' value='" + String(config.servoSpeed) + "' min='5' max='50'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Менше = швидше (5-10 швидко, 20-30 стандарт, 40-50 повільно)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='manualVent'" + String(config.manualVentControl ? " checked" : "") + "> Ручне керування заслонкою</label>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Вимкнути автоматичне регулювання заслонки (тільки вимикач)</small>";
    html += "</div>";

    html += "<div style='background: rgba(245,158,11,.14); padding: 10px; border-radius: 5px; margin-top: 10px;'>";
    html += "<small style='color: #856404;'>💡 <strong>Порада:</strong> Використовуйте кнопки для точного калібрування в реальному часі, або введіть значення вручну та збережіть через кнопку внизу форми.</small>";
    html += "</div>";
    html += "</div>";
    html += "</div>"; // tab6
    flushChunk();

    // TAB 7: WIFI
    html += "<div class='tab-content' id='tab8'>";

    // Поточний стан WiFi
    html += "<div class='section'>";
    html += "<h3>📶 ПОТОЧНЕ ПІДКЛЮЧЕННЯ</h3>";
    html += "<div style='background: rgba(56,189,248,.14); padding: 15px; border-radius: 5px; border-left: 4px solid #2196F3;'>";
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px;'>";
    html += "<div><strong>SSID:</strong><br>" + htmlEscape(WiFi.SSID()) + "</div>";
    html += "<div><strong>IP адреса:</strong><br>" + WiFi.localIP().toString() + "</div>";
    html += "<div><strong>Сила сигналу:</strong><br>" + String(WiFi.RSSI()) + " dBm";
    if (WiFi.RSSI() > -50) html += " (відмінно)";
    else if (WiFi.RSSI() > -60) html += " (добре)";
    else if (WiFi.RSSI() > -70) html += " (задовільно)";
    else html += " (слабко)";
    html += "</div>";
    html += "<div><strong>MAC адреса:</strong><br>" + WiFi.macAddress() + "</div>";
    html += "</div></div></div>";

    // Зміна WiFi мережі
    html += "<div class='section'>";
    html += "<h3>🔄 ЗМІНИТИ WI-FI МЕРЕЖУ</h3>";
    html += "<div class='form-group'>";
    html += "<label>SSID (назва мережі):</label>";
    html += "<input type='text' name='wifi_ssid' value='" + htmlEscape(WiFi.SSID()) + "' placeholder='Назва WiFi мережі'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Пароль:</label>";
    html += "<input type='password' name='wifi_password' value='' placeholder='Залиште порожнім щоб не змінювати'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Для безпеки пароль не відображається</small>";
    html += "</div>";
    html += "<div style='background: rgba(245,158,11,.14); padding: 15px; border-radius: 5px; margin-top: 10px; border-left: 4px solid #ffc107;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Після зміни WiFi налаштувань пристрій перезавантажиться та підключиться до нової мережі. ";
    html += "Переконайтеся, що ввели правильні дані!";
    html += "</div></div>";

    // Статична IP
    html += "<div class='section'>";
    html += "<h3>🌐 СТАТИЧНА IP (опціонально)</h3>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='use_static_ip' id='use_static_ip'";
    if (config.useStaticIP) html += " checked";
    html += " onchange='toggleStaticIP()'> Використовувати статичну IP адресу</label>";
    html += "</div>";
    String staticIPDisplay = config.useStaticIP ? "block" : "none";
    html += "<div id='static_ip_fields' style='display:" + staticIPDisplay + ";'>";
    html += "<div class='form-group'>";
    html += "<label>IP адреса:</label>";
    html += "<input type='text' name='static_ip' value='" + config.staticIP + "' placeholder='192.168.1.100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Шлюз (Gateway):</label>";
    html += "<input type='text' name='gateway' value='" + config.gateway + "' placeholder='192.168.1.1'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Маска підмережі:</label>";
    html += "<input type='text' name='subnet' value='" + config.subnet + "' placeholder='255.255.255.0'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>DNS сервер:</label>";
    html += "<input type='text' name='dns' value='" + config.dns + "' placeholder='8.8.8.8'>";
    html += "</div>";
    html += "</div></div>";

    // Безпека
    html += "<div class='section'>";
    html += "<h3>🔐 БЕЗПЕКА ВЕБ-ІНТЕРФЕЙСУ</h3>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='use_auth' id='use_auth'";
    if (config.useAuth) html += " checked";
    html += " onchange='toggleAuth()'> Увімкнути автентифікацію (логін/пароль)</label>";
    html += "</div>";
    String authDisplay = config.useAuth ? "block" : "none";
    html += "<div id='auth_fields' style='display:" + authDisplay + ";'>";
    html += "<div class='form-group'>";
    html += "<label>Логін:</label>";
    html += "<input type='text' name='auth_user' value='" + config.authLogin + "' placeholder='admin'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Пароль:</label>";
    html += "<input type='password' name='auth_pass' value='' placeholder='Залиште порожнім щоб не змінювати'>";
    html += "</div>";
    html += "<div style='background: rgba(239,68,68,.15); padding: 10px; border-radius: 5px; margin-top: 10px; border-left: 4px solid #f44336;'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Обов'язково увімкніть автентифікацію якщо плануєте відкрити доступ через інтернет!";
    html += "</div>";
    html += "</div></div>";

    html += "</div>"; // tab7

    // TAB 8: GOOGLE SHEETS SYNC
    html += "<div class='tab-content' id='tab9'>";

    // Інтервал запису в SPIFFS
    html += "<div class='section'>";
    html += "<h3>💾 ІНТЕРВАЛ ЗАПИСУ В SPIFFS</h3>";
    html += "<div class='form-group'>";
    html += "<label>Інтервал запису агрегованих даних (хвилини): <strong id='spiffsLogVal'>" + String(config.spiffsLogInterval) + "</strong></label>";
    html += "<input type='range' name='spiffsLogInterval' min='1' max='60' value='" + String(config.spiffsLogInterval) + "' oninput=\"document.getElementById('spiffsLogVal').textContent=this.value\">";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Менший інтервал = більше деталей у SPIFFS. Поточний: " + String(config.spiffsLogInterval) + " хв. За замовчуванням: 5 хв.</small>";
    html += "</div>";
    html += "</div>";

    // Інтервал відправки в Google Sheets
    html += "<div class='section'>";
    html += "<h3>☁️ ІНТЕРВАЛ ВІДПРАВКИ В GOOGLE SHEETS</h3>";
    html += "<div class='form-group'>";
    html += "<label>Регулярний інтервал відправки (хвилини): <strong id='sheetsIntVal'>" + String(config.sheetsSyncIntervalMin) + "</strong></label>";
    html += "<input type='range' name='sheetsSyncInterval' min='1' max='120' value='" + String(config.sheetsSyncIntervalMin) + "' oninput=\"document.getElementById('sheetsIntVal').textContent=this.value\">";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Дані відправляються, якщо температура кімнати змінилась на поріг нижче, АБО гарантовано кожні вказані хвилини (навіть без зміни температури). Поточний: " + String(config.sheetsSyncIntervalMin) + " хв. Типово: 15 хв.</small>";
    html += "</div>";
    html += "</div>";

    // Поріг логування
    html += "<div class='section'>";
    html += "<h3>📊 ПОРІГ ЗАПИСУ ДАНИХ</h3>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна зміна температури кімнати для запису (°C):</label>";
    html += "<input type='number' step='0.1' name='logTempThreshold' value='" + String(config.logTempThreshold, 1) + "' min='0.0' max='5.0'>";
    html += "<small style='color: var(--m); display: block; margin-top: 5px;'>Якщо температура змінилась менше ніж на цей поріг, рядок не буде записано (поки не мине інтервал вище). 0 = записувати всі зміни. Типове значення: 0.5°C</small>";
    html += "</div>";
    html += "</div>";

    // Інформація про синхронізацію
    html += "<div class='section'>";
    html += "<h3>ℹ️ ІНФОРМАЦІЯ</h3>";
    html += "<div style='background: rgba(16,185,129,.14); padding: 15px; border-radius: 5px; border-left: 4px solid #4CAF50;'>";
    html += "<p><strong>Як працює фільтрація:</strong></p>";
    html += "<ul style='margin: 10px 0; padding-left: 20px;'>";
    html += "<li>Дані записуються в буфер тільки якщо температура кімнати змінилась на вказаний поріг</li>";
    html += "<li>Це зменшує кількість записів при стабільних умовах</li>";
    html += "<li>Менший поріг = більше записів, більший поріг = менше записів</li>";
    html += "</ul>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // tab8

    // TAB 9: ЧАС
    html += "<div class='tab-content' id='tab10'>";
    html += "<div class='section' style='border-left-color: #2196F3;'>";
    html += "<h3>🕒 ЧАС СИСТЕМИ</h3>";
    html += "<div style='display: grid; gap: 15px;'>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Поточний час:</strong>";
    html += "<span style='color: #2196F3; font-weight: 600;'>" + getTimeString() + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Дата:</strong>";
    html += "<span style='color: #2196F3; font-weight: 600;'>" + getDateString() + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Формат часу:</strong>";
    html += "<span style='color: #2196F3; font-weight: 600;'>" + getFormattedTime() + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: " + String(isTimeSynced() ? "#e8f5e9" : "#fff3cd") + "; border-radius: 5px;'>";
    html += "<strong>Синхронізація:</strong>";
    html += "<span style='color: " + String(isTimeSynced() ? "#4CAF50" : "#ff9800") + "; font-weight: 600;'>" + String(isTimeSynced() ? "✅ Синхронізовано" : "⚠️ Немає синхронізації") + "</span>";
    html += "</div>";

    html += "</div>"; // grid
    html += "</div>"; // section

    html += "<div class='section' style='border-left-color: #2196F3;'>";
    html += "<h3>ℹ️ Про синхронізацію часу</h3>";
    html += "<p>Система використовує NTP для автоматичної синхронізації часу через інтернет.</p>";
    html += "<ul style='margin: 10px 0; padding-left: 20px;'>";
    html += "<li>Часовий пояс: Europe/Kyiv (UTC+2 / UTC+3)</li>";
    html += "<li>NTP сервер: pool.ntp.org</li>";
    html += "<li>Синхронізація відбувається при підключенні до WiFi</li>";
    html += "</ul>";
    html += "</div>";

    html += "</div>"; // tab10
    flushChunk();

    // TAB 11: ВЕНТИЛЯТОР РОСЛИН
    html += "<div class='tab-content' id='tab11'>";

    // Індикатор поточної потужності
    html += "<div class='section' style='border-left-color: #4CAF50;'>";
    html += "<h3>💨 ВЕНТИЛЯТОР ТА ПРИРОДНІЙ ВІТЕР</h3>";
    html += "<div style='background: rgba(16,185,129,.14);padding:12px;border-radius:6px;margin-bottom:15px;display:flex;align-items:center;gap:15px;'>";
    html += "<span style='font-weight:bold;color: var(--t);'>Поточна потужність:</span>";
    html += "<span id='plfPowerVal' style='font-size:22px;font-weight:bold;color:#4CAF50;'>—</span>";
    html += "<div style='flex:1;background:#ddd;border-radius:4px;height:12px;'>";
    html += "<div id='plfPowerBar' style='height:12px;background:#4CAF50;border-radius:4px;width:0%;transition:width 0.3s;'></div>";
    html += "</div>";
    html += "<span id='plfTimerState' style='font-size:13px;color: var(--m);'></span>";
    html += "</div>";
    html += "<script>";
    html += "function updatePlfIndicator(){";
    html += "  var t=document.getElementById('tab11');";
    html += "  if(!t||!t.classList.contains('active')) return;";
    html += "  fetch('/status').then(r=>r.json()).then(d=>{";
    html += "    var p=d.plantFanPower||0;";
    html += "    document.getElementById('plfPowerVal').textContent=p+'%';";
    html += "    document.getElementById('plfPowerBar').style.width=p+'%';";
    html += "    document.getElementById('plfTimerState').textContent=d.plantFanOn?'▶ ПРАЦЮЄ':'⏸ ПАУЗА';";
    html += "  }).catch(()=>{})";
    html += "}";
    html += "setInterval(updatePlfIndicator,2000);";
    html += "updatePlfIndicator();";
    html += "</script>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='plfEnabled' " + String(config.plantFanTimer.enabled ? "checked" : "") + "> Увімкнути таймер</label>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label><input type='checkbox' name='plfBreeze' " + String(config.plantFanTimer.breezeEnabled ? "checked" : "") + "> Природній вітер під час роботи</label>";
    html += "<small style='color:#777;display:block;margin-top:4px;'>Якщо увімкнено — швидкість змінюється як вітер. Якщо ні — фіксована потужність.</small>";
    html += "</div>";
    html += "</div>";

    // Таймер
    html += "<div class='section' style='border-left-color: #FF9800;'>";
    html += "<h3>⏰ ТАЙМЕР РОСЛИН</h3>";
    html += "<div class='form-group'>";
    html += "<label>Час роботи (хв):</label>";
    html += "<input type='number' name='plfOnMin' value='" + String(config.plantFanTimer.onMinutes) + "' min='0' max='240'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час роботи (сек):</label>";
    html += "<input type='number' name='plfOnSec' value='" + String(config.plantFanTimer.onSeconds) + "' min='0' max='59'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час паузи (хв):</label>";
    html += "<input type='number' name='plfOffMin' value='" + String(config.plantFanTimer.offMinutes) + "' min='0' max='240'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Час паузи (сек):</label>";
    html += "<input type='number' name='plfOffSec' value='" + String(config.plantFanTimer.offSeconds) + "' min='0' max='59'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Потужність таймера (%):</label>";
    html += "<input type='number' name='plfPower' value='" + String(config.plantFanTimer.powerPercent) + "' min='10' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна швидкість запуску (%):</label>";
    html += "<small style='color:#777;display:block;margin-bottom:5px;'>Апаратний мінімум — нижче цього значення вентилятор не стартує</small>";
    html += "<input type='number' name='plfMinStart' value='" + String(config.plantFanTimer.minStartPercent) + "' min='1' max='90'>";
    html += "</div>";
    html += "</div>";

    // Природній вітер
    html += "<div class='section' style='border-left-color: #2196F3;'>";
    html += "<h3>💨 ПРИРОДНІЙ ВІТЕР</h3>";
    html += "<p style='color: var(--m); font-size:14px;'>Імітація природного вітру: базова швидкість хаотично коливається, рандомні пориви наростають плавно і спадають.</p>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна базова швидкість (%):</label>";
    html += "<input type='number' name='brzBaseMin' value='" + String(config.breezeConfig.baseSpeedMin) + "' min='5' max='80'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальна базова швидкість (%):</label>";
    html += "<input type='number' name='brzBaseMax' value='" + String(config.breezeConfig.baseSpeedMax) + "' min='10' max='100'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Підсилення пориву (% понад базу):</label>";
    html += "<input type='number' name='brzBoost' value='" + String(config.breezeConfig.gustBoost) + "' min='5' max='60'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальна тривалість пориву (сек):</label>";
    html += "<input type='number' name='brzGstMin' value='" + String(config.breezeConfig.gustMinSec) + "' min='1' max='30'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальна тривалість пориву (сек):</label>";
    html += "<input type='number' name='brzGstMax' value='" + String(config.breezeConfig.gustMaxSec) + "' min='2' max='60'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Мінімальне затишшя (сек):</label>";
    html += "<input type='number' name='brzCalmMin' value='" + String(config.breezeConfig.calmMinSec) + "' min='2' max='120'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Максимальне затишшя (сек):</label>";
    html += "<input type='number' name='brzCalmMax' value='" + String(config.breezeConfig.calmMaxSec) + "' min='5' max='300'>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // tab11
    flushChunk();

    // TAB 12: ЕНЕРГОЗБЕРЕЖЕННЯ / БЛЕКАУТ (ECO)
    html += "<div class='tab-content' id='tab12'>";

    // Швидкі пресети та поточний стан
    html += "<div class='section' style='border-left-color: #f59e0b;'>";
    html += "<h3>🔋 РЕЖИМ ЕНЕРГОЗБЕРЕЖЕННЯ (БЛЕКАУТ / АКБ)</h3>";
    html += "<p style='color:var(--m);font-size:13px;margin-top:0;'>💡 Наведіть курсор (або натисніть) на значок <span class='tip' tabindex='0' data-tip='Тут зʼявляються детальні підказки до кожного параметра ЕКО-режиму'>?</span> біля будь-якого пункту, щоб побачити пояснення.</p>";
    html += "<div style='background: rgba(245,158,11,.14); padding: 12px; border-radius: 8px; margin-bottom: 15px;'>";
    html += "<strong>Поточний стан:</strong> " + String(isEcoModeActive() ? "🔋 ЕКО АКТИВНИЙ" : "🟢 Штатний режим (220В)");
    html += " | <strong>Напруга PZEM:</strong> " + String(powerOutageState.lastVoltage, 1) + " В";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Швидкі профілі <span class='tip' tabindex='0' data-tip='Підставляють готові рекомендовані значення в поля нижче без автозбереження. Перевірте цифри і натисніть «Зберегти» внизу.'>?</span>:</label>";
    html += "<div style='display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 8px; margin-top: 6px;'>";
    html += "<button type='button' class='btn' style='background:#ef4444;color:#fff;margin:0;' onclick='applyEcoPresetUI(0)' title='Максимальна економія старого АКБ: витяжка/вітер/зволожувач/світло OFF, обігрів до 19°C на низьких обертах з імпульсним насосом'>🆘 Виживання (старий АКБ)</button>";
    html += "<button type='button' class='btn' style='background:#f59e0b;color:#fff;margin:0;' onclick='applyEcoPresetUI(1)' title='Помірна економія: обігрів 21°C, рідкі цикли витяжки та вітру, знижені пороги вологості 45-50%, світло 25%'>⚖️ Збалансований ЕКО</button>";
    html += "<button type='button' class='btn' style='background:#10b981;color:#fff;margin:0;' onclick='applyEcoPresetUI(2)' title='Усе силове навантаження OFF — працює лише ESP32 та датчики'>☀️ Теплий сезон (все OFF)</button>";
    html += "</div>";
    html += "<div id='ecoPresetMsg' style='display:none; margin-top:10px; padding:10px 12px; border-radius:8px; background:rgba(16,185,129,.16); border-left:4px solid #10b981; font-size:13.5px;'></div>";
    html += "</div>";

    html += "<div class='form-group' title='Визначає, коли вмикається профіль економії: автоматично при зникненні 220В на PZEM, вручну примусово або повністю вимкнено'>";
    html += "<label>Активація ЕКО-режиму <span class='tip' tabindex='0' data-tip='АВТО — вмикається сам, коли PZEM на вході бачить зникнення 220В. ПРИМУСОВО — тримає економію постійно, навіть при 220В. ВИМКНЕНО — ігнорує відключення 220В.'>?</span>:</label>";
    html += "<select name='ecoActMode'>";
    html += "<option value='0'" + String(config.powerSave.activationMode == 0 ? " selected" : "") + ">🤖 Автоматично при зникненні 220В (PZEM)</option>";
    html += "<option value='1'" + String(config.powerSave.activationMode == 1 ? " selected" : "") + ">🔋 Примусово ЕКО (ручний режим економії)</option>";
    html += "<option value='2'" + String(config.powerSave.activationMode == 2 ? " selected" : "") + ">⛔ Вимкнено (ігнорувати відключення 220В)</option>";
    html += "</select>";
    html += "</div>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group' title='Скільки секунд має бути відсутня напруга 220В, перш ніж система перейде в ЕКО-режим (захист від коротких мигань)'>";
    html += "<label>Підтвердження блекауту (сек) <span class='tip' tabindex='0' data-tip='Затримка перед входом в ЕКО при зникненні 220В. Захищає від хибних спрацювань при короткому миганні світла (рекомендовано 5–10 с).'>?</span>:</label>";
    html += "<input type='number' name='ecoOutSec' value='" + String(config.powerSave.outageConfirmSec) + "' min='0' max='120'></div>";
    html += "<div class='form-group' title='Скільки секунд напруга 220В має триматися стабільно після повернення, перш ніж повернути повне навантаження'>";
    html += "<label>Стабілізація після 220В (сек) <span class='tip' tabindex='0' data-tip='Пауза після появи 220В перед поверненням штатної потужності. Береже блок живлення та обладнання від повторних стрибків (рекомендовано 10–30 с).'>?</span>:</label>";
    html += "<input type='number' name='ecoResSec' value='" + String(config.powerSave.restoreConfirmSec) + "' min='0' max='300'></div>";
    html += "</div>";
    html += "</div>";

    // Обігрів під час блекауту
    html += "<div class='section' style='border-left-color: #ef4444;'>";
    html += "<h3>🔥 ОБІГРІВ ПРИ БЛЕКАУТІ (НАСОС + ВЕНТИЛЯТОР)</h3>";
    html += "<div class='form-group' title='Оберіть поведінку обігріву при роботі від акумулятора'>";
    html += "<label>Політика обігріву <span class='tip' tabindex='0' data-tip='Вимкнути — 0% на насос і вентилятор. ЕКО-обігрів — тримає лише мінімальну температуру на низьких рівних обертах. Штатний — звичайний обігрів без обмежень ЕКО.'>?</span>:</label><select name='ecoHeatPol'>";
    html += "<option value='0'" + String(config.powerSave.heatingPolicy == 0 ? " selected" : "") + ">⛔ Вимкнути повністю (0%)</option>";
    html += "<option value='1'" + String(config.powerSave.heatingPolicy == 1 ? " selected" : "") + ">🔋 ЕКО-обігрів (низькі рівні оберти + гістерезис)</option>";
    html += "<option value='2'" + String(config.powerSave.heatingPolicy == 2 ? " selected" : "") + ">🔄 Залишити штатний режим</option>";
    html += "</select></div>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group' title='Температура в кімнаті, нижче якої вмикається ЕКО-обігрів (наприклад, 19–20°C для виживання рослин)'>";
    html += "<label>ЕКО мін. температура (°C) <span class='tip' tabindex='0' data-tip='Поріг увімкнення обігріву під час блекауту. Чим нижче це значення (напр. 19°C замість 24°C), тим довше протягне акумулятор. Основні налаштування не змінюються.'>?</span>:</label>";
    html += "<input type='number' step='0.5' name='ecoTempMin' value='" + String(config.powerSave.ecoTempMin, 1) + "' min='10' max='35'></div>";
    html += "<div class='form-group' title='На скільки градусів вище ЕКО мін. температури треба прогріти кімнату перед зупинкою обігріву'>";
    html += "<label>Гістерезис ЕКО-обігріву (°C) <span class='tip' tabindex='0' data-tip='Запас нагріву до зупинки (напр. при мін. 19°C і гістерезисі 1.0°C обігрів ввімкнеться на 19°C і вимкнеться на 20°C). Запобігає частим пускам.'>?</span>:</label>";
    html += "<input type='number' step='0.1' name='ecoTempHyst' value='" + String(config.powerSave.ecoTempHyst, 1) + "' min='0.2' max='5.0'></div>";
    html += "<div class='form-group' title='Максимальна робоча потужність насоса при блекауті (при старті автоматично спрацює Kickstart)'>";
    html += "<label>Макс. потужність насоса (%) <span class='tip' tabindex='0' data-tip='Робоча потужність насоса в ЕКО-режимі (напр. 33–40%). При запуску з 0% автоматично спрацює пусковий імпульс (Kickstart 45%), а потім знизиться сюди.'>?</span>:</label>";
    html += "<input type='number' name='ecoPumpMax' value='" + String(config.powerSave.ecoPumpMaxPercent) + "' min='0' max='100'></div>";
    html += "<div class='form-group' title='Максимальна швидкість вентилятора обігріву при блекауті'>";
    html += "<label>Макс. потужність вентилятора (%) <span class='tip' tabindex='0' data-tip='Рівні низькі оберти (20–35%) споживають у 4–6 разів менше заряду АКБ, ніж високі оберти або часті старти.'>?</span>:</label>";
    html += "<input type='number' name='ecoFanMax' value='" + String(config.powerSave.ecoFanMaxPercent) + "' min='0' max='100'></div>";
    html += "</div>";
    html += "<div class='form-group' title='Якщо теплоносій остиг і не тепліший за кімнату — насос і вентилятор не вмикатимуться дарма'>";
    html += "<label><input type='checkbox' name='ecoChkCar' " + String(config.powerSave.ecoCheckCarrierTemp ? "checked" : "") + "> Не крутити вхолосту, якщо теплоносій холодний <span class='tip' tabindex='0' data-tip='Захищає АКБ від марної витрати заряду, коли котел/теплоносій уже остиг і гріти все одно нічим.'>?</span></label>";
    html += "</div>";
    html += "<div class='form-group' title='На скільки градусів теплоносій має бути теплішим за кімнату, щоб дозволити роботу обігріву'>";
    html += "<label>Мін. різниця T_теплоносія − T_кімнати (°C) <span class='tip' tabindex='0' data-tip='Наприклад, при 3°C і кімнаті 19°C обігрів працюватиме лише поки теплоносій тепліший за 22°C.'>?</span>:</label>";
    html += "<input type='number' step='0.5' name='ecoCarDlt' value='" + String(config.powerSave.ecoCarrierMinDelta, 1) + "' min='0.5' max='20.0'></div>";
    html += "<div class='form-group' title='Керування реле живлення 220В вентилятора обігріву (Pin 7) під час блекауту'>";
    html += "<label><input type='checkbox' name='ecoHtrRel' " + String(config.powerSave.ecoAllowHeaterRelay ? "checked" : "") + "> Дозволити реле 220В вентилятора (Pin 7) при блекауті <span class='tip' tabindex='0' data-tip='Залиште вимкненим, щоб не тримати зайву котушку реле 220В під напругою під час відсутності мережі.'>?</span></label>";
    html += "</div>";
    html += "<div class='form-group' title='Періодичне вмикання насоса замість безперервної роботи при блекауті'>";
    html += "<label><input type='checkbox' name='ecoPmpPls' " + String(config.powerSave.ecoPumpPulseMode ? "checked" : "") + "> Імпульсний режим насоса (цикли ON/OFF для економії АКБ) <span class='tip' tabindex='0' data-tip='Замість постійної роботи насос проганяє гарячу воду задану кількість секунд, а потім робить паузу. Зменшує витрату АКБ у 2–4 рази.'>?</span></label>";
    html += "</div>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group' title='Тривалість роботи насоса в одному циклі імпульсного режиму'>";
    html += "<label>Імпульс насоса ON (сек) <span class='tip' tabindex='0' data-tip='Скільки секунд насос качає теплоносій у кожному циклі (рекомендовано 60–120 с).'>?</span>:</label>";
    html += "<input type='number' name='ecoPlsOn' value='" + String(config.powerSave.ecoPumpPulseOnSec) + "' min='10' max='1800'></div>";
    html += "<div class='form-group' title='Тривалість паузи насоса між імпульсами'>";
    html += "<label>Імпульс насоса пауза OFF (сек) <span class='tip' tabindex='0' data-tip='Скільки секунд насос відпочиває між імпульсами (рекомендовано 180–300 с).'>?</span>:</label>";
    html += "<input type='number' name='ecoPlsOff' value='" + String(config.powerSave.ecoPumpPulseOffSec) + "' min='0' max='3600'></div>";
    html += "</div>";
    html += "</div>";

    // Витяжка при блекауті
    html += "<div class='section' style='border-left-color: #3b82f6;'>";
    html += "<h3>🌀 ВИТЯЖКА ПРИ БЛЕКАУТІ</h3>";
    html += "<div class='form-group' title='Поведінка витяжки під час відключення світла'>";
    html += "<label>Політика витяжки <span class='tip' tabindex='0' data-tip='Вимкнути — зберігає і заряд АКБ, і тепло в приміщенні. ЕКО-таймер — коротке провітрювання на малих обертах. Без змін — працює звичайний таймер.'>?</span>:</label><select name='ecoExtPol'>";
    html += "<option value='0'" + String(config.powerSave.extractorPolicy == 0 ? " selected" : "") + ">⛔ Вимкнути повністю (0%)</option>";
    html += "<option value='1'" + String(config.powerSave.extractorPolicy == 1 ? " selected" : "") + ">🔋 ЕКО-таймер (рідкі цикли + низька потужність)</option>";
    html += "<option value='2'" + String(config.powerSave.extractorPolicy == 2 ? " selected" : "") + ">🔄 Без змін (штатний таймер)</option>";
    html += "</select></div>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;'>";
    html += "<div class='form-group' title='Хвилин роботи витяжки в ЕКО-таймері'><label>Робота (хв) <span class='tip' tabindex='0' data-tip='Тривалість одного провітрювання в ЕКО-режимі (напр. 2 хв).'>?</span>:</label><input type='number' name='ecoExtOn' value='" + String(config.powerSave.ecoExtOnMin) + "' min='0' max='120'></div>";
    html += "<div class='form-group' title='Хвилин паузи витяжки в ЕКО-таймері'><label>Пауза (хв) <span class='tip' tabindex='0' data-tip='Пауза між провітрюваннями в ЕКО-режимі (напр. 58 хв).'>?</span>:</label><input type='number' name='ecoExtOff' value='" + String(config.powerSave.ecoExtOffMin) + "' min='0' max='240'></div>";
    html += "<div class='form-group' title='Потужність витяжки в ЕКО-таймері'><label>Потужність (%) <span class='tip' tabindex='0' data-tip='Обмежена швидкість витяжки під час ЕКО-провітрювання (напр. 25%).'>?</span>:</label><input type='number' name='ecoExtPwr' value='" + String(config.powerSave.ecoExtPower) + "' min='5' max='100'></div>";
    html += "</div>";
    html += "</div>";

    // Вітер (вентилятор рослин) при блекауті
    html += "<div class='section' style='border-left-color: #10b981;'>";
    html += "<h3>💨 ВІТЕР РОСЛИН ПРИ БЛЕКАУТІ</h3>";
    html += "<div class='form-group' title='Поведінка вентилятора обдуву рослин під час блекауту'>";
    html += "<label>Політика вітру <span class='tip' tabindex='0' data-tip='Вимкнути — 0% на вентилятор рослин. ЕКО-таймер — без енерговитратних поривів, лише рівні мінімальні оберти за рідким графіком.'>?</span>:</label><select name='ecoPlfPol'>";
    html += "<option value='0'" + String(config.powerSave.plantFanPolicy == 0 ? " selected" : "") + ">⛔ Вимкнути повністю (0%)</option>";
    html += "<option value='1'" + String(config.powerSave.plantFanPolicy == 1 ? " selected" : "") + ">🔋 ЕКО-таймер (рівна низька швидкість без поривів)</option>";
    html += "<option value='2'" + String(config.powerSave.plantFanPolicy == 2 ? " selected" : "") + ">🔄 Без змін (штатний режим)</option>";
    html += "</select></div>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;'>";
    html += "<div class='form-group' title='Хвилин роботи обдуву в ЕКО-таймері'><label>Робота (хв) <span class='tip' tabindex='0' data-tip='Скільки хвилин дує обдув рослин в одному ЕКО-циклі (напр. 5 хв).'>?</span>:</label><input type='number' name='ecoPlfOn' value='" + String(config.powerSave.ecoPlantFanOnMin) + "' min='0' max='120'></div>";
    html += "<div class='form-group' title='Хвилин паузи обдуву в ЕКО-таймері'><label>Пауза (хв) <span class='tip' tabindex='0' data-tip='Скільки хвилин обдув вимкнений між циклами (напр. 55 хв).'>?</span>:</label><input type='number' name='ecoPlfOff' value='" + String(config.powerSave.ecoPlantFanOffMin) + "' min='0' max='240'></div>";
    html += "<div class='form-group' title='Фіксована швидкість обдуву в ЕКО-режимі (без поривів)'><label>Потужність (%) <span class='tip' tabindex='0' data-tip='Рівна швидкість обдуву без поривів вітру (напр. 20%).'>?</span>:</label><input type='number' name='ecoPlfPwr' value='" + String(config.powerSave.ecoPlantFanPower) + "' min='5' max='100'></div>";
    html += "</div>";
    html += "</div>";

    // Зволожувач при блекауті (зі зниженими порогами вологості)
    html += "<div class='section' style='border-left-color: #06b6d4;'>";
    html += "<h3>💧 ЗВОЛОЖУВАЧ ПРИ БЛЕКАУТІ</h3>";
    html += "<div class='form-group' title='Поведінка зволожувача під час відключення світла'>";
    html += "<label>Політика зволожувача <span class='tip' tabindex='0' data-tip='Вимкнути — зволожувач не працює. ЕКО-пороги — використовує знижену ціль вологості нижче, щоб рідше вмикатись і економити АКБ. Без змін — штатні пороги.'>?</span>:</label><select name='ecoHumPol'>";
    html += "<option value='0'" + String(config.powerSave.humidifierPolicy == 0 ? " selected" : "") + ">⛔ Вимкнути повністю</option>";
    html += "<option value='1'" + String(config.powerSave.humidifierPolicy == 1 ? " selected" : "") + ">🔋 ЕКО-пороги вологості (знижені)</option>";
    html += "<option value='2'" + String(config.powerSave.humidifierPolicy == 2 ? " selected" : "") + ">🔄 Без змін (штатні пороги)</option>";
    html += "</select></div>";
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;'>";
    html += "<div class='form-group' title='Вологість, нижче якої зволожувач увімкнеться в ЕКО-режимі'>";
    html += "<label>ЕКО мін. вологість (%) <span class='tip' tabindex='0' data-tip='Поріг увімкнення зволожувача при блекауті (напр. 45% замість 65%). Чим нижче поріг — тим рідше працює зволожувач.'>?</span>:</label>";
    html += "<input type='number' step='0.5' name='ecoHumMin' value='" + String(config.powerSave.ecoHumMin, 1) + "' min='20' max='85'></div>";
    html += "<div class='form-group' title='Вологість, при досягненні якої зволожувач вимкнеться в ЕКО-режимі'>";
    html += "<label>ЕКО макс. вологість (%) <span class='tip' tabindex='0' data-tip='Поріг вимкнення зволожувача при блекауті (має бути вищим за мінімальний, напр. 50%).'>?</span>:</label>";
    html += "<input type='number' step='0.5' name='ecoHumMax' value='" + String(config.powerSave.ecoHumMax, 1) + "' min='25' max='90'></div>";
    html += "</div>";
    html += "</div>";

    // Світло DMX та Фонові сервіси
    html += "<div class='section' style='border-left-color: #8b5cf6;'>";
    html += "<h3>💡 СВІТЛО DMX ТА ФОНОВІ СЕРВІСИ</h3>";
    html += "<div class='form-group' title='Поведінка DMX-світильника при відключенні світла'>";
    html += "<label>💡 Освітлення DMX при блекауті <span class='tip' tabindex='0' data-tip='Вимкнути — гасить світло в 0% протягом 5 секунд. Приглушити — зменшує загальну яскравість (master) на заданий відсоток від розкладу.'>?</span>:</label><select name='ecoLgtPol'>";
    html += "<option value='0'" + String(config.powerSave.lightPolicy == 0 ? " selected" : "") + ">⛔ Вимкнути повністю (0%)</option>";
    html += "<option value='1'" + String(config.powerSave.lightPolicy == 1 ? " selected" : "") + ">🌙 Приглушити до % від розкладу</option>";
    html += "<option value='2'" + String(config.powerSave.lightPolicy == 2 ? " selected" : "") + ">🔄 Без змін</option>";
    html += "</select></div>";
    html += "<div class='form-group' title='Відсоток від штатної яскравості розкладу в режимі приглушення'>";
    html += "<label>Масштаб яскравості DMX в ЕКО (%) <span class='tip' tabindex='0' data-tip='Наприклад, 25% означає, що світло світитиме в 4 рази слабше за поточний розклад (діє при політиці «Приглушити»).'>?</span>:</label>";
    html += "<input type='number' name='ecoLgtScl' value='" + String(config.powerSave.ecoLightScalePercent) + "' min='0' max='100'></div>";
    html += "<div class='form-group' title='Тимчасово зупиняє відправку в Google Sheets під час блекауту'>";
    html += "<label><input type='checkbox' name='ecoPauSh' " + String(config.powerSave.pauseSheetsSync ? "checked" : "") + "> ☁️ Призупинити автосинхронізацію Google Sheets при блекауті <span class='tip' tabindex='0' data-tip='Дані продовжують писатися локально на ESP32 (RAM/SPIFFS) і відправляться після повернення світла. Зменшує піки струму Wi-Fi та марні таймаути.'>?</span></label>";
    html += "</div>";
    html += "<div class='form-group' title='Не дозволяє адаптивному алгоритму змінювати базові пороги температури й вологості під час блекауту'>";
    html += "<label><input type='checkbox' name='ecoPauAdp' " + String(config.powerSave.pauseAdaptiveThresh ? "checked" : "") + "> 🛡️ Призупинити адаптивне зниження порогів (зберегти базовий конфіг) <span class='tip' tabindex='0' data-tip='Гарантує, що після повернення 220В система повернеться саме до ваших базових температур і вологості.'>?</span></label>";
    html += "</div>";
    html += "</div>";

    html += "</div>"; // tab12
    flushChunk();

    // Кнопки збереження
    html += "<div style='margin-top: 30px;'>";
    html += "<button type='submit' class='btn'>💾 ЗБЕРЕГТИ НАЛАШТУВАННЯ</button>";
    html += "</div>";

    html += "</form>";

    // JavaScript
    html += "<script>";
    // Функції перемикання категорій та вкладок (по id, з захистом від невалідного індексу)
    html += "function switchCat(cat) {";
    html += "  document.querySelectorAll('.cat-btn').forEach(b => b.classList.toggle('active', b.dataset.cat === cat));";
    html += "  document.querySelectorAll('.tabs').forEach(t => t.classList.toggle('active', t.id === 'cat-' + cat));";
    html += "  const firstTab = document.querySelector('#cat-' + cat + ' .tab');";
    html += "  if (firstTab) switchTab(parseInt(firstTab.dataset.tab));";
    html += "}";
    html += "function switchTab(tabId) {";
    html += "  const target = document.getElementById('tab' + tabId);";
    html += "  const btn = document.querySelector('.tab[data-tab=\"' + tabId + '\"]');";
    html += "  if (!target || !btn) { tabId = 0; }";
    html += "  const activeBtn = document.querySelector('.tab[data-tab=\"' + tabId + '\"]');";
    html += "  if (activeBtn) {";
    html += "    const cat = activeBtn.dataset.cat;";
    html += "    document.querySelectorAll('.cat-btn').forEach(b => b.classList.toggle('active', b.dataset.cat === cat));";
    html += "    document.querySelectorAll('.tabs').forEach(t => t.classList.toggle('active', t.id === 'cat-' + cat));";
    html += "  }";
    html += "  document.querySelectorAll('.tab').forEach(t => t.classList.toggle('active', parseInt(t.dataset.tab) === tabId));";
    html += "  document.querySelectorAll('.tab-content').forEach(c => c.classList.toggle('active', c.id === 'tab' + tabId));";
    html += "  localStorage.setItem('settingsTab', tabId);";
    html += "}";
    // Відновлення останньої вкладки при завантаженні
    html += "window.addEventListener('load', function() {";
    html += "  const savedTab = localStorage.getItem('settingsTab');";
    html += "  if (savedTab !== null && !isNaN(parseInt(savedTab))) switchTab(parseInt(savedTab));";
    html += "});";
    // Валідація форми
    html += "document.querySelector('form').addEventListener('submit', function(e) {";
    html += "  const tempMin = parseFloat(document.querySelector('[name=\"tempMin\"]').value);";
    html += "  const tempMax = parseFloat(document.querySelector('[name=\"tempMax\"]').value);";
    html += "  if (tempMin >= tempMax) {";
    html += "    alert('Помилка: Мінімальна температура має бути менше максимальної!');";
    html += "    e.preventDefault();";
    html += "    switchTab(0);";
    html += "    return;";
    html += "  }";
    html += "  const humMin = parseFloat(document.querySelector('[name=\"humMin\"]').value);";
    html += "  const humMax = parseFloat(document.querySelector('[name=\"humMax\"]').value);";
    html += "  if (humMin >= humMax) {";
    html += "    alert('Помилка: Мінімальна вологість має бути менше максимальної!');";
    html += "    e.preventDefault();";
    html += "    switchTab(0);";
    html += "    return;";
    html += "  }";
    html += "});";

    // Функції для серво
    html += "function sendServoCommand(cmd) {";
    html += "  return fetch('/servo/api', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
    html += "    body: 'cmd=' + cmd";
    html += "  }).then(r => r.text()).then(data => {";
    html += "    if(data.startsWith('ANGLE:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('servoCurrentAngle').innerText = angle + '°';";
    html += "    } else if(data.startsWith('CLOSED:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('servoClosedVal').innerText = angle + '°';";
    html += "      document.getElementById('manualServoClosed').value = angle;";
    html += "      alert('✅ Закрите положення збережено: ' + angle + '°');";
    html += "    } else if(data.startsWith('OPEN:')) {";
    html += "      let angle = data.split(':')[1];";
    html += "      document.getElementById('servoOpenVal').innerText = angle + '°';";
    html += "      document.getElementById('manualServoOpen').value = angle;";
    html += "      alert('✅ Відкрите положення збережено: ' + angle + '°');";
    html += "    } else if(data == 'TEST_OK') {";
    html += "      alert('✅ Тест серво завершено');";
    html += "    } else if(data.trim() === 'CALIB:ON' || data.trim() === 'CALIB:OFF') {";
    html += "      var isOn = (data.trim() === 'CALIB:ON');";
    html += "      var calibBtn = document.getElementById('servoCalibBtn');";
    html += "      if(calibBtn) calibBtn.innerHTML = isOn ? '🔓 ВИЙТИ З КАЛІБРУВАННЯ' : '🔒 УВІЙТИ В КАЛІБРУВАННЯ';";
    html += "      alert(isOn ? '✅ Режим калібрування УВІМКНЕНО' : '✅ Режим калібрування ВИМКНЕНО');";
    html += "    } else if(data.startsWith('MODE:')) {";
    html += "      location.reload();";
    html += "    }";
    html += "    return data;";
    html += "  });";
    html += "}";
    html += "function moveServo(delta) { sendServoCommand('move:' + delta); }";
    html += "function saveServoPosition(type) { sendServoCommand('save:' + type); }";
    html += "function gotoServoPosition(type) { sendServoCommand('goto:' + type); }";
    html += "function toggleServoCalibration() { sendServoCommand('calibration:toggle'); }";
    html += "function testServo() {";
    html += "  if(confirm('Тест відкриє і закриє заслонку. Продовжити?')) {";
    html += "    sendServoCommand('test');";
    html += "  }";
    html += "}";

    // Функції для WiFi налаштувань
    html += "function toggleStaticIP() {";
    html += "  const checked = document.getElementById('use_static_ip').checked;";
    html += "  document.getElementById('static_ip_fields').style.display = checked ? 'block' : 'none';";
    html += "}";
    html += "function toggleAuth() {";
    html += "  const checked = document.getElementById('use_auth').checked;";
    html += "  document.getElementById('auth_fields').style.display = checked ? 'block' : 'none';";
    html += "}";
    html += "function applyEcoPresetUI(id) {";
    html += "  const presets = {";
    html += "    0: { name:'🆘 Виживання (старий АКБ)', ecoHeatPol:1, ecoTempMin:19.0, ecoTempHyst:1.0, ecoPumpMax:35, ecoFanMax:25, ecoChkCar:true, ecoCarDlt:4.0, ecoHtrRel:false, ecoPmpPls:true, ecoPlsOn:90, ecoPlsOff:300, ecoExtPol:0, ecoPlfPol:0, ecoHumPol:0, ecoLgtPol:0, ecoPauSh:true, ecoPauAdp:true },";
    html += "    1: { name:'⚖️ Збалансований ЕКО', ecoHeatPol:1, ecoTempMin:21.0, ecoTempHyst:0.8, ecoPumpMax:45, ecoFanMax:35, ecoChkCar:true, ecoCarDlt:3.0, ecoHtrRel:false, ecoPmpPls:false, ecoExtPol:1, ecoExtOn:2, ecoExtOff:58, ecoExtPwr:25, ecoPlfPol:1, ecoPlfOn:5, ecoPlfOff:55, ecoPlfPwr:20, ecoHumPol:1, ecoHumMin:45.0, ecoHumMax:50.0, ecoLgtPol:1, ecoLgtScl:25, ecoPauSh:true, ecoPauAdp:true },";
    html += "    2: { name:'☀️ Теплий сезон (все OFF)', ecoHeatPol:0, ecoExtPol:0, ecoPlfPol:0, ecoHumPol:0, ecoLgtPol:0, ecoPauSh:true, ecoPauAdp:true }";
    html += "  };";
    html += "  const p = presets[id]; if (!p) return;";
    html += "  Object.keys(p).forEach(k => {";
    html += "    if (k === 'name') return;";
    html += "    const el = document.querySelector('[name=\"' + k + '\"]');";
    html += "    if (!el) return;";
    html += "    if (el.type === 'checkbox') el.checked = !!p[k];";
    html += "    else el.value = p[k];";
    html += "  });";
    html += "  const msg = document.getElementById('ecoPresetMsg');";
    html += "  if (msg) { msg.style.display = 'block'; msg.innerHTML = '✅ Підставлено пресет <strong>' + p.name + '</strong>. Перегляньте поля нижче, за потреби змініть та натисніть <strong>«💾 ЗБЕРЕГТИ НАЛАШТУВАННЯ»</strong> внизу.'; }";
    html += "}";
    html += "</script>";

    html += "</div>"; // container

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    flushChunk();
    server.sendContent("");
}
// ============================================================================
// ЗБЕРЕЖЕННЯ НАЛАШТУВАНЬ
// ============================================================================

void handleSaveSettings() {
    if (!checkAuth()) return;
    if (!checkCSRF()) {
        server.send(403, "text/plain", "❌ CSRF: Заборонено");
        return;
    }

    if (xSemaphoreTake(getConfigMutex(), pdMS_TO_TICKS(1000)) == pdTRUE) {
        
        // 1. ЧЕКБОКСИ (Галочки)
        config.pumpDisabled = server.hasArg("pumpDisabled");
        config.heaterFanRelayEnabled = server.hasArg("heaterFanRelay"); 
        config.coolingMode = server.hasArg("coolingMode");
        config.seasonalHeatingDisable = server.hasArg("seasonalDisable");
        config.autoStatusEnabled = server.hasArg("autoStatus");
        config.use24hFormat = server.hasArg("use24h");
        config.manualVentControl = server.hasArg("manualVent");
        config.useStaticIP = server.hasArg("use_static_ip");
        config.useAuth = server.hasArg("use_auth");
        config.extractorTimer.enabled = server.hasArg("extEnabled");
        config.plantFanTimer.enabled = server.hasArg("plfEnabled");
        config.plantFanTimer.breezeEnabled = server.hasArg("plfBreeze");

        // 2. ТЕМПЕРАТУРА ТА ВОЛОГІСТЬ
        if (server.hasArg("tempMin")) config.tempMin = server.arg("tempMin").toFloat();
        if (server.hasArg("tempMax")) config.tempMax = server.arg("tempMax").toFloat();
        if (server.hasArg("humMin"))  config.humidityConfig.minHumidity = server.arg("humMin").toFloat();
        if (server.hasArg("humMax"))  config.humidityConfig.maxHumidity = server.arg("humMax").toFloat();
        if (server.hasArg("humHyst")) config.humidityConfig.hysteresis = server.arg("humHyst").toInt();

        // 3. ОБМЕЖЕННЯ ПРИСТРОЇВ (ШІМ %)
        if (server.hasArg("pumpMin"))      config.pumpMinPercent = constrain(server.arg("pumpMin").toInt(), 0, 100);
        if (server.hasArg("pumpMax"))      config.pumpMaxPercent = constrain(server.arg("pumpMax").toInt(), 0, 100);
        if (server.hasArg("pumpStartPct")) config.pumpStartPercent = constrain(server.arg("pumpStartPct").toInt(), 20, 100);
        if (server.hasArg("pumpStartSec")) config.pumpStartDurationSec = constrain(server.arg("pumpStartSec").toInt(), 0, 15);
        if (server.hasArg("fanMin"))       config.fanMinPercent = constrain(server.arg("fanMin").toInt(), 0, 30);
        if (server.hasArg("fanMax"))       config.fanMaxPercent = constrain(server.arg("fanMax").toInt(), 0, 100);
        if (server.hasArg("extractorMin")) config.extractorMinPercent = constrain(server.arg("extractorMin").toInt(), 0, 100);
        if (server.hasArg("extractorMax")) config.extractorMaxPercent = constrain(server.arg("extractorMax").toInt(), 0, 100);

        // 4. ПОРОГИ ФОРСАЖУ ТА АВАРІЇ
        if (server.hasArg("tempCriticalLow"))  config.tempCriticalLow = server.arg("tempCriticalLow").toFloat();
        if (server.hasArg("tempEmergencyLow")) config.tempEmergencyLow = server.arg("tempEmergencyLow").toFloat();

        // 5. ДАТЧИКИ, АДАПТАЦІЯ ТА СИСТЕМА
        if (server.hasArg("scd30Interval"))   config.scd30MeasurementInterval = server.arg("scd30Interval").toInt();
        if (server.hasArg("statusPeriod"))    config.statusPeriod = server.arg("statusPeriod").toInt() * 1000;
        if (server.hasArg("adaptiveActTime")) config.adaptive_activation_time_min = constrain(server.arg("adaptiveActTime").toInt(), 5, 180);
        if (server.hasArg("adaptiveRecTime")) config.adaptive_recovery_time_min = constrain(server.arg("adaptiveRecTime").toInt(), 10, 360);
        if (server.hasArg("adaptiveTempStep")) config.adaptive_temp_step = constrain(server.arg("adaptiveTempStep").toInt(), 1, 5);
        if (server.hasArg("adaptiveHumStep"))  config.adaptive_hum_step = constrain(server.arg("adaptiveHumStep").toInt(), 1, 10);

        // 6. СЕРВОПРИВОД
        if (server.hasArg("servoClosed")) config.servoClosedAngle = server.arg("servoClosed").toInt();
        if (server.hasArg("servoOpen"))   config.servoOpenAngle = server.arg("servoOpen").toInt();
        if (server.hasArg("servoSpeed"))  config.servoSpeed = server.arg("servoSpeed").toInt();

        // 7. ТАЙМЕР ВИТЯЖКИ
        if (server.hasArg("extOnMin"))  config.extractorTimer.onMinutes = server.arg("extOnMin").toInt();
        if (server.hasArg("extOnSec"))  config.extractorTimer.onSeconds = server.arg("extOnSec").toInt();
        if (server.hasArg("extOffMin")) config.extractorTimer.offMinutes = server.arg("extOffMin").toInt();
        if (server.hasArg("extOffSec")) config.extractorTimer.offSeconds = server.arg("extOffSec").toInt();
        if (server.hasArg("extPower"))  config.extractorTimer.powerPercent = server.arg("extPower").toInt();

        // 8. ТАЙМЕР ТА ВІТЕР РОСЛИН
        if (server.hasArg("plfOnMin"))    config.plantFanTimer.onMinutes = server.arg("plfOnMin").toInt();
        if (server.hasArg("plfOnSec"))    config.plantFanTimer.onSeconds = server.arg("plfOnSec").toInt();
        if (server.hasArg("plfOffMin"))   config.plantFanTimer.offMinutes = server.arg("plfOffMin").toInt();
        if (server.hasArg("plfOffSec"))   config.plantFanTimer.offSeconds = server.arg("plfOffSec").toInt();
        if (server.hasArg("plfPower"))    config.plantFanTimer.powerPercent = server.arg("plfPower").toInt();
        if (server.hasArg("plfMinStart")) config.plantFanTimer.minStartPercent = server.arg("plfMinStart").toInt();

        if (server.hasArg("brzBaseMin")) config.breezeConfig.baseSpeedMin = server.arg("brzBaseMin").toInt();
        if (server.hasArg("brzBaseMax")) config.breezeConfig.baseSpeedMax = server.arg("brzBaseMax").toInt();
        if (server.hasArg("brzBoost"))   config.breezeConfig.gustBoost = server.arg("brzBoost").toInt();
        if (server.hasArg("brzGstMin"))  config.breezeConfig.gustMinSec = server.arg("brzGstMin").toInt();
        if (server.hasArg("brzGstMax"))  config.breezeConfig.gustMaxSec = server.arg("brzGstMax").toInt();
        if (server.hasArg("brzCalmMin")) config.breezeConfig.calmMinSec = server.arg("brzCalmMin").toInt();
        if (server.hasArg("brzCalmMax")) config.breezeConfig.calmMaxSec = server.arg("brzCalmMax").toInt();

        // 9. СИНХРОНІЗАЦІЯ ТА SPIFFS
        if (server.hasArg("spiffsLogInterval"))  config.spiffsLogInterval = server.arg("spiffsLogInterval").toInt();
        if (server.hasArg("sheetsSyncInterval")) config.sheetsSyncIntervalMin = server.arg("sheetsSyncInterval").toInt();
        if (server.hasArg("logTempThreshold"))   config.logTempThreshold = server.arg("logTempThreshold").toFloat();

        // 10. МЕРЕЖА ТА АВТЕНТИФІКАЦІЯ
        if (server.hasArg("static_ip")) config.staticIP = server.arg("static_ip");
        if (server.hasArg("gateway"))   config.gateway = server.arg("gateway");
        if (server.hasArg("subnet"))    config.subnet = server.arg("subnet");
        if (server.hasArg("dns"))       config.dns = server.arg("dns");

        if (server.hasArg("auth_user") && server.arg("auth_user").length() > 0) {
            config.authLogin = server.arg("auth_user");
        }
        if (server.hasArg("auth_pass") && server.arg("auth_pass").length() > 0) {
            config.authPassword = server.arg("auth_pass");
        }

        // 11. ПЛАВНА РОБОТА ВЕНТИЛЯТОРА ОБІГРІВУ
        config.fanSmooth.enabled              = server.hasArg("fsEn");
        config.fanSmooth.keepLowInsteadOfStop = server.hasArg("fsKeepLow");
        if (server.hasArg("fsHyst"))   config.fanSmooth.hysteresis    = constrain(server.arg("fsHyst").toFloat(), 0.1f, 3.0f);
        if (server.hasArg("fsStep"))   config.fanSmooth.maxStepPerSec = constrain(server.arg("fsStep").toInt(), 1, 50);
        if (server.hasArg("fsMinRun")) config.fanSmooth.minRunSec     = constrain(server.arg("fsMinRun").toInt(), 0, 600);
        if (server.hasArg("fsMinOff")) config.fanSmooth.minOffSec     = constrain(server.arg("fsMinOff").toInt(), 0, 600);

        // 11б. РОЗУМНЕ КЕРУВАННЯ НАСОСОМ
        config.smartPump.enabled = server.hasArg("spEn");
        if (server.hasArg("spWorkPct")) config.smartPump.workPercent     = constrain(server.arg("spWorkPct").toInt(), 10, 100);
        if (server.hasArg("spEvalSec")) config.smartPump.evalWindowSec   = constrain(server.arg("spEvalSec").toInt(), 30, 900);
        if (server.hasArg("spMinRise")) config.smartPump.minTempRise     = constrain(server.arg("spMinRise").toFloat(), 0.02f, 2.0f);
        if (server.hasArg("spCarStop")) config.smartPump.carrierStopTemp = constrain(server.arg("spCarStop").toFloat(), 0.0f, 70.0f);
        if (server.hasArg("spCarLow"))  config.smartPump.carrierLowTemp  = constrain(server.arg("spCarLow").toFloat(), 15.0f, 60.0f);
        if (server.hasArg("spMinRun"))  config.smartPump.minRunSec       = constrain(server.arg("spMinRun").toInt(), 10, 600);
        if (server.hasArg("spMinOff"))  config.smartPump.minOffSec       = constrain(server.arg("spMinOff").toInt(), 10, 900);

        // 12. ПРОФІЛЬ ЕНЕРГОЗБЕРЕЖЕННЯ / БЛЕКАУТ (ECO)
        if (server.hasArg("ecoActMode")) config.powerSave.activationMode = constrain(server.arg("ecoActMode").toInt(), 0, 2);
        if (server.hasArg("ecoOutSec"))  config.powerSave.outageConfirmSec  = constrain(server.arg("ecoOutSec").toInt(), 0, 120);
        if (server.hasArg("ecoResSec"))  config.powerSave.restoreConfirmSec = constrain(server.arg("ecoResSec").toInt(), 0, 300);

        if (server.hasArg("ecoHeatPol"))  config.powerSave.heatingPolicy     = constrain(server.arg("ecoHeatPol").toInt(), 0, 2);
        if (server.hasArg("ecoTempMin"))  config.powerSave.ecoTempMin        = constrain(server.arg("ecoTempMin").toFloat(), 10.0f, 35.0f);
        if (server.hasArg("ecoTempHyst")) config.powerSave.ecoTempHyst       = constrain(server.arg("ecoTempHyst").toFloat(), 0.2f, 5.0f);
        if (server.hasArg("ecoPumpMax"))  config.powerSave.ecoPumpMaxPercent = constrain(server.arg("ecoPumpMax").toInt(), 0, 100);
        if (server.hasArg("ecoFanMax"))   config.powerSave.ecoFanMaxPercent  = constrain(server.arg("ecoFanMax").toInt(), 0, 100);
        config.powerSave.ecoCheckCarrierTemp = server.hasArg("ecoChkCar");
        if (server.hasArg("ecoCarDlt"))   config.powerSave.ecoCarrierMinDelta = constrain(server.arg("ecoCarDlt").toFloat(), 0.5f, 20.0f);
        config.powerSave.ecoAllowHeaterRelay = server.hasArg("ecoHtrRel");
        config.powerSave.ecoPumpPulseMode    = server.hasArg("ecoPmpPls");
        if (server.hasArg("ecoPlsOn"))    config.powerSave.ecoPumpPulseOnSec  = constrain(server.arg("ecoPlsOn").toInt(), 10, 1800);
        if (server.hasArg("ecoPlsOff"))   config.powerSave.ecoPumpPulseOffSec = constrain(server.arg("ecoPlsOff").toInt(), 0, 3600);

        if (server.hasArg("ecoExtPol")) config.powerSave.extractorPolicy = constrain(server.arg("ecoExtPol").toInt(), 0, 2);
        if (server.hasArg("ecoExtOn"))  config.powerSave.ecoExtOnMin     = constrain(server.arg("ecoExtOn").toInt(), 0, 120);
        if (server.hasArg("ecoExtOff")) config.powerSave.ecoExtOffMin    = constrain(server.arg("ecoExtOff").toInt(), 0, 240);
        if (server.hasArg("ecoExtPwr")) config.powerSave.ecoExtPower     = constrain(server.arg("ecoExtPwr").toInt(), 5, 100);

        if (server.hasArg("ecoPlfPol")) config.powerSave.plantFanPolicy    = constrain(server.arg("ecoPlfPol").toInt(), 0, 2);
        if (server.hasArg("ecoPlfOn"))  config.powerSave.ecoPlantFanOnMin  = constrain(server.arg("ecoPlfOn").toInt(), 0, 120);
        if (server.hasArg("ecoPlfOff")) config.powerSave.ecoPlantFanOffMin = constrain(server.arg("ecoPlfOff").toInt(), 0, 240);
        if (server.hasArg("ecoPlfPwr")) config.powerSave.ecoPlantFanPower  = constrain(server.arg("ecoPlfPwr").toInt(), 5, 100);

        if (server.hasArg("ecoHumPol")) config.powerSave.humidifierPolicy     = constrain(server.arg("ecoHumPol").toInt(), 0, 2);
        if (server.hasArg("ecoHumMin")) config.powerSave.ecoHumMin            = constrain(server.arg("ecoHumMin").toFloat(), 20.0f, 85.0f);
        if (server.hasArg("ecoHumMax")) config.powerSave.ecoHumMax            = constrain(server.arg("ecoHumMax").toFloat(), 25.0f, 90.0f);
        if (config.powerSave.ecoHumMax <= config.powerSave.ecoHumMin) {
            config.powerSave.ecoHumMax = min(90.0f, config.powerSave.ecoHumMin + 3.0f);
        }
        if (server.hasArg("ecoLgtPol")) config.powerSave.lightPolicy          = constrain(server.arg("ecoLgtPol").toInt(), 0, 2);
        if (server.hasArg("ecoLgtScl")) config.powerSave.ecoLightScalePercent = constrain(server.arg("ecoLgtScl").toInt(), 0, 100);

        config.powerSave.pauseSheetsSync     = server.hasArg("ecoPauSh");
        config.powerSave.pauseAdaptiveThresh = server.hasArg("ecoPauAdp");

        // Збереження конфігурації в Flash
        saveConfiguration();

        // Миттєво оновлюємо стан реле 220В вентилятора (Pin 7) після збереження галочки
        bool allowRelay = config.heaterFanRelayEnabled &&
                          (!isEcoModeActive() || config.powerSave.ecoAllowHeaterRelay);
        if (heatingState.fanPower > 0 && allowRelay) {
            digitalWrite(HEATER_FAN_RELAY_PIN, LOW);
        } else {
            digitalWrite(HEATER_FAN_RELAY_PIN, HIGH);
        }

        xSemaphoreGive(getConfigMutex());
    }

    // Перенаправлення назад на сторінку налаштувань
    server.sendHeader("Location", "/settings");
    server.send(303);
}
