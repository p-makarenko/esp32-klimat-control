// ============================================================================
// WEB_PAGES_HISTORY.CPP - Сторінки історії та графіків
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Історія даних (handleHistoryPage, handleHistoryData, handleHistoryStats, handleHistoryExport)
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <memory>
#include "web_common.h"
#include "global_declarations.h"
#include "data_logger.h"
#include "google_sheets_sync.h"

extern WebServer server;

// Forward declarations
extern bool checkAuth();
extern String getUkraineMarquee();
extern LoggerStats getLoggerStats();
extern bool readRAMDataChunk(DataRecord* buffer, uint16_t offset, uint16_t count);
extern bool readSPIFFSData(const char* startDate, const char* endDate, String& outJson);
extern bool readSPIFFSDataCSV(const char* startDate, const char* endDate, String& outCSV);
extern String exportToJSON(const char* startDate, const char* endDate);
extern String exportToCSV(const char* startDate, const char* endDate);

// ============================================================================
// СТОРІНКА ІСТОРІЇ ТА ГРАФІКІВ
// ============================================================================

void handleHistoryPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) return;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");

    String html;
    html.reserve(4096);
    auto flushChunk = [&]() {
        if (html.length() > 0) {
            esp_task_wdt_reset();
            server.sendContent(html);
            html = "";
            yield();
        }
    };

    html = getHtmlHead("📈 Історія даних", true); // true = includeChartJS
    flushChunk();

    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("📈 ІСТОРІЯ ТЕМПЕРАТУРИ І ВОЛОГОСТІ");
    flushChunk();

    // Статистика
    LoggerStats stats = getLoggerStats();
    uint16_t bufCap = getRamBufferCapacity();
    const char* memLabel = isRamBufferInPsram() ? "PSRAM" : "RAM";
    uint16_t storedRam = (stats.totalRecordsRAM > bufCap) ? bufCap : (uint16_t)stats.totalRecordsRAM;
    html += "<div class='info' id='statsInfo' style='text-align: center; color: var(--m); margin-bottom: 20px; font-size: 0.9em;'>";
    html += "Записів в " + String(memLabel) + ": " + String(storedRam) + " / " + String(bufCap);
    if (bufCap >= 1440) {
        html += " (" + String(bufCap / 1440) + " дн.)";
    }
    html += " | Записів в SPIFFS: " + String(stats.totalRecordsSPIFFS);
    html += " | Оновлення кожну хвилину";
    html += "</div>";

    // Панель навігації по періодах та вибору джерела
    html += "<div class='card' style='border-left-color: #667eea;'>";
    html += "<strong>📅 Період відображення:</strong><br>";
    html += "<div style='display: flex; gap: 8px; margin: 10px 0 15px 0; flex-wrap: wrap;'>";
    html += "<button class='tab-btn active' id='tabDay' onclick='selectPeriod(\"day\")'>📅 День</button>";
    html += "<button class='tab-btn' id='tabWeek' onclick='selectPeriod(\"week\")'>📅 Тиждень</button>";
    html += "<button class='tab-btn' id='tabMonth' onclick='selectPeriod(\"month\")'>📅 Місяць</button>";
    html += "<button class='tab-btn' id='tabYear' onclick='selectPeriod(\"year\")'>📅 Рік</button>";
    html += "<button class='tab-btn' id='tabCustom' onclick='selectPeriod(\"custom\")'>📆 Свій період</button>";
    html += "</div>";

    html += "<div style='display: flex; flex-wrap: wrap; gap: 15px; align-items: center; justify-content: space-between; padding-top: 10px; border-top: 1px solid #eee;'>";
    html += "<div><strong>📁 Джерело даних:</strong> ";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='ram' checked onchange='switchDataSource()'> 📊 " + String(memLabel) + " (до " + String(bufCap >= 1440 ? (String(bufCap / 1440) + " дн.") : (String(bufCap / 60) + " год.")) + ")</label>";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='spiffs' onchange='switchDataSource()'> 💾 Пристрій (архів)</label>";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='sheets' onchange='switchDataSource()'> ☁️ Google Sheets</label>";
    html += "</div>";
    html += "<span id='loadStatus' style='color: var(--m); font-size: 0.9em; font-weight: 500;'></span>";
    html += "</div>";

    html += "<div id='dateSelector' style='display: none; margin-top: 15px; padding-top: 12px; border-top: 1px solid #eee;'>";
    html += "<div style='display: flex; flex-wrap: wrap; gap: 10px; align-items: center;'>";
    html += "<label>Від: <input type='date' id='startDate' onchange='loadDataForCurrentSelection()' style='padding: 8px; border-radius: 4px; border: 1px solid var(--b);'></label>";
    html += "<label>До: <input type='date' id='endDate' onchange='loadDataForCurrentSelection()' style='padding: 8px; border-radius: 4px; border: 1px solid var(--b);'></label>";
    html += "<button id='loadBtn' onclick='loadDataForCurrentSelection()' class='btn' style='margin: 0; padding: 8px 16px;'>🔄 Оновити</button>";
    html += "</div>";
    html += "</div>";
    html += "</div>";

    // Інструкції для масштабування
    html += "<div class='card' style='border-left-color: #2196F3;'>";
    html += "<strong>📊 Керування графіками:</strong><br>";
    html += "<span style='display: inline-block; margin-top: 5px;'>🖱️ <strong>Масштабування:</strong> прокрутка коліщатком миші / pinch на сенсорному екрані</span><br>";
    html += "<span style='display: inline-block;'>👆 <strong>Переміщення:</strong> клік і перетягування графіка</span><br>";
    html += "<span style='display: inline-block;'>🔄 <strong>Скидання:</strong> подвійний клік по графіку або кнопка нижче</span>";
    html += "</div>";

    // Додаткові стилі для мобільних графіків
    html += "<style>";
    html += ".chart-wrapper { position: relative; margin: 20px 0; }";
    html += ".chart-header { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px; margin-bottom: 10px; }";
    html += ".chart-header h2 { margin: 0; color: #667eea; font-size: 1.3em; }";
    html += ".reset-btn { padding: 8px 16px; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; }";
    html += ".reset-btn:hover { background: #45a049; }";

    // Мобільна оптимізація графіків
    html += "@media (max-width: 768px) {";
    html += "  .chart-container { height: 250px !important; touch-action: none; }";
    html += "  .chart-header h2 { font-size: 1.1em; }";
    html += "  .reset-btn { padding: 10px 14px; font-size: 13px; }";
    html += "  #dateSelector { display: flex; flex-direction: column; gap: 10px; }";
    html += "  #dateSelector label { display: block; }";
    html += "  #dateSelector input[type='date'] { width: 100%; padding: 12px; font-size: 16px; }";
    html += "}";
    html += "@media (max-width: 480px) {";
    html += "  .chart-container { height: 200px !important; }";
    html += "  .chart-header { flex-direction: column; align-items: flex-start; }";
    html += "  .reset-btn { width: 100%; }";
    html += "}";
    html += "</style>";

    // Графік температури
    html += "<div class='chart-wrapper'>";
    html += "<div class='chart-header'>";
    html += "<h2>🌡️ Температура</h2>";
    html += "<button onclick='tempChart.resetZoom()' class='reset-btn'>🔄 Скинути масштаб</button>";
    html += "</div>";
    html += "<div class='chart-container'><canvas id='tempChart'></canvas></div>";
    html += "</div>";

    // Графік вологості
    html += "<div class='chart-wrapper'>";
    html += "<div class='chart-header'>";
    html += "<h2>💧 Вологість</h2>";
    html += "<button onclick='humChart.resetZoom()' class='reset-btn'>🔄 Скинути масштаб</button>";
    html += "</div>";
    html += "<div class='chart-container'><canvas id='humChart'></canvas></div>";
    html += "</div>";

    // Графік CO2
    html += "<div class='chart-wrapper'>";
    html += "<div class='chart-header'>";
    html += "<h2>💨 CO2 (ppm)</h2>";
    html += "<button onclick='co2Chart.resetZoom()' class='reset-btn'>🔄 Скинути масштаб</button>";
    html += "</div>";
    html += "<div class='chart-container'><canvas id='co2Chart'></canvas></div>";
    html += "</div>";

    html += "</div>"; // container
    flushChunk();

    // JavaScript для графіків
    html += "<script>";
    html += "let tempChart, humChart, co2Chart;";
    html += "window.chartFullDates = [];";
    html += "let currentPeriod = 'day';";
    html += "const SHEETS_URL = '" + String(GOOGLE_SCRIPT_URL) + "';";

    // Конфігурація zoom
    html += "const zoomConfig = {";
    html += "  zoom: {";
    html += "    wheel: { enabled: true, speed: 0.1 },";
    html += "    pinch: { enabled: true },";
    html += "    drag: {";
    html += "      enabled: true,";
    html += "      backgroundColor: 'rgba(66, 133, 244, 0.2)',";
    html += "      borderColor: 'rgba(66, 133, 244, 0.8)',";
    html += "      borderWidth: 1,";
    html += "      threshold: 10";
    html += "    },";
    html += "    mode: 'x'";
    html += "  },";
    html += "  pan: {";
    html += "    enabled: true,";
    html += "    mode: 'x',";
    html += "    threshold: 5,";
    html += "    modifierKey: null";
    html += "  }";
    html += "};";

    // Ініціалізація порожніх графіків
    html += "function initCharts() {";
    html += "tempChart = new Chart(document.getElementById('tempChart'), {";
    html += "type: 'line', data: { labels: [], datasets: [";
    html += "{ label: '🔥 Теплоносій', data: [], borderColor: '#e74c3c', backgroundColor: 'rgba(231, 76, 60, 0.1)', borderWidth: 2, pointRadius: 0, tension: 0.4 },";
    html += "{ label: '🏠 Кімната', data: [], borderColor: '#3498db', backgroundColor: 'rgba(52, 152, 219, 0.1)', borderWidth: 2, pointRadius: 0, tension: 0.4 },";
    html += "{ label: '🌡️ BME280', data: [], borderColor: '#2ecc71', backgroundColor: 'rgba(46, 204, 113, 0.1)', borderWidth: 2, pointRadius: 0, tension: 0.4 }";
    html += "]}, options: {";
    html += "responsive: true, maintainAspectRatio: false, interaction: { mode: 'index', intersect: false },";
    html += "plugins: { legend: { display: true, position: 'top' }, tooltip: { callbacks: {";
    html += "title: function(ctx) { return (window.chartFullDates && window.chartFullDates[ctx[0].dataIndex]) ? window.chartFullDates[ctx[0].dataIndex] : ctx[0].label; },";
    html += "label: function(ctx) { return ctx.dataset.label + ': ' + (ctx.parsed.y !== null ? ctx.parsed.y.toFixed(1) : '0.0') + '°C'; }";
    html += "}}, zoom: zoomConfig },";
    html += "scales: { y: { beginAtZero: false, title: { display: true, text: '°C' } } }";
    html += "}});";

    html += "humChart = new Chart(document.getElementById('humChart'), {";
    html += "type: 'line', data: { labels: [], datasets: [";
    html += "{ label: '💧 Вологість', data: [], borderColor: '#3498db', backgroundColor: 'rgba(52, 152, 219, 0.2)', fill: true, borderWidth: 2, pointRadius: 0, tension: 0.4 }";
    html += "]}, options: {";
    html += "responsive: true, maintainAspectRatio: false, interaction: { mode: 'index', intersect: false },";
    html += "plugins: { legend: { display: true, position: 'top' }, tooltip: { callbacks: {";
    html += "title: function(ctx) { return (window.chartFullDates && window.chartFullDates[ctx[0].dataIndex]) ? window.chartFullDates[ctx[0].dataIndex] : ctx[0].label; },";
    html += "label: function(ctx) { return (ctx.parsed.y !== null ? ctx.parsed.y.toFixed(1) : '0.0') + '%'; }";
    html += "}}, zoom: zoomConfig },";
    html += "scales: { y: { beginAtZero: false, max: 100, title: { display: true, text: '%' } } }";
    html += "}});";

    html += "co2Chart = new Chart(document.getElementById('co2Chart'), {";
    html += "type: 'line', data: { labels: [], datasets: [";
    html += "{ label: '💨 CO2 (ppm)', data: [], borderColor: '#e67e22', backgroundColor: 'rgba(230, 126, 34, 0.1)', fill: true, borderWidth: 2, pointRadius: 0, tension: 0.4 }";
    html += "]}, options: {";
    html += "responsive: true, maintainAspectRatio: false, interaction: { mode: 'index', intersect: false },";
    html += "plugins: { legend: { display: true, position: 'top' }, tooltip: { callbacks: {";
    html += "title: function(ctx) { return (window.chartFullDates && window.chartFullDates[ctx[0].dataIndex]) ? window.chartFullDates[ctx[0].dataIndex] : ctx[0].label; },";
    html += "label: function(ctx) { return (ctx.parsed.y !== null ? ctx.parsed.y.toFixed(0) : '0') + ' ppm'; }";
    html += "}}, zoom: zoomConfig },";
    html += "scales: { y: { beginAtZero: false, title: { display: true, text: 'ppm' } } }";
    html += "}});";

    html += "document.getElementById('tempChart').ondblclick = function() { tempChart.resetZoom(); };";
    html += "document.getElementById('humChart').ondblclick = function() { humChart.resetZoom(); };";
    html += "document.getElementById('co2Chart').ondblclick = function() { co2Chart.resetZoom(); };";
    html += "}";

    html += "function updateCharts(labels, tempCarrier, tempRoom, tempBME, humidity, co2Level) {";
    html += "tempChart.data.labels = labels;";
    html += "tempChart.data.datasets[0].data = tempCarrier;";
    html += "tempChart.data.datasets[1].data = tempRoom;";
    html += "tempChart.data.datasets[2].data = tempBME;";
    html += "tempChart.update();";
    html += "humChart.data.labels = labels;";
    html += "humChart.data.datasets[0].data = humidity;";
    html += "humChart.update();";
    html += "co2Chart.data.labels = labels;";
    html += "co2Chart.data.datasets[0].data = co2Level;";
    html += "co2Chart.update();";
    html += "}";

    // Функція вибору періоду
    html += "function selectPeriod(period) {";
    html += "currentPeriod = period;";
    html += "document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));";
    html += "const tabBtn = document.getElementById('tab' + period.charAt(0).toUpperCase() + period.slice(1));";
    html += "if (tabBtn) tabBtn.classList.add('active');";
    html += "const now = new Date();";
    html += "const year = now.getFullYear();";
    html += "const month = String(now.getMonth() + 1).padStart(2, '0');";
    html += "const day = String(now.getDate()).padStart(2, '0');";
    html += "const today = year + '-' + month + '-' + day;";
    html += "let start = today;";
    html += "if (period === 'day') {";
    html += "document.getElementById('startDate').value = today;";
    html += "document.getElementById('endDate').value = today;";
    html += "document.getElementById('dateSelector').style.display = 'none';";
    html += "const src = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "if (src === 'ram') { loadRAMData(); return; }";
    html += "else if (src === 'spiffs') { loadArchiveData(today, today); return; }";
    html += "} else if (period === 'week') {";
    html += "const d = new Date(Date.now() - 7*24*60*60*1000);";
    html += "start = d.getFullYear() + '-' + String(d.getMonth()+1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');";
    html += "} else if (period === 'month') {";
    html += "const d = new Date(Date.now() - 30*24*60*60*1000);";
    html += "start = d.getFullYear() + '-' + String(d.getMonth()+1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');";
    html += "} else if (period === 'year') {";
    html += "const d = new Date(Date.now() - 365*24*60*60*1000);";
    html += "start = d.getFullYear() + '-' + String(d.getMonth()+1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');";
    html += "if (document.querySelector('input[name=\"dataSource\"]:checked').value === 'ram') {";
    html += "document.querySelector('input[name=\"dataSource\"][value=\"sheets\"]').checked = true;";
    html += "}";
    html += "} else if (period === 'custom') {";
    html += "document.getElementById('dateSelector').style.display = 'block';";
    html += "if (!document.getElementById('startDate').value) document.getElementById('startDate').value = today;";
    html += "if (!document.getElementById('endDate').value) document.getElementById('endDate').value = today;";
    html += "loadDataForCurrentSelection();";
    html += "return;";
    html += "}";
    html += "document.getElementById('startDate').value = start;";
    html += "document.getElementById('endDate').value = today;";
    html += "document.getElementById('dateSelector').style.display = 'block';";
    html += "loadDataForCurrentSelection();";
    html += "}";

    // Перемикання джерела даних (автозавантаження без додаткових кліків)
    html += "function switchDataSource() {";
    html += "const source = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "const dateSelector = document.getElementById('dateSelector');";
    html += "const now = new Date();";
    html += "const today = now.getFullYear() + '-' + String(now.getMonth()+1).padStart(2, '0') + '-' + String(now.getDate()).padStart(2, '0');";
    html += "if (!document.getElementById('startDate').value) document.getElementById('startDate').value = today;";
    html += "if (!document.getElementById('endDate').value) document.getElementById('endDate').value = today;";
    html += "dateSelector.style.display = (source === 'ram' && currentPeriod === 'day') ? 'none' : 'block';";
    html += "loadDataForCurrentSelection();";
    html += "}";

    // Завантаження за поточним вибором
    html += "function loadDataForCurrentSelection() {";
    html += "const source = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "const startDate = document.getElementById('startDate').value;";
    html += "const endDate = document.getElementById('endDate').value;";
    html += "if (source === 'ram') { loadRAMData(startDate, endDate); }";
    html += "else if (source === 'sheets') { loadSheetsData(startDate, endDate); }";
    html += "else { loadArchiveData(startDate, endDate); }";
    html += "}";

    // Завантаження RAM/PSRAM (з фільтром періоду та fallback на SPIFFS за сьогодні, якщо буфер ще порожній після ребута)
    html += "function loadRAMData(startDate, endDate) {";
    html += "const status = document.getElementById('loadStatus');";
    html += "if (status) status.textContent = '⏳ Оновлення...';";
    html += "let url = '/history/data?source=ram&format=json&maxPoints=1440';";
    html += "if (currentPeriod === 'day') { url += '&hours=24'; }";
    html += "else if (startDate && endDate) { url += '&start=' + encodeURIComponent(startDate) + '&end=' + encodeURIComponent(endDate); }";
    html += "fetch(url)";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "let data = result.data || [];";
    html += "data.sort((a, b) => a.timestamp - b.timestamp);";
    html += "if (data.length === 0) {";
    html += "  const now = new Date();";
    html += "  const today = now.getFullYear() + '-' + String(now.getMonth()+1).padStart(2, '0') + '-' + String(now.getDate()).padStart(2, '0');";
    html += "  fetch('/history/data?source=spiffs&format=json&start=' + today + '&end=' + today)";
    html += "    .then(r => r.json())";
    html += "    .then(spRes => {";
    html += "      let spData = spRes.data || [];";
    html += "      spData.sort((a, b) => a.timestamp - b.timestamp);";
    html += "      if (spData.length > 0) {";
    html += "        renderChartRecords(spData, 'Пристрій • RAM набирається після ребута');";
    html += "      } else {";
    html += "        if (status) status.textContent = '⚠️ Немає даних в RAM (перший запис через 1 хв)';";
    html += "      }";
    html += "    })";
    html += "    .catch(() => { if (status) status.textContent = '⚠️ Немає даних в RAM'; });";
    html += "  return;";
    html += "}";
    html += "renderChartRecords(data, 'RAM');";
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка:', error);";
    html += "if (status) status.textContent = '❌ Помилка RAM';";
    html += "});";
    html += "}";
    flushChunk();

    // Завантаження архіву SPIFFS (з fallback на RAM якщо SPIFFS ще порожній)
    html += "function loadArchiveData(startDate, endDate) {";
    html += "const status = document.getElementById('loadStatus');";
    html += "const btn = document.getElementById('loadBtn');";
    html += "if (btn) btn.disabled = true;";
    html += "if (status) status.textContent = '⏳ Завантаження даних...';";
    html += "let url = '/history/data?source=spiffs&format=json';";
    html += "if (startDate && endDate) { url += `&start=${startDate}&end=${endDate}`; }";
    html += "fetch(url)";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "let data = result.data || [];";
    html += "data.sort((a, b) => a.timestamp - b.timestamp);";
    html += "if (data.length === 0) {";
    html += "  fetch('/history/data?source=ram&format=json')";
    html += "    .then(r => r.json())";
    html += "    .then(ramRes => {";
    html += "      let ramData = ramRes.data || [];";
    html += "      ramData.sort((a, b) => a.timestamp - b.timestamp);";
    html += "      if (ramData.length === 0) {";
    html += "        if (status) status.textContent = '⚠️ Немає даних за період';";
    html += "        if (btn) btn.disabled = false;";
    html += "        return;";
    html += "      }";
    html += "      renderChartRecords(ramData, 'RAM (останній період)');";
    html += "      if (btn) btn.disabled = false;";
    html += "    })";
    html += "    .catch(() => {";
    html += "      if (status) status.textContent = '⚠️ Немає даних за період';";
    html += "      if (btn) btn.disabled = false;";
    html += "    });";
    html += "  return;";
    html += "}";
    html += "renderChartRecords(data, 'Пристрій');";
    html += "if (btn) btn.disabled = false;";
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка:', error);";
    html += "if (status) status.textContent = '❌ Помилка читання даних';";
    html += "if (btn) btn.disabled = false;";
    html += "});";
    html += "}";

    // Розумне формування точок графіків (УСУВАЄ КОНФЛІКТ TIMESTAMP І ВІДНОСНОГО ЧАСУ)
    html += "function renderChartRecords(records, srcName) {";
    html += "const status = document.getElementById('loadStatus');";
    html += "const labels = [], fullDates = [], tc = [], tr = [], tb = [], hum = [], co2 = [];";
    html += "let lastValidTs = Math.floor(Date.now() / 1000);";
    html += "for (let i = records.length - 1; i >= 0; i--) {";
    html += "  if (records[i].timestamp > 1000000000) {";
    html += "    lastValidTs = records[i].timestamp;";
    html += "    break;";
    html += "  }";
    html += "}";
    html += "records.forEach((record, index) => {";
    html += "  let effectiveTs = record.timestamp;";
    html += "  if (effectiveTs <= 1000000000) {";
    html += "    const offsetSec = (records.length - 1 - index) * 60;";
    html += "    effectiveTs = lastValidTs - offsetSec;";
    html += "  }";
    html += "  const date = new Date(effectiveTs * 1000);";
    html += "  const day = String(date.getDate()).padStart(2, '0');";
    html += "  const month = String(date.getMonth() + 1).padStart(2, '0');";
    html += "  const year = date.getFullYear();";
    html += "  const hours = String(date.getHours()).padStart(2, '0');";
    html += "  const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "  const shortLabel = hours + ':' + minutes;";
    html += "  const dateStr = day + '.' + month + '.' + year + ' ' + shortLabel;";
    html += "  labels.push(shortLabel);";
    html += "  fullDates.push(dateStr);";
    html += "  tc.push(Number.isNaN(record.tempCarrier) ? 0 : record.tempCarrier);";
    html += "  tr.push(Number.isNaN(record.tempRoom) ? 0 : record.tempRoom);";
    html += "  tb.push(Number.isNaN(record.tempBME) ? 0 : record.tempBME);";
    html += "  hum.push(Number.isNaN(record.humidity) ? 0 : record.humidity);";
    html += "  co2.push(Number.isNaN(record.co2Level) ? 0 : record.co2Level);";
    html += "});";
    html += "window.chartFullDates = fullDates;";
    html += "updateCharts(labels, tc, tr, tb, hum, co2);";
    html += "if (status) status.textContent = '✅ ' + records.length + ' записів (' + srcName + ')';";
    html += "}";

    // Завантаження Google Sheets
    html += "function loadSheetsData(startDate, endDate) {";
    html += "const status = document.getElementById('loadStatus');";
    html += "const btn = document.getElementById('loadBtn');";
    html += "if (!startDate || !endDate) { alert('Оберіть дати!'); return; }";
    html += "if (btn) btn.disabled = true;";
    html += "if (status) status.textContent = '⏳ Завантаження Sheets...';";
    html += "const controller = new AbortController();";
    html += "const timeoutId = setTimeout(() => controller.abort(), 35000);";
    html += "const url = SHEETS_URL + '?action=read&from=' + startDate + '&to=' + endDate;";
    html += "fetch(url, { signal: controller.signal })";
    html += ".then(r => { clearTimeout(timeoutId); if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })";
    html += ".then(text => {";
    html += "let rows;";
    html += "try { rows = JSON.parse(text); } catch(e) { ";
    html += "  if (text.indexOf('<html') >= 0 || text.indexOf('<!DOCTYPE') >= 0) {";
    html += "    throw new Error('Google Script вимагає авторизації (доступ: Anyone)');";
    html += "  }";
    html += "  throw new Error('Відповідь: ' + text.substring(0,60));";
    html += "}";
    html += "if (!Array.isArray(rows) || rows.length === 0) {";
    html += "if (status) status.textContent = '⚠️ Немає даних за період';";
    html += "return;";
    html += "}";
    html += "rows.sort((a, b) => (a[0] > b[0] ? 1 : (a[0] < b[0] ? -1 : 0)));";
    html += "const dedupMap = {}; rows.forEach(r => { if (r[0]) dedupMap[String(r[0]).trim()] = r; });";
    html += "rows = Object.keys(dedupMap).sort().map(k => dedupMap[k]);";
    html += "const labels=[], fullDates=[], tc=[], tr=[], tb=[], hum=[], co2=[];";
    html += "rows.forEach(r => {";
    html += "if (!r[0]) return;";
    html += "const dt = String(r[0]);";
    html += "labels.push(dt.substring(5,16));";
    html += "fullDates.push(dt);";
    html += "tc.push(parseFloat(String(r[1]).replace(',', '.'))||0);";
    html += "tr.push(parseFloat(String(r[2]).replace(',', '.'))||0);";
    html += "tb.push(parseFloat(String(r[3]).replace(',', '.'))||0);";
    html += "hum.push(parseFloat(String(r[4]).replace(',', '.'))||0);";
    html += "co2.push(parseFloat(String(r[5]).replace(',', '.'))||0);";
    html += "});";
    html += "window.chartFullDates = fullDates;";
    html += "updateCharts(labels, tc, tr, tb, hum, co2);";
    html += "if (status) status.textContent = '✅ ' + rows.length + ' записів (Sheets)';";
    html += "})";
    html += ".catch(err => {";
    html += "clearTimeout(timeoutId);";
    html += "if (err.name === 'AbortError') {";
    html += "  if (status) status.textContent = '❌ Таймаут Google Sheets (перевірте зв\\'язок)';";
    html += "} else {";
    html += "  if (status) status.textContent = '❌ ' + err.message;";
    html += "}";
    html += "})";
    html += ".finally(() => {";
    html += "if (btn) btn.disabled = false;";
    html += "});";
    html += "}";

    // Старт: ініціалізуємо графіки та відразу завантажуємо поточні дані
    html += "initCharts();";
    html += "selectPeriod('day');";

    html += "</script>";
    flushChunk();

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    flushChunk();
    server.sendContent("");
}

// ============================================================================
// ПОТОКОВЕ ЧИТАННЯ SPIFFS БЕЗ ВЕЛИКИХ АЛОКАЦІЙ RAM / JSONDOCUMENT
// ============================================================================

static void streamSPIFFSFileRecords(File& file, bool asCsv,
                                    unsigned long startTs, unsigned long endTs,
                                    bool& firstJsonRecord, String& outBuf) {
    char lineBuf[256];
    size_t idx = 0;
    uint16_t lines = 0;

    auto processLine = [&]() {
        if (idx == 0) return;
        lineBuf[idx] = '\0';
        idx = 0;

        unsigned long ts = strtoul(lineBuf, NULL, 10);
        if (ts == 0) return;
        bool inRange = (startTs == 0) || (ts >= startTs && ts <= endTs);
        if (!inRange) return;

        if (asCsv) {
            outBuf += lineBuf;
            outBuf += "\n";
        } else {
            char* savePtr = nullptr;
            char* f0 = strtok_r(lineBuf, ",", &savePtr);
            char* f1 = strtok_r(nullptr, ",", &savePtr);
            char* f2 = strtok_r(nullptr, ",", &savePtr);
            char* f3 = strtok_r(nullptr, ",", &savePtr);
            char* f4 = strtok_r(nullptr, ",", &savePtr);
            char* f5 = strtok_r(nullptr, ",", &savePtr);
            char* f6 = strtok_r(nullptr, ",", &savePtr);
            char* f7 = strtok_r(nullptr, ",", &savePtr);
            char* f8 = strtok_r(nullptr, ",", &savePtr);
            char* f9 = strtok_r(nullptr, ",", &savePtr);
            if (!f0 || !f1 || !f2 || !f3 || !f4) return;

            if (!firstJsonRecord) outBuf += ",";
            outBuf += "{\"timestamp\":";
            outBuf += f0;
            outBuf += ",\"tempCarrier\":";
            outBuf += String(atof(f1), 1);
            outBuf += ",\"tempRoom\":";
            outBuf += String(atof(f2), 1);
            outBuf += ",\"tempBME\":";
            outBuf += String(atof(f3), 1);
            outBuf += ",\"humidity\":";
            outBuf += String(atof(f4), 1);
            outBuf += ",\"co2Level\":";
            outBuf += String(f5 ? atoi(f5) : 0);
            outBuf += ",\"pumpPower\":";
            outBuf += String(f6 ? atoi(f6) : 0);
            outBuf += ",\"fanPower\":";
            outBuf += String(f7 ? atoi(f7) : 0);
            outBuf += ",\"extractorPower\":";
            outBuf += String(f8 ? atoi(f8) : 0);
            outBuf += ",\"mode\":";
            outBuf += String(f9 ? atoi(f9) : 0);
            outBuf += "}";
            firstJsonRecord = false;
        }

        if (outBuf.length() >= 2048) {
            esp_task_wdt_reset();
            server.sendContent(outBuf);
            outBuf = "";
            yield();
        }
    };

    while (file.available()) {
        if (++lines % 32 == 0) {
            esp_task_wdt_reset();
            yield();
        }
        int c = file.read();
        if (c < 0) break;
        if (c == '\n') {
            processLine();
        } else if (c != '\r' && idx < sizeof(lineBuf) - 1) {
            lineBuf[idx++] = (char)c;
        }
    }
    if (idx > 0) {
        processLine();
    }
}

static void streamAllSPIFFSRecords(const String& startDate, const String& endDate, bool asCsv, const String& downloadFilename) {
    unsigned long startTs = (startDate.length() > 0) ? stringToTimestamp(startDate.c_str(), false) : 0;
    unsigned long endTs   = (endDate.length() > 0)   ? stringToTimestamp(endDate.c_str(), true)    : 0;

    if (downloadFilename.length() > 0) {
        server.sendHeader("Content-Disposition", "attachment; filename=" + downloadFilename);
    }
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, asCsv ? "text/csv; charset=utf-8" : "application/json", "");

    String outBuf;
    outBuf.reserve(2304);
    bool firstJson = true;

    if (asCsv) {
        outBuf = "timestamp,tempCarrier,tempRoom,tempBME,humidity,co2Level,pumpPower,fanPower,extractorPower,mode\n";
    } else {
        outBuf = "{\"data\":[";
    }

    // Збираємо список архівних файлів наперед, щоб не тримати відкритим root одночасно з читанням
    String archives[8];
    uint8_t archCount = 0;
    File root = SPIFFS.open("/");
    if (root) {
        File entry = root.openNextFile();
        while (entry) {
            String fn = entry.name();
            entry.close();
            if (fn.indexOf("archive_") >= 0 && archCount < 8) {
                archives[archCount++] = fn.startsWith("/") ? fn : ("/" + fn);
            }
            entry = root.openNextFile();
        }
        root.close();
    }

    for (uint8_t i = 0; i < archCount; i++) {
        File af = SPIFFS.open(archives[i], "r");
        if (af) {
            streamSPIFFSFileRecords(af, asCsv, startTs, endTs, firstJson, outBuf);
            af.close();
        }
    }

    File curFile = SPIFFS.open(LOG_CURRENT_FILE, "r");
    if (curFile) {
        streamSPIFFSFileRecords(curFile, asCsv, startTs, endTs, firstJson, outBuf);
        curFile.close();
    }

    if (!asCsv) {
        outBuf += "]}";
    }
    if (outBuf.length() > 0) {
        esp_task_wdt_reset();
        server.sendContent(outBuf);
    }
    server.sendContent("");
}

// ============================================================================
// API: ДАНІ ІСТОРІЇ
// ============================================================================

void handleHistoryData() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        server.send(503, "application/json", "{\"error\":\"WiFi не підключено\"}");
        return;
    }

    String source = server.arg("source");
    String startDate = server.arg("start");
    String endDate = server.arg("end");
    String format = server.arg("format");

    if (source == "ram") {
        LoggerStats stats = getLoggerStats();
        uint16_t bufCap = getRamBufferCapacity();
        uint16_t totalRecords = (stats.totalRecordsRAM > bufCap) ? bufCap : (uint16_t)stats.totalRecordsRAM;

        unsigned long startTs = (startDate.length() > 0) ? stringToTimestamp(startDate.c_str(), false) : 0;
        unsigned long endTs   = (endDate.length() > 0)   ? stringToTimestamp(endDate.c_str(), true)    : 0;
        uint16_t hoursLimit   = server.hasArg("hours") ? (uint16_t)server.arg("hours").toInt() : 0;
        uint16_t maxPoints    = server.hasArg("maxPoints") ? (uint16_t)server.arg("maxPoints").toInt() : 0;

        // Якщо задано hours (напр. останні 24 год) — обмежуємо початковий offset у кільцевому буфері
        uint16_t startOffset = 0;
        if (hoursLimit > 0 && startTs == 0) {
            uint32_t maxByHours = (uint32_t)hoursLimit * 60UL;
            if (totalRecords > maxByHours) {
                startOffset = totalRecords - (uint16_t)maxByHours;
            }
        }

        const uint16_t CHUNK_SIZE = 32;
        std::unique_ptr<DataRecord[]> chunk(new (std::nothrow) DataRecord[CHUNK_SIZE]);
        if (!chunk) {
            server.send(500, "application/json", "{\"error\":\"Недостатньо RAM\"}");
            return;
        }

        if (format == "csv") {
            server.sendHeader("Content-Disposition", "attachment; filename=klimat_data.csv");
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/csv", "");

            server.sendContent("timestamp,tempCarrier,tempRoom,tempBME,humidity,co2Level,pumpPower,fanPower,extractorPower,mode\n");

            for (uint16_t offset = startOffset; offset < totalRecords; offset += CHUNK_SIZE) {
                uint16_t chunkCount = (totalRecords - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalRecords - offset);

                if (readRAMDataChunk(chunk.get(), offset, chunkCount)) {
                    String csvChunk = "";
                    csvChunk.reserve(chunkCount * 80);

                    for (uint16_t i = 0; i < chunkCount; i++) {
                        if (startTs > 0 && (chunk[i].timestamp < startTs || (endTs > 0 && chunk[i].timestamp > endTs))) {
                            continue;
                        }
                        csvChunk += String(chunk[i].timestamp) + ",";
                        csvChunk += String(isnan(chunk[i].tempCarrier) ? 0.0f : chunk[i].tempCarrier, 1) + ",";
                        csvChunk += String(isnan(chunk[i].tempRoom) ? 0.0f : chunk[i].tempRoom, 1) + ",";
                        csvChunk += String(isnan(chunk[i].tempBME) ? 0.0f : chunk[i].tempBME, 1) + ",";
                        csvChunk += String(isnan(chunk[i].humidity) ? 0.0f : chunk[i].humidity, 1) + ",";
                        csvChunk += String(isnan(chunk[i].co2Level) ? 0.0f : chunk[i].co2Level, 0) + ",";
                        csvChunk += String(chunk[i].pumpPower) + ",";
                        csvChunk += String(chunk[i].fanPower) + ",";
                        csvChunk += String(chunk[i].extractorPower) + ",";
                        csvChunk += String(chunk[i].mode) + "\n";
                    }
                    if (csvChunk.length() > 0) {
                        esp_task_wdt_reset();
                        server.sendContent(csvChunk);
                    }
                    yield();
                }
            }
            server.sendContent("");

        } else {
            // Обчислюємо крок проріджування для JSON, щоб великі діапазони (тиждень/місяць з PSRAM) не перевантажували Chart.js
            uint16_t effectiveMax = (maxPoints > 0) ? maxPoints : 1440;
            uint16_t rangeCount = totalRecords - startOffset;
            uint16_t step = 1;
            if (startTs == 0 && rangeCount > effectiveMax) {
                step = (rangeCount + effectiveMax - 1) / effectiveMax;
            } else if (startTs > 0 && endTs >= startTs) {
                uint32_t spanMinutes = (endTs - startTs) / 60UL + 1UL;
                uint32_t estCount = (spanMinutes < rangeCount) ? spanMinutes : rangeCount;
                if (estCount > effectiveMax) {
                    step = (uint16_t)((estCount + effectiveMax - 1UL) / effectiveMax);
                }
            }

            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "application/json", "");

            server.sendContent("{\"data\":[");
            bool firstRecord = true;
            uint32_t matchedIdx = 0;

            for (uint16_t offset = startOffset; offset < totalRecords; offset += CHUNK_SIZE) {
                uint16_t chunkCount = (totalRecords - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalRecords - offset);

                if (readRAMDataChunk(chunk.get(), offset, chunkCount)) {
                    String jsonChunk = "";
                    jsonChunk.reserve(chunkCount * 120);

                    for (uint16_t i = 0; i < chunkCount; i++) {
                        if (startTs > 0 && (chunk[i].timestamp < startTs || (endTs > 0 && chunk[i].timestamp > endTs))) {
                            continue;
                        }
                        bool isLastInBuffer = ((offset + i + 1) == totalRecords);
                        if (step > 1 && (matchedIdx % step != 0) && !isLastInBuffer) {
                            matchedIdx++;
                            continue;
                        }
                        matchedIdx++;

                        if (!firstRecord) jsonChunk += ",";
                        jsonChunk += "{";
                        jsonChunk += "\"timestamp\":" + String(chunk[i].timestamp) + ",";
                        jsonChunk += "\"tempCarrier\":" + String(isnan(chunk[i].tempCarrier) ? 0.0f : chunk[i].tempCarrier, 1) + ",";
                        jsonChunk += "\"tempRoom\":" + String(isnan(chunk[i].tempRoom) ? 0.0f : chunk[i].tempRoom, 1) + ",";
                        jsonChunk += "\"tempBME\":" + String(isnan(chunk[i].tempBME) ? 0.0f : chunk[i].tempBME, 1) + ",";
                        jsonChunk += "\"humidity\":" + String(isnan(chunk[i].humidity) ? 0.0f : chunk[i].humidity, 1) + ",";
                        jsonChunk += "\"co2Level\":" + String(isnan(chunk[i].co2Level) ? 0.0f : chunk[i].co2Level, 0) + ",";
                        jsonChunk += "\"pumpPower\":" + String(chunk[i].pumpPower) + ",";
                        jsonChunk += "\"fanPower\":" + String(chunk[i].fanPower) + ",";
                        jsonChunk += "\"extractorPower\":" + String(chunk[i].extractorPower) + ",";
                        jsonChunk += "\"mode\":" + String(chunk[i].mode);
                        jsonChunk += "}";
                        firstRecord = false;
                    }
                    if (jsonChunk.length() > 0) {
                        esp_task_wdt_reset();
                        server.sendContent(jsonChunk);
                    }
                    yield();
                }
            }

            server.sendContent("]}");
            server.sendContent("");
        }

    } else if (source == "spiffs") {
        Serial.printf("📥 SPIFFS stream request: %s to %s (format: %s)\n",
                      startDate.c_str(), endDate.c_str(), format.c_str());
        bool asCsv = (format == "csv");
        String dlName = asCsv ? "klimat_archive.csv" : "";
        streamAllSPIFFSRecords(startDate, endDate, asCsv, dlName);
    } else {
        server.send(400, "application/json", "{\"error\":\"Невірний параметр source\"}");
    }
}

// ============================================================================
// API: СТАТИСТИКА ЛОГУВАННЯ
// ============================================================================

void handleHistoryClearLogs() {
    if (!checkAuth()) return;
    File f = SPIFFS.open(LOG_CURRENT_FILE, "w");
    if (f) { f.close(); }

    File root = SPIFFS.open("/");
    if (root) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            file.close();
            if (name.indexOf("archive_") >= 0) SPIFFS.remove(name);
            file = root.openNextFile();
        }
        root.close();
    }
    Serial.println("🧹 [SPIFFS] Всі логи очищено через веб");
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleHistoryStats() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        server.send(503, "application/json", "{\"error\":\"WiFi не підключено\"}");
        return;
    }

    LoggerStats stats = getLoggerStats();

    String json = "{";
    json += "\"totalRecordsRAM\":" + String(stats.totalRecordsRAM) + ",";
    json += "\"ramBufferCapacity\":" + String(getRamBufferCapacity()) + ",";
    json += "\"ramInPsram\":" + String(isRamBufferInPsram() ? "true" : "false") + ",";
    json += "\"psramTotalBytes\":" + String(ESP.getPsramSize()) + ",";
    json += "\"psramFreeBytes\":" + String(ESP.getFreePsram()) + ",";
    json += "\"totalRecordsSPIFFS\":" + String(stats.totalRecordsSPIFFS) + ",";
    json += "\"lastLogTimeRAM\":" + String(stats.lastLogTimeRAM) + ",";
    json += "\"lastLogTimeSPIFFS\":" + String(stats.lastLogTimeSPIFFS) + ",";
    json += "\"currentFileSize\":" + String(stats.currentFileSize) + ",";
    json += "\"archiveFilesCount\":" + String(stats.archiveFilesCount) + ",";
    json += "\"spiffsUsedBytes\":" + String(stats.spiffsUsedBytes) + ",";
    json += "\"spiffsTotalBytes\":" + String(stats.spiffsTotalBytes) + ",";
    float usedPct = (stats.spiffsTotalBytes > 0) ? ((stats.spiffsUsedBytes * 100.0f) / stats.spiffsTotalBytes) : 0.0f;
    json += "\"spiffsUsedPercent\":" + String(usedPct, 1);
    json += "}";

    server.send(200, "application/json", json);
}

// ============================================================================
// API: ЕКСПОРТ ДАНИХ
// ============================================================================

void handleHistoryExport() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        server.send(503, "text/plain", "WiFi не підключено");
        return;
    }

    String startDate = server.arg("start");
    String endDate = server.arg("end");
    String format = server.arg("format");

    if (startDate.length() == 0 || endDate.length() == 0) {
        server.send(400, "text/plain", "Потрібні параметри start та end");
        return;
    }

    bool asCsv = (format != "json");
    String ext = asCsv ? ".csv" : ".json";
    String filename = "klimat_data_" + startDate + "_" + endDate + ext;
    streamAllSPIFFSRecords(startDate, endDate, asCsv, filename);
}