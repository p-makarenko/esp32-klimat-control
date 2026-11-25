// ============================================================================
// WEB_PAGES_TABLET.CPP - Екран для Android-планшета + голосовий помічник "Степан"
// ============================================================================
// Маршрут: GET /tablet
// Функціонал:
// - Чорний режим енергозбереження (очікування) + Screen Wake Lock
// - Пробудження голосом по кодовому слову "Степан" (або дотиком)
// - Стартовий екран: Температура та Вологість великим шрифтом
// - Динамічний показ будь-яких параметрів за запитом ("Покажи CO2", "Покажи все")
// - Голосові відповіді українською (вивід на Bluetooth-колонку планшета)
// - Голосове керування системою через /command (з підтвердженням критичних дій)
// ============================================================================

#include "web_interface.h"
#include <WebServer.h>

extern bool checkAuth();

static const char TABLET_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#000000">
<title>Степан — Екран клімату</title>
<style>
:root{
  --bg:#060911;
  --card:#101726;
  --card2:#162033;
  --border:#22314d;
  --text:#f8fafc;
  --muted:#94a3b8;
  --accent:#38bdf8;
  --warm:#fb7185;
  --hum:#38bdf8;
  --co2:#34d399;
  --warn:#f59e0b;
  --err:#ef4444;
}
*{box-sizing:border-box;user-select:none;-webkit-tap-highlight-color:transparent}
html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:#000;color:var(--text);font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}

/* Стартовий екран активації (1 клік для дозволу мікрофона, звуку і повного екрана на Android) */
#startGate{
  position:fixed;inset:0;z-index:1000;background:#050810;
  display:flex;flex-direction:column;align-items:center;justify-content:center;
  padding:24px;text-align:center;gap:18px;
}
#startGate h1{margin:0;font-size:clamp(26px,5vw,44px);font-weight:700;letter-spacing:-0.02em}
#startGate p{margin:0;max-width:560px;color:var(--muted);font-size:clamp(14px,2.2vw,18px);line-height:1.5}
.start-btn{
  background:linear-gradient(135deg,#0ea5e9,#2563eb);color:#fff;border:none;
  padding:20px 36px;border-radius:18px;font-size:clamp(18px,3vw,24px);font-weight:700;
  cursor:pointer;box-shadow:0 10px 35px rgba(14,165,233,.35);transition:transform .15s;
}
.start-btn:active{transform:scale(.97)}
.http-warn{
  display:none;max-width:640px;background:#1f1608;border:1px solid #f59e0b;
  border-radius:12px;padding:14px 16px;font-size:13px;color:#fde68a;text-align:left;line-height:1.45;
}
.http-warn code{background:#000;color:#38bdf8;padding:2px 6px;border-radius:4px;user-select:text}

/* Режим енергозбереження (повністю чорний екран) */
#sleepOverlay{
  position:fixed;inset:0;z-index:500;background:#000;
  display:flex;flex-direction:column;align-items:center;justify-content:center;
  opacity:0;pointer-events:none;transition:opacity .7s ease;
  cursor:pointer;
}
#sleepOverlay.active{opacity:1;pointer-events:auto}
.sleep-clock{font-size:clamp(42px,10vw,86px);font-weight:200;color:rgba(255,255,255,.10);font-variant-numeric:tabular-nums;letter-spacing:2px}
.sleep-hint{margin-top:14px;font-size:14px;color:rgba(255,255,255,.12);letter-spacing:.5px}
.sleep-dot{
  width:8px;height:8px;border-radius:50%;margin-top:16px;
  background:rgba(56,189,248,.22);animation:breathe 4s infinite ease-in-out;
}
@keyframes breathe{0%,100%{opacity:.2;transform:scale(.9)}50%{opacity:.7;transform:scale(1.25)}}

/* Основний інтерфейс */
#app{
  width:100%;height:100%;background:var(--bg);
  display:flex;flex-direction:column;padding:clamp(12px,2vw,24px);gap:clamp(10px,1.8vw,18px);
}
.topbar{
  display:flex;align-items:center;justify-content:space-between;gap:12px;
  padding:10px 16px;background:var(--card);border:1px solid var(--border);border-radius:16px;
}
.brand{display:flex;align-items:center;gap:12px}
.avatar{
  width:42px;height:42px;border-radius:50%;background:var(--card2);border:2px solid var(--border);
  display:flex;align-items:center;justify-content:center;font-size:20px;transition:.25s;
}
.avatar.listening{border-color:var(--accent);box-shadow:0 0 18px rgba(56,189,248,.45)}
.avatar.active-dialog{border-color:var(--co2);background:#0b2a22;box-shadow:0 0 24px rgba(52,211,153,.6);animation:pulse 1.2s infinite}
.avatar.speaking{border-color:var(--warn);box-shadow:0 0 22px rgba(245,158,11,.55)}
@keyframes pulse{0%,100%{transform:scale(1)}50%{transform:scale(1.08)}}
.title-col{display:flex;flex-direction:column}
.title-main{font-size:clamp(16px,2.2vw,20px);font-weight:700}
.title-sub{font-size:12px;color:var(--muted)}
.top-right{display:flex;align-items:center;gap:10px;flex-wrap:wrap;justify-content:flex-end}
.pill{
  padding:6px 12px;border-radius:999px;font-size:13px;font-weight:600;
  background:var(--card2);border:1px solid var(--border);color:var(--text);
}
.pill.mode-auto{border-color:#10b981;color:#34d399}
.pill.mode-manual{border-color:#f59e0b;color:#fbbf24}
.pill.mode-err{border-color:#ef4444;color:#f87171}
.clock-top{font-size:clamp(18px,2.6vw,24px);font-weight:700;font-variant-numeric:tabular-nums;color:var(--text)}
.icon-btn{
  background:var(--card2);color:var(--text);border:1px solid var(--border);
  border-radius:12px;padding:8px 12px;font-size:16px;cursor:pointer;
}
.icon-btn:active{transform:scale(.95)}

/* Динамічна сітка карток */
.grid{
  flex:1;min-height:0;display:grid;gap:clamp(12px,2vw,22px);
  grid-template-columns:repeat(auto-fit,minmax(260px,1fr));
  align-content:stretch;
}
.grid.count-1{grid-template-columns:1fr}
.grid.count-2{grid-template-columns:repeat(2,1fr)}
@media(max-width:680px){.grid.count-2{grid-template-columns:1fr}}
.card{
  background:linear-gradient(180deg,var(--card) 0%,#0c1220 100%);
  border:1px solid var(--border);border-radius:24px;
  padding:clamp(18px,3vw,32px);
  display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center;
  position:relative;overflow:hidden;transition:border-color .25s,transform .2s;
}
.card::before{
  content:'';position:absolute;top:0;left:0;right:0;height:4px;background:var(--c,var(--accent));
}
.card-label{font-size:clamp(16px,2.5vw,24px);color:var(--muted);font-weight:600;margin-bottom:8px;display:flex;align-items:center;gap:8px}
.card-val{
  font-size:clamp(54px,11vw,126px);font-weight:800;line-height:1.02;
  font-variant-numeric:tabular-nums;color:var(--c,var(--text));letter-spacing:-0.03em;
}
.grid.count-many .card-val{font-size:clamp(34px,6vw,64px)}
.card-unit{font-size:0.42em;font-weight:600;opacity:.8;margin-left:4px}
.card-sub{margin-top:10px;font-size:clamp(13px,1.8vw,17px);color:var(--muted)}

/* Нижня панель голосового помічника */
.voicebar{
  display:flex;align-items:center;gap:12px;
  padding:12px 16px;background:var(--card);border:1px solid var(--border);border-radius:16px;
}
.mic-btn{
  flex:0 0 auto;width:54px;height:54px;border-radius:50%;border:none;
  background:#1e293b;color:#fff;font-size:24px;cursor:pointer;
  display:flex;align-items:center;justify-content:center;transition:.2s;
}
.mic-btn.on{background:#0284c7;box-shadow:0 0 20px rgba(2,132,199,.5)}
.mic-btn.dialog{background:#059669;box-shadow:0 0 24px rgba(16,185,129,.65)}
.voice-text{flex:1;min-width:0}
.voice-heard{font-size:clamp(14px,2vw,18px);font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.voice-reply{font-size:13px;color:var(--accent);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:2px}
.quick-chips{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.chip{
  background:var(--card2);color:var(--muted);border:1px solid var(--border);
  border-radius:10px;padding:7px 11px;font-size:12px;font-weight:600;cursor:pointer;
}
.chip:active,.chip.active{background:var(--accent);color:#000;border-color:var(--accent)}

/* Модальне вікно налаштувань */
#settingsModal{
  position:fixed;inset:0;z-index:800;background:rgba(0,0,0,.78);
  display:none;align-items:center;justify-content:center;padding:18px;
}
#settingsModal.open{display:flex}
.modal-box{
  width:100%;max-width:480px;background:var(--card);border:1px solid var(--border);
  border-radius:20px;padding:22px;display:flex;flex-direction:column;gap:14px;
  max-height:90vh;overflow-y:auto;
}
.modal-box h2{margin:0;font-size:20px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:8px 0;border-bottom:1px solid var(--border)}
.row label{font-size:14px;color:var(--text)}
.row select,.row input[type=checkbox]{background:var(--card2);color:var(--text);border:1px solid var(--border);border-radius:8px;padding:7px 10px;font-size:14px}
.help-list{font-size:13px;color:var(--muted);line-height:1.55;background:var(--bg);padding:12px;border-radius:12px;border:1px solid var(--border)}
.help-list b{color:var(--text)}
</style>
</head>
<body>

<!-- Екран першого запуску -->
<div id="startGate">
  <div style="font-size:56px">🎙️</div>
  <h1>Голосовий екран «Степан»</h1>
  <p>Натисніть кнопку нижче один раз, щоб активувати повний екран, мікрофон планшета, голосові відповіді на Bluetooth-колонку та темний режим енергозбереження.</p>
  <button class="start-btn" onclick="startStepan()">Запустити Степана</button>
  <div id="httpWarn" class="http-warn">
    ⚠️ <b>Важливо для мікрофона в Chrome на Android:</b> локальні адреси <code>http://</code> потребують одноразового дозволу.<br>
    1. Відкрийте в Chrome: <code>chrome://flags/#unsafely-treat-insecure-origin-as-secure</code><br>
    2. Увімкніть (<b>Enabled</b>) і впишіть: <code id="originHint"></code><br>
    3. Натисніть <b>Relaunch</b> (перезапустити Chrome).
  </div>
  <a href="/" style="color:var(--muted);font-size:14px;text-decoration:none;margin-top:8px">← Повернутися до звичайного веб-інтерфейсу</a>
</div>

<!-- Чорний екран енергозбереження -->
<div id="sleepOverlay" onclick="wakeUp(false)">
  <div class="sleep-clock" id="sleepClock">--:--</div>
  <div class="sleep-hint">Скажіть «Степан» або торкніться екрана</div>
  <div class="sleep-dot" id="sleepDot"></div>
</div>

<!-- Основний інтерфейс -->
<div id="app">
  <div class="topbar">
    <div class="brand">
      <div class="avatar" id="avatar">🤖</div>
      <div class="title-col">
        <div class="title-main">Степан</div>
        <div class="title-sub" id="statusSub">Очікування запуску...</div>
      </div>
    </div>
    <div class="top-right">
      <span class="pill" id="modePill">—</span>
      <span class="clock-top" id="topClock">--:--</span>
      <button class="icon-btn" onclick="goSleep()" title="Темний екран">🌙</button>
      <button class="icon-btn" onclick="toggleSettings(true)" title="Налаштування та команди">⚙️</button>
      <a class="icon-btn" href="/" style="text-decoration:none" title="Головний веб-інтерфейс">🏠</a>
    </div>
  </div>

  <div class="grid count-2" id="cardsGrid"></div>

  <div class="voicebar">
    <button class="mic-btn" id="micBtn" onclick="manualActivateDialog()" title="Сказати команду">🎙️</button>
    <div class="voice-text">
      <div class="voice-heard" id="heardText">Скажіть «Степан»...</div>
      <div class="voice-reply" id="replyText">Головний екран: температура та вологість</div>
    </div>
    <div class="quick-chips">
      <button class="chip active" id="chip-home" onclick="setView(['temp','hum'])">Головна</button>
      <button class="chip" id="chip-co2" onclick="setView(['co2'])">CO₂</button>
      <button class="chip" id="chip-all" onclick="setView(['temp','hum','co2','pressure','carrier','actuators','power'])">Все</button>
    </div>
  </div>
</div>

<!-- Налаштування та довідка команд -->
<div id="settingsModal" onclick="if(event.target===this)toggleSettings(false)">
  <div class="modal-box">
    <h2>⚙️ Налаштування «Степана»</h2>
    <div class="row">
      <label for="sleepSec">Темний екран через</label>
      <select id="sleepSec" onchange="savePrefs()">
        <option value="20">20 секунд</option>
        <option value="45" selected>45 секунд</option>
        <option value="90">90 секунд</option>
        <option value="180">3 хвилини</option>
        <option value="0">Не вимикати</option>
      </select>
    </div>
    <div class="row">
      <label for="voiceEnabled">Озвучувати відповіді голосом</label>
      <input type="checkbox" id="voiceEnabled" checked onchange="savePrefs()">
    </div>
    <div class="row">
      <label for="quietAuto">Авто-тиша (вимк. вентилятори на 20с при діалозі)</label>
      <input type="checkbox" id="quietAuto" onchange="savePrefs()">
    </div>
    <div class="row">
      <label for="showSleepClock">Ледь помітний годинник у сні</label>
      <input type="checkbox" id="showSleepClock" checked onchange="savePrefs()">
    </div>

    <div class="help-list">
      <b>Голосові команди (починайте зі «Степан»):</b><br>
      • <b>Пробудження:</b> «Степан» (відкриває Температуру і Вологість)<br>
      • <b>Показати на екрані:</b> «Покажи температуру», «Покажи вологість», «Покажи CO2», «Покажи тиск», «Покажи теплоносій», «Покажи насос / витяжку / вентилятори», «Покажи напругу / енергію», «Покажи все», «Головний екран»<br>
      • <b>Сказати голосом:</b> «Яка температура?», «Скажи вологість», «Скільки CO2?», «Стан системи / звіт»<br>
      • <b>Керування:</b> «Режим авто», «Ручний режим», «Форсаж», «Еко увімкни / вимкни / авто», «Насос 50», «Вентилятор 40», «Витяжка 60», «Вимкни витяжку», «Встанови температуру 23», «Тиша»<br>
      • <b>Екран:</b> «Спати» / «Темний екран»
    </div>

    <div style="display:flex;gap:10px;justify-content:flex-end">
      <button class="chip" onclick="testSpeak()">🔊 Тест голосу</button>
      <button class="chip active" onclick="toggleSettings(false)">Закрити</button>
    </div>
  </div>
</div>

<script>
// ============================================================================
// СТАН ДОДАТКУ
// ============================================================================
let state = {
  started: false,
  sleeping: false,
  dialogActive: false,
  dialogTimer: null,
  sleepTimer: null,
  pendingConfirm: null, // { cmd, speakText, expireAt }
  viewKeys: ['temp', 'hum'], // Стартовий екран: температура та вологість
  data: null,
  energy: null,
  wakeLock: null,
  recognition: null,
  recognizing: false,
  speaking: false,
  prefs: {
    sleepSec: 45,
    voiceEnabled: true,
    quietAuto: false,
    showSleepClock: true
  }
};

const WAKE_WORDS = ['степан', 'степане', 'стьопа', 'стьопка', 'степа', 'stepan', 'стипан'];

// Перевірка HTTP для підказки Chrome
(function(){
  if (location.protocol !== 'https:' && location.hostname !== 'localhost' && location.hostname !== '127.0.0.1') {
    document.getElementById('httpWarn').style.display = 'block';
    document.getElementById('originHint').textContent = location.origin;
  }
  loadPrefs();
  updateClocks();
  setInterval(updateClocks, 1000);
})();

function loadPrefs() {
  try {
    const saved = JSON.parse(localStorage.getItem('stepan_prefs') || '{}');
    state.prefs = Object.assign(state.prefs, saved);
  } catch(e) {}
  document.getElementById('sleepSec').value = String(state.prefs.sleepSec);
  document.getElementById('voiceEnabled').checked = !!state.prefs.voiceEnabled;
  document.getElementById('quietAuto').checked = !!state.prefs.quietAuto;
  document.getElementById('showSleepClock').checked = !!state.prefs.showSleepClock;
  applySleepClockVisibility();
}

function savePrefs() {
  state.prefs.sleepSec = parseInt(document.getElementById('sleepSec').value, 10);
  state.prefs.voiceEnabled = document.getElementById('voiceEnabled').checked;
  state.prefs.quietAuto = document.getElementById('quietAuto').checked;
  state.prefs.showSleepClock = document.getElementById('showSleepClock').checked;
  localStorage.setItem('stepan_prefs', JSON.stringify(state.prefs));
  applySleepClockVisibility();
  resetSleepTimer();
}

function applySleepClockVisibility() {
  document.getElementById('sleepClock').style.display = state.prefs.showSleepClock ? 'block' : 'none';
}

function updateClocks() {
  const now = new Date();
  const hh = String(now.getHours()).padStart(2, '0');
  const mm = String(now.getMinutes()).padStart(2, '0');
  const str = hh + ':' + mm;
  document.getElementById('topClock').textContent = str;
  document.getElementById('sleepClock').textContent = str;
}

// ============================================================================
// ЗАПУСК, FULLSCREEN, WAKE LOCK
// ============================================================================
async function startStepan() {
  state.started = true;
  document.getElementById('startGate').style.display = 'none';

  // Повний екран
  try {
    if (document.documentElement.requestFullscreen) {
      await document.documentElement.requestFullscreen();
    }
  } catch(e) {}

  // Заборона апаратного блокування екрана
  requestWakeLock();
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') requestWakeLock();
  });

  // Перше завантаження даних + періодичне оновлення
  await fetchAllData();
  setInterval(fetchAllData, 4000);

  // Запуск розпізнавання мови
  initSpeechRecognition();
  resetSleepTimer();
  speak('Степан на зв\'язку. На екрані температура та вологість.');
}

async function requestWakeLock() {
  try {
    if ('wakeLock' in navigator) {
      state.wakeLock = await navigator.wakeLock.request('screen');
    }
  } catch(e) {}
}

// ============================================================================
// СОН ТА ПРОБУДЖЕННЯ
// ============================================================================
function resetSleepTimer() {
  clearTimeout(state.sleepTimer);
  if (state.prefs.sleepSec > 0 && !state.sleeping) {
    state.sleepTimer = setTimeout(() => {
      if (!state.dialogActive && !state.speaking) {
        goSleep();
      } else {
        resetSleepTimer();
      }
    }, state.prefs.sleepSec * 1000);
  }
}

function goSleep() {
  state.sleeping = true;
  state.dialogActive = false;
  clearTimeout(state.dialogTimer);
  clearTimeout(state.sleepTimer);
  document.getElementById('sleepOverlay').classList.add('active');
  updateMicUI();
}

function wakeUp(withDialog) {
  const wasSleeping = state.sleeping;
  state.sleeping = false;
  document.getElementById('sleepOverlay').classList.remove('active');
  if (wasSleeping) {
    setView(['temp', 'hum']);
  }
  if (withDialog) {
    activateDialogWindow();
  }
  resetSleepTimer();
}

function activateDialogWindow() {
  state.dialogActive = true;
  clearTimeout(state.dialogTimer);
  if (state.prefs.quietAuto) {
    fetch('/api/quiet?on=1&ttl=20').catch(()=>{});
  }
  state.dialogTimer = setTimeout(() => {
    state.dialogActive = false;
    state.pendingConfirm = null;
    updateMicUI();
    resetSleepTimer();
  }, 16000);
  updateMicUI();
}

function manualActivateDialog() {
  wakeUp(true);
  beep(660, 120);
  setReply('Слухаю команду...');
  if (!state.recognizing) startRecognitionSafe();
}

// ============================================================================
// ДАНІ З ESP32
// ============================================================================
async function fetchAllData() {
  try {
    const r = await fetch('/status', {cache: 'no-store'});
    if (r.ok) {
      state.data = await r.json();
      updateHeaderMode();
      renderCards();
    }
  } catch(e) {}

  // Якщо відкриті картки енергії або запитали енергію — підвантажуємо /energy/api
  if (state.viewKeys.includes('power')) {
    try {
      const r2 = await fetch('/energy/api', {cache: 'no-store'});
      if (r2.ok) {
        state.energy = await r2.json();
        renderCards();
      }
    } catch(e) {}
  }
}

function numValid(v) {
  return v !== null && v !== undefined && !isNaN(Number(v));
}

function fmt(v, dec = 1) {
  return numValid(v) ? Number(v).toFixed(dec) : '—';
}

function getMainTemp() {
  const d = state.data;
  if (!d) return null;
  if (numValid(d.tempRoom)) return Number(d.tempRoom);
  if (numValid(d.tempBME)) return Number(d.tempBME);
  if (numValid(d.co2Temp)) return Number(d.co2Temp);
  return null;
}

function getMainHum() {
  const d = state.data;
  if (!d) return null;
  if (numValid(d.humidity)) return Number(d.humidity);
  if (numValid(d.co2Humidity)) return Number(d.co2Humidity);
  return null;
}

function updateHeaderMode() {
  const d = state.data;
  const pill = document.getElementById('modePill');
  if (!d) return;
  const m = d.mode || 'АВТО';
  pill.textContent = d.ecoActive ? (m + ' • ЕКО') : m;
  pill.className = 'pill ' + (m === 'АВТО' ? 'mode-auto' : (m === 'АВАРІЯ' ? 'mode-err' : 'mode-manual'));
}

// ============================================================================
// ВІДОБРАЖЕННЯ КАРТОК
// ============================================================================
function setView(keys) {
  state.viewKeys = keys && keys.length ? keys : ['temp', 'hum'];
  document.querySelectorAll('.quick-chips .chip').forEach(c => c.classList.remove('active'));
  if (state.viewKeys.length === 2 && state.viewKeys[0] === 'temp' && state.viewKeys[1] === 'hum') {
    document.getElementById('chip-home').classList.add('active');
  } else if (state.viewKeys.length === 1 && state.viewKeys[0] === 'co2') {
    document.getElementById('chip-co2').classList.add('active');
  } else if (state.viewKeys.length >= 6) {
    document.getElementById('chip-all').classList.add('active');
  }
  if (state.viewKeys.includes('power') && !state.energy) {
    fetchAllData();
  }
  renderCards();
  resetSleepTimer();
}

function renderCards() {
  const grid = document.getElementById('cardsGrid');
  const d = state.data || {};
  const keys = state.viewKeys;

  grid.className = 'grid ' + (keys.length === 1 ? 'count-1' : (keys.length === 2 ? 'count-2' : 'count-many'));

  let html = '';
  for (const k of keys) {
    if (k === 'temp') {
      const t = getMainTemp();
      const subParts = [];
      if (numValid(d.tempBME)) subParts.push('BME: ' + fmt(d.tempBME) + '°C');
      if (numValid(d.tempCarrier)) subParts.push('Теплоносій: ' + fmt(d.tempCarrier) + '°C');
      html += makeCard('🌡️ Температура', fmt(t, 1), '°C', subParts.join(' • ') || 'Основний датчик', '#fb7185');
    }
    else if (k === 'hum') {
      const h = getMainHum();
      const sub = numValid(d.co2Humidity) ? ('SCD30: ' + fmt(d.co2Humidity, 0) + '%') : 'Відносна вологість';
      html += makeCard('💧 Вологість', fmt(h, 1), '%', sub, '#38bdf8');
    }
    else if (k === 'co2') {
      const c = numValid(d.co2Level) ? Math.round(d.co2Level) : '—';
      let note = 'Норма';
      let col = '#34d399';
      if (numValid(d.co2Level)) {
        if (d.co2Level > 1400) { note = 'Високий рівень!'; col = '#ef4444'; }
        else if (d.co2Level > 1000) { note = 'Помірно підвищений'; col = '#f59e0b'; }
      }
      html += makeCard('🌿 CO₂', c, 'ppm', note, col);
    }
    else if (k === 'pressure') {
      const p = numValid(d.pressure) ? Math.round(d.pressure) : '—';
      html += makeCard('🧭 Тиск', p, 'гПа', 'Атмосферний тиск', '#a78bfa');
    }
    else if (k === 'carrier') {
      html += makeCard('🔥 Теплоносій', fmt(d.tempCarrier, 1), '°C', 'Насос: ' + (d.pumpPower ?? 0) + '% • Вент: ' + (d.fanPower ?? 0) + '%', '#fb923c');
    }
    else if (k === 'actuators') {
      const txt = (d.pumpPower ?? 0) + '% / ' + (d.fanPower ?? 0) + '% / ' + (d.extractorPower ?? 0) + '%';
      const sub = 'Насос • Вентилятор • Витяжка' + (numValid(d.plantFanPower) ? (' | Обдув: ' + d.plantFanPower + '%') : '');
      html += makeCard('🌀 Приводи', txt, '', sub, '#38bdf8');
    }
    else if (k === 'power') {
      const e = state.energy || {};
      const volt = numValid(e.voltage) ? fmt(e.voltage, 0) : (numValid(d.lastVoltage) ? fmt(d.lastVoltage, 0) : '—');
      const pwr = numValid(e.power) ? (fmt(e.power, 0) + ' Вт') : '';
      const sub = (d.powerOutageDetected ? '⚠️ Блекаут! ' : '') + (d.ecoActive ? 'ЕКО активний ' : '') + pwr;
      html += makeCard('⚡ Живлення', volt, 'В', sub.trim() || 'Мережа 220В', d.powerOutageDetected ? '#ef4444' : '#f59e0b');
    }
  }
  grid.innerHTML = html;
}

function makeCard(label, val, unit, sub, color) {
  return `<div class="card" style="--c:${color}">
    <div class="card-label">${label}</div>
    <div class="card-val">${val}<span class="card-unit">${unit}</span></div>
    <div class="card-sub">${sub}</div>
  </div>`;
}

// ============================================================================
// РОЗПІЗНАВАННЯ МОВИ (Web Speech API)
// ============================================================================
function initSpeechRecognition() {
  const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (!SR) {
    setReply('❌ Браузер не підтримує розпізнавання мови (використайте Chrome)');
    document.getElementById('statusSub').textContent = 'Мікрофон недоступний';
    return;
  }

  const rec = new SR();
  rec.lang = 'uk-UA';
  rec.continuous = true;
  rec.interimResults = true;
  rec.maxAlternatives = 1;

  rec.onstart = () => {
    state.recognizing = true;
    updateMicUI();
  };

  rec.onend = () => {
    state.recognizing = false;
    updateMicUI();
    // Автоперезапуск, щоб Степан слухав постійно (крім моменту коли сам говорить)
    if (state.started && !state.speaking) {
      setTimeout(startRecognitionSafe, 350);
    }
  };

  rec.onerror = (ev) => {
    if (ev.error === 'not-allowed' || ev.error === 'service-not-allowed') {
      document.getElementById('statusSub').textContent = 'Немає дозволу на мікрофон!';
      setReply('Дозвольте мікрофон (див. підказку для http:// у Chrome)');
      return;
    }
    // Для no-speech / network / aborted просто перезапуститься в onend
  };

  rec.onresult = (event) => {
    let interim = '';
    let finalText = '';
    for (let i = event.resultIndex; i < event.results.length; i++) {
      const txt = event.results[i][0].transcript;
      if (event.results[i].isFinal) finalText += txt + ' ';
      else interim += txt;
    }

    const combined = (finalText || interim).trim();
    if (!combined) return;

    if (!state.sleeping) {
      document.getElementById('heardText').textContent = '🎙️ ' + combined;
    }

    const low = combined.toLowerCase();

    // Швидке пробудження ще на проміжному результаті, якщо сказали "Степан"
    if (state.sleeping && containsWakeWord(low)) {
      wakeUp(true);
      document.getElementById('heardText').textContent = '🎙️ ' + combined;
    }

    if (finalText.trim().length > 0) {
      handleSpokenPhrase(finalText.trim());
    }
  };

  state.recognition = rec;
  startRecognitionSafe();
}

function startRecognitionSafe() {
  if (!state.recognition || state.recognizing || state.speaking) return;
  try { state.recognition.start(); } catch(e) {}
}

function stopRecognitionSafe() {
  if (!state.recognition || !state.recognizing) return;
  try { state.recognition.stop(); } catch(e) {}
}

function containsWakeWord(low) {
  return WAKE_WORDS.some(w => low.includes(w));
}

function stripWakeWord(low) {
  let out = low;
  for (const w of WAKE_WORDS) {
    out = out.replace(new RegExp(w + '[,.!?:;\\s]*', 'gi'), ' ');
  }
  return out.replace(/\s+/g, ' ').trim();
}

function updateMicUI() {
  const av = document.getElementById('avatar');
  const btn = document.getElementById('micBtn');
  const sub = document.getElementById('statusSub');
  av.className = 'avatar';
  btn.className = 'mic-btn';

  if (state.speaking) {
    av.classList.add('speaking');
    sub.textContent = 'Говорить...';
  } else if (state.dialogActive) {
    av.classList.add('active-dialog');
    btn.classList.add('dialog');
    sub.textContent = 'Слухаю вашу команду...';
  } else if (state.recognizing) {
    av.classList.add('listening');
    btn.classList.add('on');
    sub.textContent = 'Слухаю кодове слово «Степан»';
  } else {
    sub.textContent = 'Мікрофон на паузі';
  }
}

// ============================================================================
// СИНТЕЗ МОВИ (TTS на Bluetooth-колонку планшета) + БІП
// ============================================================================
function beep(freq = 620, ms = 110) {
  try {
    const ctx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.value = freq;
    gain.gain.value = 0.08;
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    setTimeout(() => { osc.stop(); ctx.close(); }, ms);
  } catch(e) {}
}

function speak(text) {
  setReply(text);
  if (!state.prefs.voiceEnabled || !('speechSynthesis' in window)) return;

  try {
    state.speaking = true;
    stopRecognitionSafe(); // щоб мікрофон не слухав сам себе
    updateMicUI();

    window.speechSynthesis.cancel();
    const u = new SpeechSynthesisUtterance(text);
    u.lang = 'uk-UA';
    u.rate = 1.02;

    const voices = window.speechSynthesis.getVoices();
    const uk = voices.find(v => v.lang && v.lang.toLowerCase().startsWith('uk'));
    if (uk) u.voice = uk;

    u.onend = u.onerror = () => {
      state.speaking = false;
      updateMicUI();
      if (state.started) setTimeout(startRecognitionSafe, 250);
      resetSleepTimer();
    };
    window.speechSynthesis.speak(u);
  } catch(e) {
    state.speaking = false;
    startRecognitionSafe();
  }
}

function testSpeak() {
  speak('Привіт! Я Степан. Температура ' + fmt(getMainTemp(), 1) + ' градусів, вологість ' + fmt(getMainHum(), 0) + ' відсотків.');
}

function setReply(txt) {
  document.getElementById('replyText').textContent = txt;
}

// ============================================================================
// ОБРОБКА ГОЛОСОВИХ КОМАНД "СТЕПАНА"
// ============================================================================
function extractNumber(str) {
  const m = str.match(/(\d+(?:[.,]\d+)?)/);
  return m ? parseFloat(m[1].replace(',', '.')) : null;
}

async function sendEspCommand(cmd) {
  try {
    const body = new URLSearchParams();
    body.append('cmd', cmd);
    const r = await fetch('/command', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    const txt = await r.text();
    await fetchAllData();
    return { ok: r.ok, text: txt };
  } catch(e) {
    return { ok: false, text: 'Помилка зв\'язку з контролером' };
  }
}

async function handleSpokenPhrase(raw) {
  const low = raw.toLowerCase().trim();
  const hasWake = containsWakeWord(low);

  // Якщо не в діалозі і не сказали "Степан" — ігноруємо
  if (!hasWake && !state.dialogActive) return;

  // Прокидаємося і продовжуємо вікно діалогу
  wakeUp(true);

  const cmdText = hasWake ? stripWakeWord(low) : low;

  // Якщо сказали тільки "Степан"
  if (!cmdText) {
    setView(['temp', 'hum']);
    beep(680, 120);
    speak('Слухаю');
    return;
  }

  // 1. Підтвердження критичної команди ("Так" / "Ні")
  if (state.pendingConfirm && Date.now() < state.pendingConfirm.expireAt) {
    if (/(^|\s)(так|підтверджую|давай|роби|ок|добре)(\s|$)/.test(cmdText)) {
      const pending = state.pendingConfirm;
      state.pendingConfirm = null;
      const res = await sendEspCommand(pending.cmd);
      speak(res.ok ? pending.okSpeak : 'Не вдалося виконати команду');
      return;
    }
    if (/(^|\s)(ні|скасуй|відміна|не треба)(\s|$)/.test(cmdText)) {
      state.pendingConfirm = null;
      speak('Добре, скасовано');
      return;
    }
  }

  // 2. Сон / темний екран
  if (/(спати|вимкни екран|темний екран|відпочивай|на добраніч)/.test(cmdText)) {
    speak('Переходжу в режим очікування');
    setTimeout(goSleep, 1800);
    return;
  }

  // 3. Повернутися на головний екран
  if (/(головн|стартов|додому|основн)/.test(cmdText)) {
    setView(['temp', 'hum']);
    speak('Повернув головний екран');
    return;
  }

  // 4. Режим тиші
  if (/(тиша|тихо|режим тиші)/.test(cmdText)) {
    await fetch('/api/quiet?on=1&ttl=120').catch(()=>{});
    speak('Увімкнув режим тиші на дві хвилини');
    return;
  }

  // 5. Голосове керування режимами та пристроями
  if (/(автоматичн|режим авто|увімкни авто|включи авто|в авто)/.test(cmdText) && !cmdText.includes('еко')) {
    const res = await sendEspCommand('auto');
    speak(res.ok ? 'Перевів систему в автоматичний режим' : 'Помилка команди');
    return;
  }
  if (/(ручний режим|в ручний|увімкни ручний)/.test(cmdText)) {
    const res = await sendEspCommand('manual');
    speak(res.ok ? 'Увімкнув ручний режим' : 'Помилка команди');
    return;
  }
  if (/(форсаж)/.test(cmdText)) {
    const res = await sendEspCommand('force');
    speak(res.ok ? 'Увімкнув режим форсажу' : 'Помилка команди');
    return;
  }
  if (/(еко|економ)/.test(cmdText)) {
    if (/(вимк|викл|стоп|відкл)/.test(cmdText)) {
      const res = await sendEspCommand('eco off');
      speak(res.ok ? 'Еко режим вимкнено' : 'Помилка');
      return;
    }
    if (/(авто)/.test(cmdText)) {
      const res = await sendEspCommand('eco auto');
      speak(res.ok ? 'Еко режим переведено в авто' : 'Помилка');
      return;
    }
    if (/(увімк|вкл|актив)/.test(cmdText)) {
      const res = await sendEspCommand('eco on');
      speak(res.ok ? 'Еко режим увімкнено' : 'Помилка');
      return;
    }
  }

  // Встановлення температури / вологості (з підтвердженням!)
  if (/(встанови|постав|зроби|задай)/.test(cmdText) && /(температур)/.test(cmdText)) {
    const val = extractNumber(cmdText);
    if (val !== null && val >= 5 && val <= 40) {
      state.pendingConfirm = {
        cmd: 'temp ' + val,
        okSpeak: 'Температуру встановлено на ' + val + ' градусів',
        expireAt: Date.now() + 15000
      };
      speak('Встановити температуру ' + val + ' градусів? Скажіть так для підтвердження.');
      return;
    }
  }
  if (/(встанови|постав|зроби|задай)/.test(cmdText) && /(волог)/.test(cmdText)) {
    const val = extractNumber(cmdText);
    if (val !== null && val >= 20 && val <= 95) {
      state.pendingConfirm = {
        cmd: 'hum ' + val,
        okSpeak: 'Вологість встановлено на ' + val + ' відсотків',
        expireAt: Date.now() + 15000
      };
      speak('Встановити вологість ' + val + ' відсотків? Скажіть так для підтвердження.');
      return;
    }
  }

  // Керування насосом / вентилятором / витяжкою
  const devMap = [
    { rx: /(насос|помп)/, cmd: 'pump', name: 'Насос' },
    { rx: /(вентилятор|обігрів)/, cmd: 'fan', name: 'Вентилятор' },
    { rx: /(витяжк)/, cmd: 'extractor', name: 'Витяжку' }
  ];
  for (const dev of devMap) {
    if (dev.rx.test(cmdText) && /(вимк|викл|увімк|вкл|постав|на \d|\d+\s*%|\d+$)/.test(cmdText)) {
      let pct = extractNumber(cmdText);
      if (/(вимк|викл|стоп|нуль)/.test(cmdText)) pct = 0;
      else if (pct === null && /(увімк|вкл)/.test(cmdText)) pct = 50;
      if (pct !== null && pct >= 0 && pct <= 100) {
        pct = Math.round(pct);
        const res = await sendEspCommand(dev.cmd + ' ' + pct);
        setView(['actuators']);
        speak(res.ok ? (pct === 0 ? (dev.name + ' вимкнено') : (dev.name + ' встановлено на ' + pct + ' відсотків')) : 'Помилка команди');
        return;
      }
    }
  }

  // 6. Показати / Сказати параметри (можна кілька в одній фразі!)
  const asked = [];
  if (/(температур|тепло|градус)/.test(cmdText) && !/(теплоносі)/.test(cmdText)) asked.push('temp');
  if (/(волог)/.test(cmdText)) asked.push('hum');
  if (/(co2|цо2|цо 2|це о два|вуглекисл|повітря)/.test(cmdText)) asked.push('co2');
  if (/(тиск)/.test(cmdText)) asked.push('pressure');
  if (/(теплоносі|батаре|радіатор|котел)/.test(cmdText)) asked.push('carrier');
  if (/(насос|вентилятор|витяжк|привод|обдув|вітер)/.test(cmdText)) asked.push('actuators');
  if (/(напруг|енерг|електрик|світл|вольт|блекаут|живлен|еко)/.test(cmdText)) asked.push('power');

  const wantAll = /(все|всі параметр|повний звіт|стан систем|звіт|що там|як справи)/.test(cmdText);
  const wantSay = /(скажи|скільки|яка|який|які|звіт|стан|що там|як справи)/.test(cmdText);
  const wantShow = /(покажи|виведи|відкрий|екран)/.test(cmdText);

  if (wantAll) {
    setView(['temp', 'hum', 'co2', 'pressure', 'carrier', 'actuators', 'power']);
    await fetchAllData();
    const d = state.data || {};
    const parts = [];
    if (numValid(getMainTemp())) parts.push('Температура ' + fmt(getMainTemp(), 1) + ' градусів');
    if (numValid(getMainHum())) parts.push('вологість ' + fmt(getMainHum(), 0) + ' відсотків');
    if (numValid(d.co2Level)) parts.push('CO2 ' + Math.round(d.co2Level) + ' ppm');
    parts.push('режим ' + (d.mode || 'авто'));
    speak(parts.join(', ') + '.');
    return;
  }

  if (asked.length > 0) {
    setView(asked);
    if (asked.includes('power')) await fetchAllData();

    // Формуємо голосову відповідь (озвучуємо завжди, або коротко коли просто "покажи")
    const d = state.data || {};
    const e = state.energy || {};
    const speech = [];
    for (const k of asked) {
      if (k === 'temp') speech.push('Температура ' + (numValid(getMainTemp()) ? fmt(getMainTemp(), 1) + ' градусів' : 'недоступна'));
      if (k === 'hum') speech.push('Вологість ' + (numValid(getMainHum()) ? fmt(getMainHum(), 0) + ' відсотків' : 'недоступна'));
      if (k === 'co2') speech.push('Рівень CO2 ' + (numValid(d.co2Level) ? Math.round(d.co2Level) + ' ppm' : 'недоступний'));
      if (k === 'pressure') speech.push('Тиск ' + (numValid(d.pressure) ? Math.round(d.pressure) + ' гектопаскалів' : 'недоступний'));
      if (k === 'carrier') speech.push('Теплоносій ' + (numValid(d.tempCarrier) ? fmt(d.tempCarrier, 1) + ' градусів' : 'недоступний'));
      if (k === 'actuators') speech.push('Насос ' + (d.pumpPower ?? 0) + ', вентилятор ' + (d.fanPower ?? 0) + ', витяжка ' + (d.extractorPower ?? 0) + ' відсотків');
      if (k === 'power') {
        const v = numValid(e.voltage) ? Math.round(e.voltage) : (numValid(d.lastVoltage) ? Math.round(d.lastVoltage) : null);
        speech.push('Напруга ' + (v !== null ? v + ' вольт' : 'недоступна'));
      }
    }
    if (wantSay || !wantShow) {
      speak(speech.join('. '));
    } else {
      speak('Показую. ' + speech.join('. '));
    }
    return;
  }

  // Якщо не розпізнано команду
  setReply('Не зрозумів: «' + cmdText + '». Спробуйте: «Покажи CO2» або «Яка температура?»');
}

function toggleSettings(open) {
  document.getElementById('settingsModal').classList.toggle('open', open);
  resetSleepTimer();
}
</script>
</body>
</html>
)rawliteral";

void handleTabletPage() {
    if (!checkAuth()) return;
    server.send_P(200, "text/html; charset=utf-8", TABLET_HTML);
}
