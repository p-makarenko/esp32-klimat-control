// ============================================================================
// WEB_PAGES_MISC.CPP - Допоміжні сторінки
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Допомога, відладка
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include "web_common.h"
#include "global_declarations.h"
#include "data_logger.h"

extern WebServer server;
extern SystemConfig config;
extern HeatingState heatingState;
extern PowerOutageState powerOutageState;
// Forward declarations
extern bool checkAuth();
extern String getUkraineMarquee();
extern const char* resetReasonToStr(int r);
extern bool isCrashResetReason(int r);
extern uint32_t getDiagBootCount();
extern uint32_t getDiagCrashCount();
extern int getDiagLastResetReason();
extern int getDiagLastCrashReason();
extern String getDiagLastCrashTime();

// ============================================================================
// СТОРІНКА ДОПОМОГИ (/help) — ПОВНОЦІННА ІНТЕРАКТИВНА ДОВІДКА СИСТЕМИ v5.0
// Потокова передача (chunked + PROGMEM) без великих алокацій у Heap ESP32
// ============================================================================

static const char HELP_CSS_AND_CONTROLS[] PROGMEM = R"rawliteral(
<style>
.hlp-hero{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px;margin:0 0 16px}
.hlp-stat{background:var(--s2);border:1px solid var(--b);border-left:3px solid var(--a);border-radius:10px;padding:11px 13px}
.hlp-stat .k{font-size:11.5px;color:var(--m);text-transform:uppercase;letter-spacing:.03em;font-weight:600}
.hlp-stat .v{font-size:15px;font-weight:700;color:var(--t);margin-top:3px;font-variant-numeric:tabular-nums;word-break:break-word}
.hlp-stat .sub{font-size:12px;color:var(--m);margin-top:2px}
.hlp-toolbar{position:sticky;top:52px;z-index:85;background:var(--s);border:1px solid var(--b);border-radius:12px;padding:10px 12px;margin:0 0 14px;box-shadow:var(--sh)}
.hlp-search-row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.hlp-search-wrap{position:relative;flex:1 1 240px;min-width:0}
.hlp-search{width:100%;padding:10px 12px 10px 36px;border:1px solid var(--b);border-radius:9px;background:var(--s2);color:var(--t);font-size:14.5px}
.hlp-search-ico{position:absolute;left:11px;top:50%;transform:translateY(-50%);pointer-events:none;font-size:15px;opacity:.75}
.hlp-toc{display:flex;gap:6px;overflow-x:auto;padding:8px 0 2px;scrollbar-width:thin;-webkit-overflow-scrolling:touch}
.hlp-chip{flex:0 0 auto;display:inline-flex;align-items:center;gap:4px;padding:6px 10px;border-radius:99px;background:var(--s2);border:1px solid var(--b);color:var(--m);text-decoration:none;font-size:12.5px;font-weight:600;white-space:nowrap;transition:.15s;cursor:pointer}
.hlp-chip:hover{border-color:var(--a);color:var(--t);background:var(--s)}
.hlp-count{font-size:12.5px;color:var(--m);margin-top:6px;display:none}
details.hlp-sec{background:var(--s2);border:1px solid var(--b);border-radius:12px;margin:0 0 14px;overflow:hidden;scroll-margin-top:130px}
details.hlp-sec[open]{border-color:rgba(56,189,248,.45)}
details.hlp-sec>summary{list-style:none;display:flex;align-items:center;justify-content:space-between;gap:10px;padding:15px 18px;cursor:pointer;font-size:1.05rem;font-weight:700;color:var(--t);user-select:none;background:var(--s2);transition:background .15s}
details.hlp-sec>summary::-webkit-details-marker{display:none}
details.hlp-sec>summary:hover{background:rgba(56,189,248,.07)}
details.hlp-sec>summary::after{content:'▾';font-size:16px;color:var(--a);transition:transform .2s;flex:0 0 auto}
details.hlp-sec[open]>summary::after{transform:rotate(180deg)}
details.hlp-sec[open]>summary{border-bottom:1px solid var(--b)}
.hlp-body{padding:16px 18px;font-size:14.2px;line-height:1.62;color:var(--t)}
.hlp-body h3{margin:18px 0 8px;font-size:1rem;color:var(--a);display:flex;align-items:center;gap:6px}
.hlp-body h3:first-child{margin-top:0}
.hlp-body p{margin:0 0 10px}
.hlp-body ul,.hlp-body ol{margin:6px 0 12px;padding-left:20px}
.hlp-body li{margin-bottom:5px}
.hlp-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(245px,1fr));gap:10px;margin:10px 0}
.hlp-card{background:var(--s);border:1px solid var(--b);border-radius:10px;padding:12px 14px}
.hlp-card h4{margin:0 0 6px;font-size:14.5px;color:var(--t);display:flex;align-items:center;justify-content:space-between;gap:6px}
.hlp-card p{margin:0;font-size:13.2px;color:var(--m);line-height:1.5}
.hlp-note,.hlp-warn,.hlp-err,.hlp-ok{padding:11px 14px;border-radius:9px;margin:10px 0;font-size:13.5px;line-height:1.52;border:1px solid var(--b);border-left-width:4px}
.hlp-note{background:rgba(56,189,248,.10);border-left-color:var(--a)}
.hlp-ok{background:rgba(16,185,129,.11);border-left-color:var(--ok)}
.hlp-warn{background:rgba(245,158,11,.12);border-left-color:var(--w)}
.hlp-err{background:rgba(239,68,68,.12);border-left-color:var(--e)}
.hlp-tbl-wrap{overflow-x:auto;margin:10px 0;border:1px solid var(--b);border-radius:10px;background:var(--s)}
.hlp-tbl{width:100%;border-collapse:collapse;font-size:13.3px;font-variant-numeric:tabular-nums}
.hlp-tbl th{background:var(--s2);color:var(--m);font-weight:700;padding:9px 11px;white-space:nowrap}
.hlp-tbl td{padding:8px 11px;border-top:1px solid var(--b);vertical-align:top}
.hlp-tbl tr:hover td{background:rgba(56,189,248,.04)}
.cmd-copy{cursor:pointer;display:inline-block;padding:2px 7px;border-radius:5px;background:var(--s2);color:var(--a);border:1px solid var(--b);font-family:ui-monospace,Consolas,monospace;font-size:12.8px;transition:.15s;white-space:nowrap}
.cmd-copy:hover{background:var(--ah);color:#fff;border-color:var(--a)}
.hlp-deep{display:inline-flex;align-items:center;gap:4px;padding:5px 10px;border-radius:7px;background:var(--s);border:1px solid var(--b);color:var(--a);text-decoration:none;font-size:12.5px;font-weight:600;margin:3px 4px 3px 0;transition:.15s}
.hlp-deep:hover{background:var(--ah);color:#fff;border-color:var(--a)}
.hlp-badge{display:inline-block;padding:2px 8px;border-radius:99px;font-size:11.5px;font-weight:700}
.b-ok{background:rgba(16,185,129,.18);color:var(--ok)}
.b-w{background:rgba(245,158,11,.18);color:var(--w)}
.b-e{background:rgba(239,68,68,.18);color:var(--e)}
.b-a{background:rgba(56,189,248,.18);color:var(--a)}
.hlp-faq{background:var(--s);border:1px solid var(--b);border-left:3px solid var(--w);border-radius:10px;padding:12px 14px;margin:10px 0}
.hlp-faq .q{font-weight:700;color:var(--t);margin-bottom:5px;font-size:14.3px}
.hlp-faq .a{color:var(--m);font-size:13.5px;line-height:1.55}
#hlpToast{position:fixed;bottom:18px;right:18px;z-index:999;background:var(--ok);color:#fff;padding:10px 16px;border-radius:10px;font-size:13.5px;font-weight:600;box-shadow:var(--sh);opacity:0;pointer-events:none;transform:translateY(10px);transition:.2s}
#hlpToast.show{opacity:1;transform:translateY(0)}
@media(max-width:768px){
  .hlp-toolbar{top:46px;padding:8px 10px}
  details.hlp-sec>summary{padding:13px 14px;font-size:.98rem}
  .hlp-body{padding:13px 14px;font-size:13.8px}
  .hlp-search{font-size:16px}
}
</style>

<div class="hlp-toolbar">
  <div class="hlp-search-row">
    <div class="hlp-search-wrap">
      <span class="hlp-search-ico">🔍</span>
      <input type="search" id="helpSearch" class="hlp-search" placeholder="Швидкий пошук по довідці (напр. насос, блекаут, Степан, TC421, Phyphox, GPIO, a50)..." oninput="filterHelp(this.value)">
    </div>
    <button type="button" class="btn btn-secondary" style="margin:0;padding:8px 12px;font-size:13px" onclick="toggleAllHelp(true)">📂 Розгорнути все</button>
    <button type="button" class="btn btn-secondary" style="margin:0;padding:8px 12px;font-size:13px" onclick="toggleAllHelp(false)">📁 Згорнути все</button>
  </div>
  <div class="hlp-toc">
    <a class="hlp-chip" href="#sec-map" onclick="openSec('sec-map')">🗺️ Сторінки</a>
    <a class="hlp-chip" href="#sec-modes" onclick="openSec('sec-modes')">🤖 Режими й Автоматика</a>
    <a class="hlp-chip" href="#sec-eco" onclick="openSec('sec-eco')">🔋 Блекаут (ЕКО)</a>
    <a class="hlp-chip" href="#sec-light" onclick="openSec('sec-light')">🔆 Світло TC421 / DMX</a>
    <a class="hlp-chip" href="#sec-energy" onclick="openSec('sec-energy')">⚡ Енергія та Аудит</a>
    <a class="hlp-chip" href="#sec-stepan" onclick="openSec('sec-stepan')">🎙️ Екран «Степан»</a>
    <a class="hlp-chip" href="#sec-history" onclick="openSec('sec-history')">📈 Графіки та Sheets</a>
    <a class="hlp-chip" href="#sec-cmds" onclick="openSec('sec-cmds')">💬 Консольні команди</a>
    <a class="hlp-chip" href="#sec-hw" onclick="openSec('sec-hw')">🔌 Датчики та GPIO</a>
    <a class="hlp-chip" href="#sec-net" onclick="openSec('sec-net')">📶 Wi-Fi, OTA, Backup</a>
    <a class="hlp-chip" href="#sec-faq" onclick="openSec('sec-faq')">🛠️ Несправності (FAQ)</a>
  </div>
  <div id="hlpCount" class="hlp-count"></div>
</div>
)rawliteral";

static const char HELP_SEC_1_2[] PROGMEM = R"rawliteral(
<!-- 1. ПУТІВНИК ПО СТОРІНКАХ -->
<details class="hlp-sec" id="sec-map" open>
  <summary><span>🗺️ 1. Путівник по системі та всіх сторінках інтерфейсу</span></summary>
  <div class="hlp-body">
    <p><strong>Клімат-Контроль v5.0</strong> — це автономна багатозадачна система на базі <strong>ESP32-S3 (FreeRTOS)</strong> для точного підтримання температури, вологості, повітрообміну, природного обдуву рослин, спектрального освітлення, енергомоніторингу та виживання при відключеннях 220В.</p>

    <div class="hlp-grid">
      <div class="hlp-card">
        <h4><span>🏠 Головна</span><a class="hlp-deep" href="/">Відкрити →</a></h4>
        <p>Живе зведення кожні 3 сек: температура кімнати й теплоносія, вологість, тиск, CO₂, потужність A/B/C, швидкий перемикач ЕКО та веб-консоль команд.</p>
      </div>
      <div class="hlp-card">
        <h4><span>🎛️ Керування</span><a class="hlp-deep" href="/control">Відкрити →</a></h4>
        <p>Швидке ручне управління повзунками Насоса (A), Вентилятора (B) та Витяжки (C), вибір режиму АВТО/РУЧНИЙ і блокування автоповернення.</p>
      </div>
      <div class="hlp-card">
        <h4><span>⚙️ Налаштування</span><a class="hlp-deep" href="/settings">Відкрити →</a></h4>
        <p>Усі робочі та системні параметри у 13 тематичних вкладках з інтерактивними підказками <code>?</code> біля кожного поля.</p>
      </div>
      <div class="hlp-card">
        <h4><span>🔆 Фітолампа TC421</span><a class="hlp-deep" href="/tc421">Відкрити →</a></h4>
        <p>Wi-Fi керування 4 каналами спектру (Red, White, Yellow, IR+UV), 4 профілі росту, Конструктор природного дня, світанок/захід і EOD Far-Red.</p>
      </div>
      <div class="hlp-card">
        <h4><span>💡 DMX Освітлення</span><a class="hlp-deep" href="/light">Відкрити →</a></h4>
        <p>Провідне керування світильниками по протоколу DMX512 (RS485): ручні повзунки R/G/B/Master та розклад по ключових точках доби.</p>
      </div>
      <div class="hlp-card">
        <h4><span>⚡ Енергоконтролер</span><a class="hlp-deep" href="/energy">Відкрити →</a></h4>
        <p>Показники лічильника PZEM-004T (В, А, Вт, кВт·год, Гц, cos φ), двозонний тариф День/Ніч (23:00–07:00) та історія споживання.</p>
      </div>
      <div class="hlp-card">
        <h4><span>🔬 Енерго- та світлоаудит</span><a class="hlp-deep" href="/audit">Відкрити →</a></h4>
        <p>Автономне зняття кривих Люкс/Ват через смартфон (Phyphox) і чистого споживання кожного пристрою без підключення ноутбука.</p>
      </div>
      <div class="hlp-card">
        <h4><span>📈 Історія та Графіки</span><a class="hlp-deep" href="/history">Відкрити →</a></h4>
        <p>Інтерактивні графіки за день/тиждень/місяць/рік із трьох джерел (RAM, архів SPIFFS, Google Sheets), масштабування та експорт у CSV/JSON.</p>
      </div>
      <div class="hlp-card">
        <h4><span>🎙️ Екран «Степан»</span><a class="hlp-deep" href="/tablet">Відкрити →</a></h4>
        <p>Повноекранний інтерфейс для Android-планшета з чорним режимом сну, активацією голосом «Степан» та озвученням українською мовою.</p>
      </div>
      <div class="hlp-card">
        <h4><span>📶 Wi-Fi / 🔄 OTA / 🔧 Відладка</span><a class="hlp-deep" href="/ota">OTA →</a></h4>
        <p>Сканування й пріоритет мереж (<a href="/wifi">/wifi</a>), прошивка по повітрю та бекапи конфігурації (<a href="/ota">/ota</a>), стан RAM і задач FreeRTOS (<a href="/debug">/debug</a>).</p>
      </div>
    </div>

    <h3>⚡ Швидкий перехід одразу у потрібну вкладку Налаштувань:</h3>
    <div>
      <a class="hlp-deep" href="/settings" onclick="goTab(0)">🌡️ Клімат</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(1)">🔧 Пристрої (Насос / Вентилятор)</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(2)">🌀 Таймер витяжки</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(11)">💨 Вітер рослин</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(3)">🔄 Сезон / Охолодження</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(4)">🚨 Пороги форсажу й аварії</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(12)">🔋 Економія (Блекаут)</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(5)">💨 Датчик CO₂</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(7)">🎯 Калібрування серво</a>
      <a class="hlp-deep" href="/settings" onclick="goTab(9)">📊 Логи та Google Sheets</a>
    </div>
  </div>
</details>

<!-- 2. РЕЖИМИ ТА АВТОМАТИКА -->
<details class="hlp-sec" id="sec-modes">
  <summary><span>🤖 2. Режими роботи, Розумний насос, Плавний вентилятор і Вітер</span></summary>
  <div class="hlp-body">
    <h3>🎛️ Основні режими системи</h3>
    <ul>
      <li><span class="hlp-badge b-ok">🤖 АВТО</span> — штатний інтелектуальний режим. Система сама підтримує цільовий діапазон температури й вологості, керує насосом, вентилятором обігріву, зволожувачем, заслонкою та таймерами витяжки й обдуву рослин.</li>
      <li><span class="hlp-badge b-w">✋ РУЧНИЙ</span> — активується автоматично, коли ви рухаєте повзунки на сторінці <a href="/control">Керування</a> або вводите команду потужності (наприклад, <code class="cmd-copy" onclick="copyCmd('a40')">a40</code>). <strong>Захист від забудькуватості:</strong> через <strong>15 хвилин</strong> система сама повернеться в <code>АВТО</code>. Якщо потрібно залишити ручний режим надовго — увімкніть прапорець <strong>«🔒 Блокувати ручний режим»</strong> (у заблокованому ручному режимі автоматичний Форсаж та Аварія також вимкнені).</li>
      <li><span class="hlp-badge b-w">⚡ ФОРСАЖ</span> — вмикається автоматично, якщо температура кімнати впала до критичного порогу (типово <code>≤ 20.0°C</code>): насос та вентилятор виходять на <strong>80%</strong> для швидкого прогріву.</li>
      <li><span class="hlp-badge b-e">🚨 АВАРІЯ</span> — вмикається при падінні температури до аварійного порогу (типово <code>≤ 18.0°C</code>): насос і вентилятор працюють на <strong>100%</strong> до стабілізації.</li>
    </ul>

    <h3>💧 Розумний насос (SmartPump) — як система уникає «синусоїди» та економить ресурс</h3>
    <p>Налаштовується у <a href="/settings" onclick="goTab(1)">Налаштування → Пристрої</a>:</p>
    <ul>
      <li><strong>🚀 Пусковий імпульс (Kickstart):</strong> при старті насоса з <code>0%</code> на низькі робочі оберти (наприклад, 33%) система спочатку подає підвищену пускову потужність (типово <strong>45% на 2 сек</strong>), щоб ротор гарантовано зрушив з місця, а потім плавно опускає до робочої.</li>
      <li><strong>🧠 Оцінка за швидкістю нагріву кімнати:</strong> коли вмикається обігрів, спочатку продувається наявне тепло радіатора лише вентилятором. Система чекає <em>«Вікно оцінки»</em> (типово 180 сек) і дивиться на приріст температури кімнати. Якщо кімната вже нагрівається на <code>≥ 0.15°C</code> за вікно — насос <strong>не вмикається даремно</strong>. Якщо приріст менший (або кімната втратила <code>≥ 0.25°C</code> — тоді достроково через 30 с), вмикається підкачка насосом.</li>
      <li><strong>🛑 Стоп по теплоносію:</strong> щойно свіжа гаряча вода дійшла до датчика теплоносія (наприклад, <code>36°C</code>) і минув мінімальний час роботи (<code>60 с</code>), насос зупиняється, даючи вентилятору плавно зняти тепло з радіатора без перельоту температури вище норми.</li>
      <li><strong>⛔ Повне блокування насоса:</strong> галочка <em>«Повністю вимкнути насос»</em> жорстко тримає <code>0%</code> у будь-якому режимі (для літа або сухого обслуговування контуру).</li>
    </ul>

    <h3>🌊 Плавний вентилятор обігріву (FanSmooth) та Реле 220В</h3>
    <ul>
      <li>Керується безшумним ШІМ 25 кГц (GPIO 38) разом із силовим <strong>Реле 220В (GPIO 7)</strong>, яке повністю знеструмлює мотор при <code>0%</code> (реле можна програмно заблокувати в налаштуваннях).</li>
      <li><strong>Плавний хід:</strong> обмежує швидкість розгону й гальмування (типово <code>5 %/сек</code>), виключаючи акустичні удари та пускові просідання.</li>
      <li><strong>Антициклювання біля верхньої межі:</strong> опція <em>«Тримати низькі оберти замість стопу»</em> дозволяє в зоні від <code>T_макс</code> до <code>T_макс + гістерезис</code> підтримувати мінімальний тихий продув замість постійних стартів/зупинок.</li>
    </ul>

    <h3>💨 Обдув рослин «Природній вітер» (Breeze) та 🌀 Таймер витяжки</h3>
    <ul>
      <li><strong>Вітер рослин (GPIO 11):</strong> працює за власним циклічним таймером (хвилини + секунди роботи і паузи). Якщо увімкнено <em>«Природній вітер»</em>, під час активної фази базова швидкість плавно «дихає» між <code>baseMin</code> і <code>baseMax</code>, а через випадкові паузи затишшя виникають м'які пориви вітру (<code>+gustBoost%</code>), що зміцнює стебла та запобігає застою вологи біля листя. Параметр <em>«Мінімальна швидкість запуску»</em> враховує поріг старту конкретного кулера.</li>
      <li><strong>Витяжка (GPIO 12) і Заслонка Серво (GPIO 14):</strong> витяжка працює за розкладом (наприклад, 30 хв ВКЛ / 30 хв ВИМК) або за перевищенням температури вентиляції. Сервопривод плавно відкриває/закриває повітряну заслонку і <strong>автоматично відключає ШІМ (detach) через 1 секунду після завершення руху</strong>, щоб серво не гудів і не грівся. Підтримується механічний тумблер примусового відкриття (GPIO 13).</li>
    </ul>

    <h3>💧 Зволожувач, Адаптивні пороги та Сезонні режими</h3>
    <ul>
      <li><strong>Зволожувач (GPIO 6):</strong> вмикається нижче <code>humMin</code> і вимикається на <code>humMax</code> із захистом від перезволоження: пауза між циклами не менше <strong>30 сек</strong>, безперервна робота не більше <strong>10 хвилин</strong>.</li>
      <li><strong>Адаптація порогів:</strong> якщо у сильний мороз система довго (<code>adaptiveActTime</code> хв) не може дотягнути до заданої температури/вологості, вона тимчасово покроково знижує ціль, а при потеплінні автоматично відновлює ваші базові налаштування.</li>
      <li><strong>Сезонність (<a href="/settings" onclick="goTab(3)">Сезон</a>):</strong> можна увімкнути авто-відключення обігріву на теплі місяці (травень–вересень) або літній <strong>Режим охолодження</strong> (коли в трубах циркулює холодна вода — вентилятор вмикається при перевищенні температури).</li>
    </ul>
  </div>
</details>
)rawliteral";

static const char HELP_SEC_3_4[] PROGMEM = R"rawliteral(
<!-- 3. ЕКО-РЕЖИМ ТА БЛЕКАУТ -->
<details class="hlp-sec" id="sec-eco">
  <summary><span>🔋 3. Енергозбереження та виживання при блекауті (ЕКО / АКБ / ДБЖ)</span></summary>
  <div class="hlp-body">
    <p>Коли система живиться від інвертора/ДБЖ, вхідний лічильник <strong>PZEM-004T</strong> підключається до міської мережі 220В до ДБЖ. Щойно напруга виходить за межі <strong>175–255 В</strong> (зникає світло), контролер автоматично переходить у <strong>профіль ЕКО</strong>, розтягуючи заряд акумулятора в рази без зміни ваших основних налаштувань.</p>

    <div class="hlp-note">
      <strong>Захист від брязкоту мережі:</strong> система чекає <em>«Підтвердження блекауту»</em> (типово <strong>6 сек</strong>) перед входом в ЕКО при миганні світла, а після повернення 220В витримує паузу <em>«Стабілізація після 220В»</em> (типово <strong>15 сек</strong>) перед плавним поверненням повного навантаження.
    </div>

    <h3>⚙️ 3 способи активації та 3 готові пресети (<a href="/settings" onclick="goTab(12)">Налаштування → Економія</a>)</h3>
    <ul>
      <li><strong>Активація:</strong> <code>АВТО (по 220В PZEM)</code> — рекомендовано на кожен день; <code>ПРИМУСОВО ЕКО</code> — можна увімкнути в 1 клік кнопкою <em>«🔋 Увімкнути ЕКО-режим вручну»</em> на Головній сторінці або командою <code class="cmd-copy" onclick="copyCmd('eco on')">eco on</code>; <code>ВИМКНЕНО</code> — ігнорувати відключення 220В.</li>
      <li><strong>🆘 Пресет «Виживання (старий АКБ)»:</strong> витяжка, вітер, зволожувач і DMX вимкнені (0%); обігрів тримає лише мінімум виживання <code>19°C</code> на тихих обертах (насос ≤35% в імпульсному режимі 90с ВКЛ / 300с пауза, вентилятор ≤25%).</li>
      <li><strong>⚖️ Пресет «Збалансований ЕКО»:</strong> обігрів <code>21°C</code>, провітрювання витяжкою 2 хв щогодини на 25%, обдув рослин 5 хв щогодини на 20% без поривів, зволожувач 45–50%, світло 25%.</li>
      <li><strong>☀️ Пресет «Теплий сезон (все OFF)»:</strong> усе силове обладнання 0%, працюють лише ESP32, датчики та запис історії.</li>
    </ul>

    <h3>🛡️ Розумні захисти акумулятора під час блекауту</h3>
    <ul>
      <li><strong>Перевірка гарячого теплоносія:</strong> якщо котел без світла також остиг і різниця <code>T_теплоносія − T_кімнати</code> менша за поріг (типово <code>3–4°C</code>), насос і вентилятор <strong>не крутять вхолосту</strong>, зберігаючи заряд АКБ.</li>
      <li><strong>Вимкнення котушки реле 220В (Pin 7)</strong> та <strong>імпульсна прокачка насоса</strong> (короткі цикли подачі води замість постійного обертання).</li>
      <li><strong>Автоматичний ліміт фітолампи TC421:</strong> якщо в профілі TC421 задано <em>«Макс. при ЕКО %»</em> (наприклад, 15% або 0%), лампа миттєво знижує потужність усіх каналів.</li>
      <li><strong>Пауза Google Sheets без втрати даних:</strong> під час блекауту Wi-Fi не витрачає енергію на важкі SSL-сесії в інтернет — усі точки пишуться в локальну пам'ять ESP32 (RAM + SPIFFS) і автоматично довантажуються в таблицю після появи світла.</li>
    </ul>
  </div>
</details>

<!-- 4. СВІТЛО TC421 ТА DMX512 -->
<details class="hlp-sec" id="sec-light">
  <summary><span>🔆 4. Фітолампа TC421 (Wi-Fi) та Освітлення DMX512</span></summary>
  <div class="hlp-body">
    <h3>🔆 Wi-Fi Фітолампа TC421 (<a href="/tc421">/tc421</a>)</h3>
    <p>ESP32-S3 постійно піднімає власну ізольовану точку доступу <strong><code>KlimatLight</code></strong> (пароль за замовчуванням: <code>12345678</code>) паралельно з основним домашнім Wi-Fi (режим <code>AP+STA</code>). Контролер TC421 підключається напряму до ESP32, тому світло працює на 100% автономно навіть без роутера й інтернету!</p>

    <ul>
      <li><strong>Підключення та пошук:</strong> натисніть <strong>«🔍 Знайти»</strong> на сторінці <a href="/tc421">/tc421</a> — ESP32 знайде контролер по UDP (порт 5000) і збереже його IP та MAC у NVS. Кнопка <strong>«🕒 Синхр. час»</strong> вирівнює внутрішній годинник TC421 за NTP-часом ESP32.</li>
      <li><strong>Розподіл 4 активних каналів:</strong> <code>🔴 CH1 Червоний (660 нм)</code> — фотосинтез і цвітіння; <code>⚪ CH2 Білий</code> — базова маса росту; <code>🟡 CH3 Жовтий (теплий)</code> — розкриття листя; <code>🟣 CH4 IR + UV</code> — фітохромна регуляція, ефірні олії та сигнал сну (CH5 завжди 0%).</li>
      <li><strong>3 режими роботи лампи:</strong>
        <ol>
          <li><strong>Автопілот ESP32 (рекомендовано):</strong> щохвилини плавно розраховує яскравість кожного каналу за природною кривою та миттєво реагує на блекаут (ЕКО).</li>
          <li><strong>Ручний режим (Live-повзунки):</strong> миттєва зміна спектру без перезавантаження сторінки з одночасним відображенням реальних Ватів із PZEM-004T.</li>
          <li><strong>Внутрішній розклад TC421:</strong> передає керування пам'яті самого приладу TC421.</li>
        </ol>
      </li>
      <li><strong>Конструктор природного дня та 4 профілі:</strong> підтримуються 4 незалежні слоти (<em>Вегетація 18/6</em>, <em>Цвітіння 12/12</em>, <em>Природний 14/10</em>, <em>Мій профіль</em>). Налаштовуються: час і тривалість плавного світанку/заходу (5–120 хв), денна стеля кожного каналу, <strong>ранковий імпульс CH4</strong> на світанку, <strong>полуденне вікно УФ</strong> у зеніті, фоновий денний рівень CH4 та <strong>вечірній EOD Far-Red («сигнал сну»)</strong> після заходу сонця для швидкого переведення рослин у нічну фазу. Живий SVG-графік одразу малює добову криву всіх 4 каналів.</li>
    </ul>

    <h3>💡 Провідне освітлення DMX512 (<a href="/light">/light</a>)</h3>
    <ul>
      <li>Працює через трансивер <strong>MAX485</strong> на апаратному <code>UART1</code> (250 кбіт/с, 8N2, стартова адреса <code>1</code>): <strong>CH1 Червоний</strong>, <strong>CH2 Зелений</strong>, <strong>CH3 Синій</strong>, <strong>CH4 Master (загальна яскравість 0–255)</strong>.</li>
      <li>У таблиці ключових точок задаються часові мітки доби — планувальник автоматично сортує їх за часом та плавно інтерполює колір і яскравість між точками.</li>
    </ul>
  </div>
</details>
)rawliteral";

static const char HELP_SEC_5_6_7[] PROGMEM = R"rawliteral(
<!-- 5. ЕНЕРГІЯ ТА АУДИТ -->
<details class="hlp-sec" id="sec-energy">
  <summary><span>⚡ 5. Енергоконтролер (День/Ніч) та Автономний світло- і енергоаудит</span></summary>
  <div class="hlp-body">
    <h3>⚡ Енергоконтролер PZEM-004T (<a href="/energy">/energy</a>)</h3>
    <ul>
      <li>Опитує цифровий ватметр <strong>PZEM-004T v3.0</strong> по протоколу Modbus RTU кожні 2 секунди: напруга (В), струм (А), активна потужність (Вт), накопичена енергія (кВт·год), частота (Гц) та коефіцієнт потужності (PF / cos φ).</li>
      <li><strong>Двозонний тарифний облік:</strong> система автоматично розділяє спожиті кВт·год на <strong>☀️ Денний тариф (07:00–23:00)</strong> та <strong>🌙 Нічний тариф (23:00–07:00)</strong> і надійно зберігає лічильники в енергонезалежній пам'яті NVS.</li>
    </ul>

    <h3>🔬 Як провести енерго- та світловий аудит зі смартфона без ПК (<a href="/audit">/audit</a>)</h3>
    <p>Вбудований інструмент дозволяє виміряти реальну світловіддачу кожного каналу фітолампи (<strong>Люкс на 1 Ват чистої потужності</strong>) та точне енергоспоживання насоса, вентиляторів і зволожувача на всіх обертах:</p>
    <ol>
      <li>Встановіть на смартфон безкоштовний науковий додаток <strong>Phyphox</strong>.</li>
      <li>У Phyphox відкрийте датчик <strong>«Освітленість» (Light)</strong> → натисніть меню <code>⋮</code> у правому верхньому куті → увімкніть <strong>«Віддалений доступ» (Allow remote access)</strong> → знизу екрана з'явиться адреса виду <code>http://192.168.x.x:8080</code> → натисніть кнопку <strong>▶ Play</strong>.</li>
      <li>Відкрийте сторінку <a href="/audit">/audit</a>, впишіть IP:порт із Phyphox у поле <em>«Адреса Phyphox»</em> і натисніть <strong>«🔌 Тест»</strong> — статус має показати зелене <code>✅ OK: ... lx</code>.</li>
      <li>Покладіть смартфон екраном (датчиком світла) догори в центрі боксу на рівні верхівок рослин, вкажіть відстань до лампи (см), відмітьте потрібні канали/пристрої, залиште увімкненою галочку <strong>«🔇 Вимикати кліматику на час тесту»</strong> (для чистого заміру холостого фону ESP32 + блоків живлення) і натисніть <strong>«▶ Запустити аудит»</strong>.</li>
      <li>Скрипт автоматично заміряє базовий фон (Idle Вт), прожене кожен канал/пристрій по точках потужності з медіанною фільтрацією, побудує графік <strong>Люкс/Вт</strong>, виділить зеленим <strong>ефективний діапазон ККД</strong> і збереже профіль рекомендацій!</li>
    </ol>
    <div class="hlp-ok">
      <strong>✨ Автопідстановка в 1 клік:</strong> після проходження аудиту відкрийте <a href="/settings" onclick="goTab(1)">Налаштування → Пристрої</a> — зверху з'явиться плашка з вашими реальними Ватами та кнопкою <strong>«✨ Підставити в Макс.»</strong> для насоса й витяжки! Результати також можна вивантажити у файл <strong>CSV</strong>.
    </div>
  </div>
</details>

<!-- 6. ГОЛОСОВИЙ ЕКРАН СТЕПАН -->
<details class="hlp-sec" id="sec-stepan">
  <summary><span>🎙️ 6. Голосовий екран «Степан» для планшета (/tablet)</span></summary>
  <div class="hlp-body">
    <p>Сторінка <a href="/tablet">/tablet</a> створена спеціально для настінного або настільного Android-планшета (чи старого смартфона з Bluetooth-колонкою), перетворюючи його на живий голосовий дисплей клімат-контролю.</p>

    <h3>🌟 Основні фішки «Степана»</h3>
    <ul>
      <li><strong>Чорний режим енергозбереження + Wake Lock:</strong> планшет не блокується системним паролем, а через заданий час (20–180 с) переходить у повністю чорний екран із ледь помітним годинником (береже матрицю і не світить уночі).</li>
      <li><strong>Голосове пробудження та синтез мовлення:</strong> скажіть кодове слово <strong>«Степан»</strong> (також розуміє <em>«Степане», «Стьопа»</em>) або торкніться екрана — він прокинеться, покаже температуру й вологість гігантським шрифтом і відповість голосом українською мовою.</li>
      <li><strong>🔇 Авто-тиша при діалозі:</strong> якщо в налаштуваннях «Степана» (кнопка <code>⚙️</code> зверху) увімкнути <em>«Авто-тиша»</em>, при зверненні до нього ESP32 на 20 секунд притишує вентилятори через <code>/api/quiet</code>, щоб шум повітря не заважав мікрофону розпізнати команду!</li>
    </ul>

    <div class="hlp-warn">
      <strong>⚠️ Одноразове налаштування мікрофона в Chrome на Android (для локального HTTP):</strong><br>
      Браузер Chrome за замовчуванням блокує мікрофон на сайтах без <code>https://</code>. Щоб дозволити мікрофон для вашого контролера:<br>
      1️⃣ Скопіюйте в адресний рядок Chrome на планшеті: <code class="cmd-copy" onclick="copyCmd('chrome://flags/#unsafely-treat-insecure-origin-as-secure')">chrome://flags/#unsafely-treat-insecure-origin-as-secure</code><br>
      2️⃣ Перемкніть прапорець у стан <strong>Enabled</strong> та впишіть адресу вашого контролера (наприклад, <code id="stepanOriginCmd" class="cmd-copy" onclick="copyCmd(this.textContent)">http://192.168.1.100</code>).<br>
      3️⃣ Натисніть <strong>Relaunch</strong> і один раз дайте дозвіл на мікрофон при натисканні <em>«Запустити Степана»</em>.
    </div>

    <h3>🗣️ Приклади голосових команд (починайте зі слова «Степан»):</h3>
    <div class="hlp-tbl-wrap">
      <table class="hlp-tbl">
        <thead><tr><th>Категорія</th><th>Що сказати</th><th>Дія системи</th></tr></thead>
        <tbody>
          <tr><td><strong>Екран</strong></td><td>«Степан, покажи CO2» / «покажи тиск» / «покажи теплоносій» / «покажи енергію» / «покажи все» / «головний екран»</td><td>Перебудовує великі картки на екрані та озвучує вибрані показники</td></tr>
          <tr><td><strong>Запитання</strong></td><td>«Яка температура?» / «Скажи вологість» / «Скільки CO2?» / «Звіт» / «Стан системи»</td><td>Проговорює голосом актуальні дані всіх датчиків і режим роботи</td></tr>
          <tr><td><strong>Режими й ЕКО</strong></td><td>«Режим авто» / «Ручний режим» / «Форсаж» / «Еко увімкни» / «Еко авто» / «Еко вимкни»</td><td>Перемикає режим клімату або профіль енергозбереження</td></tr>
          <tr><td><strong>Пристрої</strong></td><td>«Насос 50» / «Вентилятор 35» / «Витяжка 60» / «Вимкни витяжку» / «Встанови температуру 24» / «Тиша»</td><td>Керує обертами, змінює цільову температуру або тимчасово приглушує вентилятори</td></tr>
          <tr><td><strong>Сон</strong></td><td>«Спати» / «Темний екран»</td><td>Миттєво переводить дисплей планшета в чорний режим очікування</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</details>

<!-- 7. ГРАФІКИ ТА GOOGLE SHEETS -->
<details class="hlp-sec" id="sec-history">
  <summary><span>📈 7. Графіки, Логування в пам'ять та Google Sheets (/history)</span></summary>
  <div class="hlp-body">
    <h3>🗄️ Трирівнева архітектура збереження історії</h3>
    <ol>
      <li><strong>📊 RAM-буфер (Оперативна пам'ять — 360 записів = 6 годин):</strong> точка записується щохвилини. Працює миттєво без зносу флеш-пам'яті, забезпечує детальний живий графік останніх годин та залишає великий запас вільної RAM для стабільного HTTPS/SSL.</li>
      <li><strong>💾 SPIFFS (Внутрішній флеш-архів пристрою):</strong> агрегує та записує дані на флеш із налаштовуваним інтервалом (<strong>1–60 хв</strong>, типово 5 хв). Не стирається при перезавантаженні чи вимкненні живлення.</li>
      <li><strong>☁️ Google Sheets (Хмарний безлімітний архів):</strong> відправляє пакети даних у вашу Google Таблицю через Google Apps Script. Підтримує перегляд графіків за <strong>Тиждень, Місяць, Рік або довільний календарний період</strong>.</li>
    </ol>

    <h3>⚖️ Як працює розумна економія трафіку (<a href="/settings" onclick="goTab(9)">Налаштування → Sync</a>)</h3>
    <ul>
      <li>Запис у чергу синхронізації створюється, якщо температура кімнати змінилася на <strong>Поріг запису</strong> (типово <code>0.5°C</code>), <strong>АБО</strong> гарантовано кожні <strong>N хвилин</strong> (<em>Регулярний інтервал відправки</em>, типово <code>15 хв</code>), навіть коли температура ідеально рівна.</li>
      <li>При тимчасовій відсутності інтернету записи не губляться — вони накопичуються і відправляються наступним пакетом (або вручну кнопкою <strong>«📤 SYNC»</strong> на Головній).</li>
      <li><strong>Масштабування графіків:</strong> крутіть коліщатко миші (або розводьте два пальці на смартфоні) для зуму, перетягуйте графік убік, а для скидання масштабу двічі клікніть по полотну або натисніть зелену кнопку <em>«Скинути зум»</em>. Доступне вивантаження історії у <strong>CSV</strong> та <strong>JSON</strong>.</li>
    </ul>
  </div>
</details>
)rawliteral";

static const char HELP_SEC_8[] PROGMEM = R"rawliteral(
<!-- 8. ДОВІДНИК КОНСОЛЬНИХ КОМАНД -->
<details class="hlp-sec" id="sec-cmds">
  <summary><span>💬 8. Повний довідник консольних команд (Веб-консоль та USB Serial)</span></summary>
  <div class="hlp-body">
    <p>Усі наведені нижче команди працюють у полі <strong>«💬 Командна строка»</strong> на <a href="/">Головній сторінці</a>, а також у <strong>USB Serial Monitor</strong> (швидкість порту <code>115200</code> бод, UTF-8). <em>Натисніть на будь-яку команду в таблиці, щоб миттєво скопіювати її в буфер обміну!</em></p>

    <div class="hlp-tbl-wrap">
      <table class="hlp-tbl">
        <thead><tr><th>Група</th><th>Команда (клік = копія)</th><th>Опис дії та параметри</th></tr></thead>
        <tbody>
          <tr><td rowspan="5"><strong>🎛️ Режими роботи</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('auto')">auto</code></td>
              <td>Увімкнути штатний <strong>АВТОМАТИЧНИЙ</strong> режим, зняти блокування ручного режиму та скинути прапорці аварії.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('manual')">manual</code></td>
              <td>Перейти в <strong>РУЧНИЙ</strong> режим (автоповернення в АВТО через 15 хв, якщо не увімкнено <code>lock</code>).</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('lock')">lock</code></td>
              <td>Увімкнути / вимкнути <strong>блокування ручного режиму</strong> (без 15-хвилинного автоповернення).</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('force')">force</code> / <code class="cmd-copy" onclick="copyCmd('emergency')">emergency</code></td>
              <td>Примусово увімкнути режим <strong>ФОРСАЖ</strong> (80%) або <strong>АВАРІЯ</strong> (100%).</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('reset')">reset</code></td>
              <td>Скинути аварійний та примусовий ЕКО-режим і повернути систему до <code>АВТО</code>.</td></tr>

          <tr><td rowspan="3"><strong>🔋 Блекаут / ЕКО</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('eco on')">eco on</code> / <code class="cmd-copy" onclick="copyCmd('eco auto')">eco auto</code> / <code class="cmd-copy" onclick="copyCmd('eco off')">eco off</code></td>
              <td>Примусово увімкнути ЕКО-режим / перевести в АВТО по напрузі 220В PZEM / повністю вимкнути реакцію на блекаут.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('eco status')">eco status</code></td>
              <td>Вивести детальний звіт про стан 220В, поточну напругу та активні політики ЕКО для всіх пристроїв.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('eco preset 0')">eco preset 0</code> | <code class="cmd-copy" onclick="copyCmd('eco preset 1')">1</code> | <code class="cmd-copy" onclick="copyCmd('eco preset 2')">2</code></td>
              <td>Миттєво застосувати та зберегти пресет: <code>0</code> — Виживання, <code>1</code> — Збалансований ЕКО, <code>2</code> — Теплий сезон.</td></tr>

          <tr><td rowspan="3"><strong>⚡ Пристрої (ШІМ)</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('a50')">a50</code> / <code class="cmd-copy" onclick="copyCmd('b40')">b40</code> / <code class="cmd-copy" onclick="copyCmd('c30')">c30</code></td>
              <td>Швидкі однолітерні команди (0–100%): <code>a</code> = Насос, <code>b</code> = Вентилятор обігріву, <code>c</code> = Витяжка (переводить у ручний режим).</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('pump 35')">pump 35</code> / <code class="cmd-copy" onclick="copyCmd('fan 45')">fan 45</code> / <code class="cmd-copy" onclick="copyCmd('extractor 60')">extractor 60</code></td>
              <td>Повні команди встановлення потужності Насоса, Вентилятора обігріву або Витяжки (<code>0..100%</code>).</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('plant 50')">plant 50</code> / <code class="cmd-copy" onclick="copyCmd('plant auto')">plant auto</code></td>
              <td>Задати фіксовану ручну швидкість вентилятора обдуву рослин (<code>0..100%</code>) або повернути його до таймера й природного вітру.</td></tr>

          <tr><td rowspan="2"><strong>🌡️ Цільовий клімат</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('tmin 24.0')">tmin 24.0</code> / <code class="cmd-copy" onclick="copyCmd('tmax 26.0')">tmax 26.0</code> / <code class="cmd-copy" onclick="copyCmd('temp 25')">temp 25</code></td>
              <td>Змінити й зберегти мін./макс. температуру (°C). Команда <code>temp X</code> ставить діапазон <code>X .. X+1°C</code>.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('hmin 60')">hmin 60</code> / <code class="cmd-copy" onclick="copyCmd('hmax 65')">hmax 65</code> / <code class="cmd-copy" onclick="copyCmd('hum 60')">hum 60</code></td>
              <td>Змінити й зберегти мін./макс. вологість (%). Команда <code>hum X</code> ставить діапазон <code>X .. X+5%</code>.</td></tr>

          <tr><td rowspan="2"><strong>⏰ Таймер витяжки</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('timer set 15:00 45:00')">timer set 15:00 45:00</code> / <code class="cmd-copy" onclick="copyCmd('timer power 70')">timer power 70</code></td>
              <td>Задати цикл витяжки <code>ХВ:СЕК</code> роботи та <code>ХВ:СЕК</code> паузи і робочу потужність таймера (10–100%).</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('timer on 30')">timer on 30</code> / <code class="cmd-copy" onclick="copyCmd('timer off')">timer off</code></td>
              <td>Увімкнути безперервну роботу таймера на N хвилин (1–240) або повністю вимкнути таймер витяжки.</td></tr>

          <tr><td rowspan="2"><strong>🎯 Серво і Тести</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('servo')">servo</code> / <code class="cmd-copy" onclick="copyCmd('servo move 90')">servo move 90</code> / <code class="cmd-copy" onclick="copyCmd('servo test')">servo test</code></td>
              <td>Показати кути заслонки / плавно повернути серво на кут <code>0..180°</code> / виконати цикл відкриття→закриття.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('servo set open')">servo set open</code> / <code class="cmd-copy" onclick="copyCmd('servo set closed')">servo set closed</code></td>
              <td>Зберегти поточний кут сервопривода як положення <strong>ВІДКРИТО</strong> або <strong>ЗАКРИТО</strong>; також доступні <code class="cmd-copy" onclick="copyCmd('test pump')">test pump</code>, <code class="cmd-copy" onclick="copyCmd('test fan')">test fan</code>, <code class="cmd-copy" onclick="copyCmd('test vent')">test vent</code>.</td></tr>

          <tr><td rowspan="3"><strong>☁️ Хмара та Система</strong></td>
              <td><code class="cmd-copy" onclick="copyCmd('sheets-sync')">sheets-sync</code> / <code class="cmd-copy" onclick="copyCmd('sheets-stats')">sheets-stats</code></td>
              <td>Примусово відправити накопичені дані в Google Sheets / показати статистику відправлених пакетів і помилок.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('status')">status</code> / <code class="cmd-copy" onclick="copyCmd('save')">save</code> / <code class="cmd-copy" onclick="copyCmd('quiet')">quiet</code> / <code class="cmd-copy" onclick="copyCmd('verbose')">verbose</code></td>
              <td>Оновити статус / зберегти поточну конфігурацію в NVS / вимкнути або увімкнути періодичний автовивід у Serial Monitor.</td></tr>
          <tr><td><code class="cmd-copy" onclick="copyCmd('web')">web</code> / <code class="cmd-copy" onclick="copyCmd('wifi reset')">wifi reset</code> / <code class="cmd-copy" onclick="copyCmd('reboot')">reboot</code></td>
              <td>Показати поточний IP веб-сервера / очистити збережені Wi-Fi мережі / безпечно перезавантажити ESP32.</td></tr>
        </tbody>
      </table>
    </div>
    <div class="hlp-note">
      <strong>💡 Додатково в USB Serial Monitor:</strong> підтримується перемикання між компактним і повним меню командою <code>mode</code> (або <code>m</code> / <code>s</code> / <code>cool</code> / <code>season</code> / <code>sheets-reset</code>).
    </div>
  </div>
</details>
)rawliteral";

static const char HELP_SEC_9_10_11[] PROGMEM = R"rawliteral(
<!-- 9. АПАРАТНА СХЕМА ТА GPIO -->
<details class="hlp-sec" id="sec-hw">
  <summary><span>🔌 9. Апаратна схема, Датчики, Розпіновка ESP32-S3 (GPIO) та Відладка</span></summary>
  <div class="hlp-body">
    <h3>📌 Таблиця підключення всіх модулів до ESP32-S3 (16 MB Flash)</h3>
    <div class="hlp-tbl-wrap">
      <table class="hlp-tbl">
        <thead><tr><th>Вузол / Датчик</th><th>Піни ESP32-S3</th><th>Інтерфейс / Параметри</th><th>Призначення в системі</th></tr></thead>
        <tbody>
          <tr><td><strong>🌡️ DS18B20 (×2)</strong></td><td><code>GPIO 16</code></td><td>1-Wire (підтяжка 4.7 кОм)</td><td>Точна температура кімнати (основна) та температура теплоносія (труба/радіатор).</td></tr>
          <tr><td><strong>💧 BME280</strong></td><td><code>SDA=GPIO 1</code>, <code>SCL=GPIO 2</code></td><td>I²C шина #0</td><td>Відносна вологість повітря (%), атмосферний тиск (hPa) та резервна температура.</td></tr>
          <tr><td><strong>💨 Sensirion SCD30</strong></td><td><code>SDA=GPIO 4</code>, <code>SCL=GPIO 5</code></td><td>I²C шина #1 (NDIR)</td><td>Концентрація CO₂ (ppm), додаткова температура і вологість (інтервал 2–1800 с).</td></tr>
          <tr><td><strong>⚡ PZEM-004T v3.0</strong></td><td><code>RX=GPIO 18</code>, <code>TX=GPIO 17</code></td><td>UART2 (Modbus RTU, 9600)</td><td>Моніторинг 220В, струму, Вт, кВт·год (День/Ніч) та миттєве виявлення блекауту.</td></tr>
          <tr><td><strong>🔵 Насос (A)</strong></td><td><code>GPIO 21</code></td><td>LEDC PWM CH0, 25 кГц, 8-біт</td><td>Циркуляція теплоносія (Kickstart при старті + SmartPump за швидкістю нагріву).</td></tr>
          <tr><td><strong>🟠 Вентилятор обігріву (B)</strong></td><td><code>GPIO 38</code> + <code>GPIO 7</code></td><td>PWM CH1 (25 кГц) + Реле 220В</td><td>Знімання тепла з радіатора (плавний розгін FanSmooth + відсічення живлення реле).</td></tr>
          <tr><td><strong>🟢 Витяжка (C)</strong></td><td><code>GPIO 12</code></td><td>LEDC PWM CH2, 25 кГц, 8-біт</td><td>Вентиляція боксу за температурою або за циклічним таймером роботи/паузи.</td></tr>
          <tr><td><strong>🌿 Вітер рослин</strong></td><td><code>GPIO 11</code></td><td>LEDC PWM CH3, 25 кГц, 8-біт</td><td>Внутрішній обдув рослин за таймером та алгоритмом природного вітру (Breeze).</td></tr>
          <tr><td><strong>🌫️ Зволожувач</strong></td><td><code>GPIO 6</code></td><td>Цифровий вихід (HIGH/LOW)</td><td>Керування зволожувачем повітря із захистом від частих пусків і переливу (макс. 10 хв).</td></tr>
          <tr><td><strong>🎯 Серво заслонки + Тумблер</strong></td><td><code>GPIO 14</code> + <code>GPIO 13</code></td><td>Servo PWM + Digital Input</td><td>Плавний привід повітряної заслонки (auto-detach через 1 с) + механічний перемикач.</td></tr>
          <tr><td><strong>💡 DMX512 (MAX485)</strong></td><td><code>TX=40</code>, <code>RX=41</code>, <code>DE/RE=37</code></td><td>UART1 (RS485, 250 kbps, 8N2)</td><td>Керування 4-канальним світильником RGBW/Master за добовим графіком.</td></tr>
          <tr><td><strong>🔆 TC421 Фітолампа</strong></td><td>Вбудований Wi-Fi</td><td>AP <code>KlimatLight</code>, UDP :5000</td><td>Бездротове 4-канальне керування спектром росту, світанками та EOD Far-Red.</td></tr>
        </tbody>
      </table>
    </div>

    <h3>🔧 Як читати показники на сторінці Відладка (<a href="/debug">/debug</a>)</h3>
    <ul>
      <li><strong>Причина старту та лічильник збоїв:</strong> показує загальну кількість запусків і чи були аварійні перезавантаження (Watchdog / Panic / Brownout) із часом останнього збою.</li>
      <li><strong>Макс. суцільний блок RAM (Heap):</strong> найважливіший показник для хмарної синхронізації. Для відкриття захищеного TLS/HTTPS з'єднання з Google Sheets потрібен суцільний блок <strong>не менше 50 KB</strong> (зелене значення).</li>
      <li><strong>Запас стеку задач FreeRTOS (High Water Mark):</strong> показує мінімальний залишок стеку кожної з 10 фонових задач. Зелений колір (<code>&gt;1200 B</code>) означає повну безпеку від переповнення стеку.</li>
    </ul>
  </div>
</details>

<!-- 10. МЕРЕЖА, БЕЗПЕКА, OTA ТА BACKUP -->
<details class="hlp-sec" id="sec-net">
  <summary><span>📶 10. Доступ із пристроїв, Wi-Fi, Безпека, Прошивка OTA та Backup</span></summary>
  <div class="hlp-body">
    <h3>🔗 Як відкрити інтерфейс із різних пристроїв</h3>
    <ul>
      <li><strong>💻 Ноутбук / ПК (Windows, macOS, Linux) та 📱 iPhone / iPad:</strong> працюють імена <code>http://klimat.local</code> (mDNS) та <code>http://klimat</code> (NetBIOS на Windows), а також пряма IP-адреса.</li>
      <li><strong>🤖 Android-смартфони та планшети:</strong> Android стандартно не резолвить <code>.local</code>, тому заходьте за <strong>IP-адресою</strong> (відскануйте QR-код на Головній сторінці). <em>Порада:</em> у Chrome натисніть <code>⋮ → Додати на головний екран</code> — з'явиться зручна іконка додатку!</li>
      <li><strong>Розумний мульти-Wi-Fi (<a href="/wifi">/wifi</a>):</strong> при старті ESP32 сканує ефір, порівнює зі списком збережених мереж та <strong>автоматично підключається до тієї відомої мережі, у якої найсильніший сигнал RSSI</strong>. Щоб IP не змінювався на роутері чи хотспоті — увімкніть <em>Статичну IP</em>.</li>
    </ul>

    <h3>🔐 Безпека та доступ через інтернет</h3>
    <ul>
      <li>У <a href="/settings" onclick="goTab(8)">Налаштування → Wi-Fi</a> можна увімкнути <strong>HTTP-автентифікацію</strong> (логін та складний пароль). POST-запити додатково захищені перевіркою <code>CSRF Referer</code>.</li>
      <li><strong>⚠️ Критично:</strong> якщо ви відкриваєте зовнішній доступ до контролера через роутер (<em>Port Forwarding</em> на внутрішній порт <code>80</code> або VPN/тунель), <strong>обов'язково увімкніть пароль</strong>!</li>
    </ul>

    <h3>🔄 Оновлення прошивки по повітрю (OTA) та Резервні копії (<a href="/ota">/ota</a>)</h3>
    <ul>
      <li><strong>Web OTA:</strong> оберіть скомпільований файл <code>firmware.bin</code> на сторінці <a href="/ota">/ota</a> і натисніть <em>«Завантажити прошивку»</em>. Перед початком запису система <strong>автоматично створює NVS Backup із контрольною сумою CRC32</strong> та тимчасово призупиняє фонові задачі для 100% надійної прошивки.</li>
      <li><strong>Backup та перенесення налаштувань:</strong>
        <ul>
          <li><em>Створити / Відновити Backup</em> — миттєва резервна копія всіх параметрів у внутрішній NVS-пам'яті чіпа.</li>
          <li><em>Export JSON / Import JSON</em> — завантажує всі ваші налаштування клімату у файл <code>klimat_config_*.json</code> на телефон або комп'ютер, щоб мати зовнішню копію на будь-який випадок.</li>
          <li><em>Factory Reset</em> — повне скидання до заводських параметрів (потребує подвійного підтвердження).</li>
        </ul>
      </li>
    </ul>
  </div>
</details>

<!-- 11. УСУНЕННЯ НЕСПРАВНОСТЕЙ (FAQ) -->
<details class="hlp-sec" id="sec-faq">
  <summary><span>🛠️ 11. Часті питання та Усунення несправностей (Troubleshooting)</span></summary>
  <div class="hlp-body">
    <div class="hlp-faq">
      <div class="q">🌡️ Датчик температури показує «🚨 ПОМИЛКА» (або у логах -127°C / 85°C)</div>
      <div class="a">Перевірте контакт шини 1-Wire на <code>GPIO 16</code>, живлення 3.3В та наявність підтягуючого резистора <strong>4.7 кОм</strong> між лінією даних і 3.3В. Значення <code>85°C</code> означає, що датчик щойно перезавантажився через поганий контакт живлення; <code>-127°C</code> — обрив лінії даних.</div>
    </div>

    <div class="hlp-faq">
      <div class="q">💨 CO₂ (SCD30) або Вологість (BME280) показує «НЕ ВІДПОВІДАЄ»</div>
      <div class="a">BME280 працює на <code>GPIO 1 (SDA) / GPIO 2 (SCL)</code>, а SCD30 — на окремій шині <code>GPIO 4 (SDA) / GPIO 5 (SCL)</code>. Перевірте довжину екранованого кабелю I²C та живлення датчика. SCD30 потребує перших 4–6 секунд після подачі живлення для видачі першого заміру.</div>
    </div>

    <div class="hlp-faq">
      <div class="q">🔆 Фітолампа TC421 у статусі «Не знайдено»</div>
      <div class="a">Переконайтеся, що на TC421 подано живлення 12/24В і він попередньо прив'язаний у режимі STA до мережі <strong><code>KlimatLight</code></strong> (пароль <code>12345678</code>). Натисніть <strong>«🔍 Знайти»</strong> на сторінці <a href="/tc421">/tc421</a>. Якщо ви знаєте статичний IP лампи (зазвичай <code>192.168.10.2</code> або <code>192.168.4.2</code>), розгорніть <em>«Налаштування IP / MAC вручну»</em> і збережіть його.</div>
    </div>

    <div class="hlp-faq">
      <div class="q">💧 Насос гудить на малих відсотках, але вода не циркулює</div>
      <div class="a">Відкрийте <a href="/settings" onclick="goTab(1)">Налаштування → Пристрої</a>: підвищте <strong>«Мінімум насоса (%)»</strong> до порогу стабільного обертання (зазвичай 28–33%) та переконайтеся, що увімкнений <strong>«🚀 Пускова потужність»</strong> (45–55% на 2–3 сек) для надійного зриву ротора з місця.</div>
    </div>

    <div class="hlp-faq">
      <div class="q">🎯 Сервопривод заслонки впирається в край і дзижчить</div>
      <div class="a">Крайні кути впираються в механічний упор. Перейдіть у <a href="/settings" onclick="goTab(7)">Налаштування → Серво</a>, натисніть <strong>«🔒 Увійти в калібрування»</strong>, кнопками <code>+1° / -1°</code> відведіть тягу на 2–3° від упору, де немає напруження, і натисніть <em>«💾 Зберегти як ЗАКРИТО / ВІДКРИТО»</em>, після чого вийдіть із режиму калібрування.</div>
    </div>

    <div class="hlp-faq">
      <div class="q">🎙️ На планшеті в «Степані» не працює розпізнавання голосу або немає звуку</div>
      <div class="a">1) Дозвольте <code>http://</code> адресу контролера в прапорці Chrome <code>chrome://flags/#unsafely-treat-insecure-origin-as-secure</code> (див. Розділ 6).<br>2) Перевірте в налаштуваннях Android <em>«Синтез мовлення Google»</em> наявність українського голосового пакета (<code>uk-UA</code>) та гучність медіа.<br>3) Якщо заважає гул обдуву — увімкніть галочку <em>«Авто-тиша»</em> у налаштуваннях «Степана».</div>
    </div>

    <div class="hlp-faq">
      <div class="q">☁️ Помилка синхронізації з Google Sheets</div>
      <div class="a">1) Перевірте наявність інтернету на роутері/телефоні, до якого підключений ESP32.<br>2) Перевірте на сторінці <a href="/debug">Відладка</a> параметр <em>«Макс. суцільний блок»</em> (має бути &gt;50 KB) та чи синхронізований системний час NTP (<a href="/settings" onclick="goTab(10)">Час</a>).<br>3) Якщо активний режим ЕКО з галочкою <em>«Призупинити Google Sheets при блекауті»</em> — зачекайте повернення 220В або натисніть ручний <strong>«📤 SYNC»</strong>.</div>
    </div>

    <div class="hlp-faq">
      <div class="q">📶 Змінили пароль роутера або ввели неправильний SSID і немає зв'язку</div>
      <div class="a">Система при старті автоматично перебирає всі раніше збережені мережі та резервні хотспоти (наприклад, роздачу з телефону <code>Redmi Note 14</code>). Також ви завжди можете підключитися телефоном до вбудованої точки доступу контролера <strong><code>KlimatLight</code></strong> (пароль <code>12345678</code>) або через USB Serial ввести команду <code class="cmd-copy" onclick="copyCmd('wifi reset')">wifi reset</code>.</div>
    </div>
  </div>
</details>

<div id="hlpToast">✅ Скопійовано!</div>

<script>
(function(){
  var originEl = document.getElementById('stepanOriginCmd');
  if (originEl && location.origin && location.origin.indexOf('http') === 0) {
    originEl.textContent = location.origin;
  }
})();
function goTab(id) {
  try { localStorage.setItem('settingsTab', String(id)); } catch(e) {}
}
function openSec(id) {
  var el = document.getElementById(id);
  if (el && el.tagName === 'DETAILS') el.open = true;
}
function toggleAllHelp(openState) {
  document.querySelectorAll('details.hlp-sec').forEach(function(d){
    if (d.style.display !== 'none') d.open = openState;
  });
}
var _toastTimer = null;
function showToast(msg) {
  var t = document.getElementById('hlpToast');
  if (!t) return;
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(_toastTimer);
  _toastTimer = setTimeout(function(){ t.classList.remove('show'); }, 1900);
}
function copyCmd(txt) {
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(txt).then(function(){
      showToast('✅ Скопійовано: ' + txt);
    }).catch(function(){ fallbackCopy(txt); });
  } else {
    fallbackCopy(txt);
  }
}
function fallbackCopy(txt) {
  var ta = document.createElement('textarea');
  ta.value = txt;
  ta.style.position = 'fixed';
  ta.style.opacity = '0';
  document.body.appendChild(ta);
  ta.select();
  try { document.execCommand('copy'); showToast('✅ Скопійовано: ' + txt); } catch(e) {}
  document.body.removeChild(ta);
}
function filterHelp(raw) {
  var q = (raw || '').trim().toLowerCase();
  var secs = document.querySelectorAll('details.hlp-sec');
  var cntEl = document.getElementById('hlpCount');
  if (!q) {
    secs.forEach(function(d, idx){
      d.style.display = '';
      d.open = (idx === 0);
    });
    if (cntEl) cntEl.style.display = 'none';
    return;
  }
  var matched = 0;
  secs.forEach(function(d){
    var txt = (d.textContent || '').toLowerCase();
    if (txt.indexOf(q) !== -1) {
      d.style.display = '';
      d.open = true;
      matched++;
    } else {
      d.style.display = 'none';
    }
  });
  if (cntEl) {
    cntEl.style.display = 'block';
    cntEl.textContent = matched > 0
      ? ('Знайдено розділів за запитом «' + raw.trim() + '»: ' + matched + ' із ' + secs.length)
      : ('Нічого не знайдено за запитом «' + raw.trim() + '». Спробуйте інше ключове слово.');
  }
}
</script>
)rawliteral";

void handleHelpPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) return;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");

    String html;
    html.reserve(3072);
    auto flushChunk = [&]() {
        if (html.length() > 0) {
            server.sendContent(html);
            html = "";
            yield();
        }
    };

    html = getHtmlHead("📖 Довідка — Клімат-Контроль");
    html += "<div class='container'>";
    html += getNavHeader("📖 ПОВНА ДОВІДКА ТА ІНСТРУКЦІЯ СИСТЕМИ");

    // Жива шапка поточного стану пристрою
    String ipStr = WiFi.localIP().toString();
    String urlStr = "http://" + ipStr;
    String curModeStr = heatingState.emergencyMode ? "🚨 АВАРІЯ"
                      : (heatingState.forceMode ? "⚡ ФОРСАЖ"
                      : (heatingState.manualMode ? (heatingState.manualModeLocked ? "✋ РУЧНИЙ 🔒" : "✋ РУЧНИЙ") : "🤖 АВТО"));
    String ecoStr = isEcoModeActive() ? "🔋 ЕКО АКТИВНИЙ" : "🟢 Штатний 220В";

    html += "<div class='hlp-hero'>";

    html += "<div class='hlp-stat' style='border-left-color:var(--a)'>";
    html += "<div class='k'>🌐 Адреса підключення</div>";
    html += "<div class='v'><span class='cmd-copy' onclick=\"copyCmd('" + urlStr + "')\" title='Натисніть щоб скопіювати'>" + urlStr + "</span></div>";
    html += "<div class='sub'>mDNS: <code>http://klimat.local</code> | AP: <code>" LIGHT_AP_SSID "</code></div>";
    html += "</div>";

    html += "<div class='hlp-stat' style='border-left-color:var(--ok)'>";
    html += "<div class='k'>📶 Поточний Wi-Fi</div>";
    html += "<div class='v'>" + htmlEscape(WiFi.SSID().length() ? WiFi.SSID() : String("Режим точки доступу")) + "</div>";
    html += "<div class='sub'>Сигнал: " + String(WiFi.RSSI()) + " dBm | Auth: " + String(config.useAuth ? "Увімкнено 🔐" : "Вимкнено") + "</div>";
    html += "</div>";

    html += "<div class='hlp-stat' style='border-left-color:var(--w)'>";
    html += "<div class='k'>⚙️ Режим та Живлення</div>";
    html += "<div class='v'>" + curModeStr + "</div>";
    html += "<div class='sub'>" + ecoStr + " | Ціль: " + String(config.tempMin, 1) + "–" + String(config.tempMax, 1) + "°C, " +
            String(config.humidityConfig.minHumidity, 0) + "–" + String(config.humidityConfig.maxHumidity, 0) + "%</div>";
    html += "</div>";

    html += "<div class='hlp-stat' style='border-left-color:#a855f7'>";
    html += "<div class='k'>📦 Прошивка ESP32-S3</div>";
    html += "<div class='v'>" VERSION "</div>";
    html += "<div class='sub'>Збірка: " BUILD_DATE " " BUILD_TIME "</div>";
    html += "</div>";

    html += "</div>"; // .hlp-hero
    flushChunk();

    // Відправляємо статичні блоки напряму з PROGMEM (0 байт алокації в Heap)
    server.sendContent_P(HELP_CSS_AND_CONTROLS);
    yield();
    server.sendContent_P(HELP_SEC_1_2);
    yield();
    server.sendContent_P(HELP_SEC_3_4);
    yield();
    server.sendContent_P(HELP_SEC_5_6_7);
    yield();
    server.sendContent_P(HELP_SEC_8);
    yield();
    server.sendContent_P(HELP_SEC_9_10_11);
    yield();

    html += "</div>"; // .container
    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();
    flushChunk();
    server.sendContent("");
}

// ============================================================================
// СТОРІНКА ВІДЛАДКИ
// ============================================================================

void handleDebugPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) return;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");

    String html;
    html.reserve(3072);
    auto flushChunk = [&]() {
        if (html.length() > 0) {
            server.sendContent(html);
            html = "";
            yield();
        }
    };

    html = getHtmlHead("🔧 Відладка");
    html += "<div class='container'>";
    html += getNavHeader("🔧 ВІДЛАДКОВА ІНФОРМАЦІЯ");
    flushChunk();

    // Діагностика перезавантажень
    int lastRsn = getDiagLastResetReason();
    int lastCrashRsn = getDiagLastCrashReason();
    bool wasCrash = isCrashResetReason(lastRsn);
    String rsnColor = wasCrash ? "#ef4444" : "#10b981";

    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>🔄 Діагностика перезавантажень</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    html += "<div style='display: flex; justify-content: space-between; gap: 10px; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Причина останнього старту:</strong>";
    html += "<span style='color: " + rsnColor + "; font-weight: 700; text-align: right;'>" + htmlEscape(String(resetReasonToStr(lastRsn))) + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Всього запусків / Аварійних збоїв:</strong>";
    html += "<span><b>" + String(getDiagBootCount()) + "</b> / <b style='color:" + String(getDiagCrashCount() > 0 ? "#f59e0b" : "#10b981") + ";'>" + String(getDiagCrashCount()) + "</b></span>";
    html += "</div>";

    if (getDiagCrashCount() > 0) {
        html += "<div style='display: flex; justify-content: space-between; gap: 10px; padding: 10px; background: var(--s); border-radius: 5px;'>";
        html += "<strong>Останній аварійний збій:</strong>";
        html += "<span style='color: #f59e0b; text-align: right;'>" + htmlEscape(String(resetReasonToStr(lastCrashRsn))) + " (" + htmlEscape(getDiagLastCrashTime()) + ")</span>";
        html += "</div>";
    }

    html += "</div></div>";

    // Системна інформація: Пам'ять
    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>💾 Пам'ять (Heap)</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    uint32_t freeH = ESP.getFreeHeap();
    uint32_t minFreeH = ESP.getMinFreeHeap();
    uint32_t maxBlockH = ESP.getMaxAllocHeap();
    uint32_t totalH = ESP.getHeapSize();

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Вільна пам'ять зараз:</strong>";
    html += "<span style='color: #4CAF50; font-weight: 600;'>" + String(freeH / 1024) + " KB (" + String(freeH) + " B)</span>";
    html += "</div>";

    String minColor = (minFreeH < 35000) ? "#f44336" : ((minFreeH < 55000) ? "#ff9800" : "#4CAF50");
    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Мінімум вільної (пік навантаження):</strong>";
    html += "<span style='color: " + minColor + "; font-weight: 600;'>" + String(minFreeH / 1024) + " KB (" + String(minFreeH) + " B)</span>";
    html += "</div>";

    String blkColor = (maxBlockH < 50000) ? "#ff9800" : "#4CAF50";
    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Макс. суцільний блок (для SSL):</strong>";
    html += "<span style='color: " + blkColor + "; font-weight: 600;'>" + String(maxBlockH / 1024) + " KB (" + String(maxBlockH) + " B)</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Всього пам'яті:</strong>";
    html += "<span>" + String(totalH / 1024) + " KB</span>";
    html += "</div>";

    float heapUsed = 100.0f - (freeH * 100.0f / (totalH ? totalH : 1));
    String heapColor = heapUsed > 80 ? "#f44336" : (heapUsed > 60 ? "#ff9800" : "#4CAF50");
    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Використано:</strong>";
    html += "<span style='color: " + heapColor + "; font-weight: 600;'>" + String(heapUsed, 1) + "%</span>";
    html += "</div>";

    if (psramFound()) {
        uint32_t totalP = ESP.getPsramSize();
        uint32_t freeP = ESP.getFreePsram();
        float psramUsed = 100.0f - (freeP * 100.0f / (totalP ? totalP : 1));
        html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
        html += "<strong>PSRAM (вільно / всього):</strong>";
        html += "<span style='color: #38bdf8; font-weight: 600;'>" + String(freeP / 1024) + " KB / " + String(totalP / 1024) + " KB (" + String(psramUsed, 1) + "% зайнято)</span>";
        html += "</div>";
    } else {
        html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
        html += "<strong>PSRAM:</strong>";
        html += "<span style='color: #f59e0b; font-weight: 600;'>не виявлено (fallback RAM)</span>";
        html += "</div>";
    }

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Буфер історії:</strong>";
    html += "<span><b>" + String(getRamBufferCapacity()) + "</b> записів (" + String(isRamBufferInPsram() ? "PSRAM" : "внутрішня RAM") + ")</span>";
    html += "</div>";

    html += "</div></div>";
    flushChunk();

    // Запас стеку задач FreeRTOS
    auto addStackRow = [&](const char* label, TaskHandle_t h) {
        if (h == NULL) return;
        uint32_t freeBytes = (uint32_t)uxTaskGetStackHighWaterMark(h);
        String col = (freeBytes < 512) ? "#f44336" : ((freeBytes < 1200) ? "#ff9800" : "#4CAF50");
        html += "<div style='display: flex; justify-content: space-between; padding: 8px 10px; background: var(--s); border-radius: 5px;'>";
        html += "<span>" + String(label) + ":</span>";
        html += "<span style='color:" + col + "; font-weight:600;'>мінімум вільно " + String(freeBytes) + " B</span>";
        html += "</div>";
    };

    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>🧱 Запас стеку задач (High Water Mark)</h3>";
    html += "<div style='display: grid; gap: 8px;'>";
    addStackRow("WebTask (веб-сервер)", webTaskHandle);
    addStackRow("AdvLogicTask (логіка)", advancedLogicTaskHandle);
    addStackRow("SensorTask (датчики)", sensorTaskHandle);
    addStackRow("HeatingTask (обігрів)", heatingTaskHandle);
    addStackRow("DataLoggerTask (логи)", dataLoggerTaskHandle);
    addStackRow("VentilationTask (серво)", ventTaskHandle);
    addStackRow("PlantFanTask (вітер)", plantFanTaskHandle);
    html += "</div></div>";
    flushChunk();

    // CPU та система
    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>🖥️ Система</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Задач FreeRTOS:</strong>";
    html += "<span>" + String(uxTaskGetNumberOfTasks()) + "</span>";
    html += "</div>";

    unsigned long uptimeSec = millis() / 1000;
    unsigned long days = uptimeSec / 86400;
    unsigned long hours = (uptimeSec % 86400) / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    unsigned long secs = uptimeSec % 60;
    String uptimeStr = "";
    if (days > 0) uptimeStr += String(days) + "д ";
    uptimeStr += String(hours) + "г " + String(minutes) + "хв " + String(secs) + "с";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Час роботи:</strong>";
    html += "<span style='color: #2196F3; font-weight: 600;'>" + uptimeStr + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Температура чіпа:</strong>";
    html += "<span>" + String(temperatureRead(), 1) + "°C</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Частота CPU:</strong>";
    html += "<span>" + String(getCpuFrequencyMhz()) + " MHz</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Версія SDK:</strong>";
    html += "<span>" + String(ESP.getSdkVersion()) + "</span>";
    html += "</div>";

    html += "</div>";
    html += "</div>";

    // WiFi інформація
    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>📶 WiFi</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>SSID:</strong>";
    html += "<span>" + htmlEscape(WiFi.SSID()) + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>IP:</strong>";
    html += "<span>" + WiFi.localIP().toString() + "</span>";
    html += "</div>";

    int rssi = WiFi.RSSI();
    String rssiColor = rssi > -50 ? "#4CAF50" : (rssi > -70 ? "#ff9800" : "#f44336");
    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>Сигнал:</strong>";
    html += "<span style='color: " + rssiColor + "; font-weight: 600;'>" + String(rssi) + " dBm</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: var(--s); border-radius: 5px;'>";
    html += "<strong>MAC:</strong>";
    html += "<span style='font-family: monospace;'>" + WiFi.macAddress() + "</span>";
    html += "</div>";

    html += "</div>";
    html += "</div>";

    html += "</div>"; // container
    flushChunk();

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();
    flushChunk();
    server.sendContent("");
}

