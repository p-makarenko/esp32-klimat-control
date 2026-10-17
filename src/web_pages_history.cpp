// ============================================================================
// WEB_PAGES_HISTORY.CPP - Сторінки історії та графіків
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Історія даних (handleHistoryPage, handleHistoryData, handleHistoryStats, handleHistoryExport)
// Критика Крок 2: Покращено touch events для графіків
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
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
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("📈 Історія даних", true); // true = includeChartJS

    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("📈 ІСТОРІЯ ТЕМПЕРАТУРИ І ВОЛОГОСТІ");

    // Статистика
    LoggerStats stats = getLoggerStats();
    html += "<div class='info' id='statsInfo' style='text-align: center; color: #666; margin-bottom: 20px; font-size: 0.9em;'>";
    html += "Записів в RAM: " + String(stats.totalRecordsRAM) + " / " + String(HISTORY_BUFFER_SIZE);
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

    html += "<div style='display: flex; flex-wrap: wrap; gap: 15px; align-items: center; padding-top: 10px; border-top: 1px solid #eee;'>";
    html += "<div><strong>📁 Джерело даних:</strong> ";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='ram' checked onchange='switchDataSource()'> 📊 RAM (6 год)</label>";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='spiffs' onchange='switchDataSource()'> 💾 Пристрій (архів)</label>";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='sheets' onchange='switchDataSource()'> ☁️ Google Sheets</label>";
    html += "</div>";
    html += "</div>";

    html += "<div id='dateSelector' style='display: none; margin-top: 15px; padding-top: 12px; border-top: 1px solid #eee;'>";
    html += "<div style='display: flex; flex-wrap: wrap; gap: 10px; align-items: center;'>";
    html += "<label>Від: <input type='date' id='startDate' style='padding: 8px; border-radius: 4px; border: 1px solid #ddd;'></label>";
    html += "<label>До: <input type='date' id='endDate' style='padding: 8px; border-radius: 4px; border: 1px solid #ddd;'></label>";
    html += "<button id='loadBtn' onclick='loadDataForCurrentSelection()' class='btn' style='margin: 0; padding: 8px 16px;'>📥 Завантажити</button>";
    html += "<span id='loadStatus' style='margin-left: 10px; color: #666; font-size: 0.9em;'></span>";
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

    // JavaScript для графіків
    html += "<script>";
    html += "let tempChart, humChart, co2Chart;";
    html += "window.chartFullDates = [];";
    html += "let currentPeriod = 'day';";
    html += "const SHEETS_URL = '" + String(GOOGLE_SCRIPT_URL) + "';";

    // Конфігурація zoom з оптимізованими threshold для touch
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
    html += "label: function(ctx) { return ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(1) + '°C'; }";
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
    html += "label: function(ctx) { return ctx.parsed.y.toFixed(1) + '%'; }";
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
    html += "label: function(ctx) { return ctx.parsed.y.toFixed(0) + ' ppm'; }";
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
    html += "const today = new Date().toISOString().split('T')[0];";
    html += "let start = today;";
    html += "if (period === 'day') {";
    html += "document.getElementById('startDate').value = today;";
    html += "document.getElementById('endDate').value = today;";
    html += "document.getElementById('dateSelector').style.display = 'none';";
    html += "const src = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "if (src === 'ram') { loadRAMData(); return; }";
    html += "} else if (period === 'week') {";
    html += "start = new Date(Date.now() - 7*24*60*60*1000).toISOString().split('T')[0];";
    html += "} else if (period === 'month') {";
    html += "start = new Date(Date.now() - 30*24*60*60*1000).toISOString().split('T')[0];";
    html += "} else if (period === 'year') {";
    html += "start = new Date(Date.now() - 365*24*60*60*1000).toISOString().split('T')[0];";
    html += "} else if (period === 'custom') {";
    html += "document.getElementById('dateSelector').style.display = 'block';";
    html += "if (document.querySelector('input[name=\"dataSource\"]:checked').value === 'ram') {";
    html += "document.querySelector('input[name=\"dataSource\"][value=\"spiffs\"]').checked = true;";
    html += "}";
    html += "return;";
    html += "}";
    html += "document.getElementById('startDate').value = start;";
    html += "document.getElementById('endDate').value = today;";
    html += "document.getElementById('dateSelector').style.display = 'block';";
    html += "if (document.querySelector('input[name=\"dataSource\"]:checked').value === 'ram') {";
    html += "document.querySelector('input[name=\"dataSource\"][value=\"spiffs\"]').checked = true;";
    html += "}";
    html += "loadDataForCurrentSelection();";
    html += "}";

    // Перемикання джерела даних
    html += "function switchDataSource() {";
    html += "const source = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "const dateSelector = document.getElementById('dateSelector');";
    html += "if (source === 'ram') {";
    html += "selectPeriod('day');";
    html += "} else {";
    html += "const today = new Date().toISOString().split('T')[0];";
    html += "if (!document.getElementById('startDate').value) document.getElementById('startDate').value = today;";
    html += "if (!document.getElementById('endDate').value) document.getElementById('endDate').value = today;";
    html += "dateSelector.style.display = 'block';";
    html += "const status = document.getElementById('loadStatus');";
    html += "if (source === 'sheets') {";
    html += "  if (status) status.textContent = 'ℹ️ Оберіть дати та натисніть «Завантажити»';";
    html += "} else {";
    html += "  loadDataForCurrentSelection();";
    html += "}";
    html += "}";
    html += "}";

    // Завантаження за поточним вибором
    html += "function loadDataForCurrentSelection() {";
    html += "const source = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "const startDate = document.getElementById('startDate').value;";
    html += "const endDate = document.getElementById('endDate').value;";
    html += "if (source === 'ram') { loadRAMData(); }";
    html += "else if (source === 'sheets') { loadSheetsData(startDate, endDate); }";
    html += "else { loadArchiveData(startDate, endDate); }";
    html += "}";

    // Завантаження RAM
    html += "function loadRAMData() {";
    html += "const status = document.getElementById('loadStatus');";
    html += "if (status) status.textContent = '⏳ Оновлення...';";
    html += "fetch('/history/data?source=ram&format=json')";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "let data = result.data || [];";
    html += "data = data.filter(r => r.timestamp > 1000000000);";
    html += "data.sort((a, b) => a.timestamp - b.timestamp);";
    html += "const labels = [], fullDates = [], tc = [], tr = [], tb = [], hum = [], co2 = [];";
    html += "data.forEach(record => {";
    html += "const date = new Date(record.timestamp * 1000);";
    html += "const day = String(date.getDate()).padStart(2, '0');";
    html += "const month = String(date.getMonth() + 1).padStart(2, '0');";
    html += "const year = date.getFullYear();";
    html += "const hours = String(date.getHours()).padStart(2, '0');";
    html += "const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "labels.push(hours + ':' + minutes);";
    html += "fullDates.push(day + '.' + month + '.' + year + ' ' + hours + ':' + minutes);";
    html += "tc.push(record.tempCarrier); tr.push(record.tempRoom); tb.push(record.tempBME);";
    html += "hum.push(record.humidity); co2.push(record.co2Level || 0);";
    html += "});";
    html += "window.chartFullDates = fullDates;";
    html += "updateCharts(labels, tc, tr, tb, hum, co2);";
    html += "if (status) status.textContent = '✅ ' + data.length + ' записів';";
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка:', error);";
    html += "if (status) status.textContent = '❌ Помилка RAM';";
    html += "});";
    html += "}";

    // Завантаження архіву SPIFFS
    html += "function loadArchiveData(startDate, endDate) {";
    html += "const status = document.getElementById('loadStatus');";
    html += "const btn = document.getElementById('loadBtn');";
    html += "if (!startDate || !endDate) { alert('Оберіть дати!'); return; }";
    html += "if (btn) btn.disabled = true;";
    html += "if (status) status.textContent = '⏳ Завантаження SPIFFS...';";
    html += "fetch(`/history/data?source=spiffs&start=${startDate}&end=${endDate}&format=json`)";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "let data = result.data || [];";
    html += "data = data.filter(r => r.timestamp > 1000000000);";
    html += "data.sort((a, b) => a.timestamp - b.timestamp);";
    html += "if (data.length === 0) {";
    html += "if (status) status.textContent = '⚠️ Немає даних за період';";
    html += "if (btn) btn.disabled = false;";
    html += "return;";
    html += "}";
    html += "const labels = [], fullDates = [], tc = [], tr = [], tb = [], hum = [], co2 = [];";
    html += "data.forEach(record => {";
    html += "const date = new Date(record.timestamp * 1000);";
    html += "const day = String(date.getDate()).padStart(2, '0');";
    html += "const month = String(date.getMonth() + 1).padStart(2, '0');";
    html += "const year = date.getFullYear();";
    html += "const hours = String(date.getHours()).padStart(2, '0');";
    html += "const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "labels.push(day + '.' + month + ' ' + hours + ':' + minutes);";
    html += "fullDates.push(day + '.' + month + '.' + year + ' ' + hours + ':' + minutes);";
    html += "tc.push(record.tempCarrier); tr.push(record.tempRoom); tb.push(record.tempBME);";
    html += "hum.push(record.humidity); co2.push(record.co2Level || 0);";
    html += "});";
    html += "window.chartFullDates = fullDates;";
    html += "updateCharts(labels, tc, tr, tb, hum, co2);";
    html += "if (status) status.textContent = '✅ ' + data.length + ' записів';";
    html += "if (btn) btn.disabled = false;";
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка:', error);";
    html += "if (status) status.textContent = '❌ Помилка читання архіву';";
    html += "if (btn) btn.disabled = false;";
    html += "});";
    html += "}";

    // Завантаження Google Sheets
    html += "function loadSheetsData(startDate, endDate) {";
    html += "const status = document.getElementById('loadStatus');";
    html += "const btn = document.getElementById('loadBtn');";
    html += "if (!startDate || !endDate) { alert('Оберіть дати!'); return; }";
    html += "if (btn) btn.disabled = true;";
    html += "  if (status) status.textContent = '⏳ Завантаження Sheets...';";
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
    html += "const labels=[], fullDates=[], tc=[], tr=[], tb=[], hum=[], co2=[];";
    html += "rows.forEach(r => {";
    html += "if (!r[0]) return;";
    html += "const dt = String(r[0]);";
    html += "labels.push(dt.substring(5,16));";
    html += "fullDates.push(dt);";
    html += "tc.push(parseFloat(r[1])||0);";
    html += "tr.push(parseFloat(r[2])||0);";
    html += "tb.push(parseFloat(r[3])||0);";
    html += "hum.push(parseFloat(r[4])||0);";
    html += "co2.push(parseFloat(r[5])||0);";
    html += "});";
    html += "window.chartFullDates = fullDates;";
    html += "updateCharts(labels, tc, tr, tb, hum, co2);";
    html += "if (status) status.textContent = '✅ ' + rows.length + ' записів';";
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

    // Старт
    html += "initCharts();";
    html += "selectPeriod('day');";

    html += "</script>";

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

// ============================================================================
// API: ДАНІ ІСТОРІЇ
// ============================================================================

void handleHistoryData() {
    if (WiFi.status() != WL_CONNECTED) {
        server.send(503, "application/json", "{\"error\":\"WiFi не підключено\"}");
        return;
    }

    String source = server.arg("source");
    String startDate = server.arg("start");
    String endDate = server.arg("end");
    String format = server.arg("format");

    if (source == "ram") {
        LoggerStats stats = getLoggerStats();
        uint16_t totalRecords = (stats.totalRecordsRAM > HISTORY_BUFFER_SIZE) ? HISTORY_BUFFER_SIZE : stats.totalRecordsRAM;

        if (format == "csv") {
            server.sendHeader("Content-Disposition", "attachment; filename=klimat_data.csv");
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/csv", "");

            server.sendContent("timestamp,tempCarrier,tempRoom,tempBME,humidity,co2Level,pumpPower,fanPower,extractorPower,mode\n");

            const uint16_t CHUNK_SIZE = 50;
            DataRecord chunk[CHUNK_SIZE];

            for (uint16_t offset = 0; offset < totalRecords; offset += CHUNK_SIZE) {
                uint16_t chunkCount = (totalRecords - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalRecords - offset);

                if (readRAMDataChunk(chunk, offset, chunkCount)) {
                    String csvChunk = "";
                    csvChunk.reserve(chunkCount * 80);

                    for (uint16_t i = 0; i < chunkCount; i++) {
                        if (chunk[i].timestamp > 0) {
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
                    }
                    server.sendContent(csvChunk);
                    yield();
                }
            }
            server.sendContent("");

        } else {
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "application/json", "");

            server.sendContent("{\"data\":[");

            const uint16_t CHUNK_SIZE = 50;
            DataRecord chunk[CHUNK_SIZE];
            bool firstRecord = true;

            for (uint16_t offset = 0; offset < totalRecords; offset += CHUNK_SIZE) {
                uint16_t chunkCount = (totalRecords - offset > CHUNK_SIZE) ? CHUNK_SIZE : (totalRecords - offset);

                if (readRAMDataChunk(chunk, offset, chunkCount)) {
                    String jsonChunk = "";
                    jsonChunk.reserve(chunkCount * 120);

                    for (uint16_t i = 0; i < chunkCount; i++) {
                        if (chunk[i].timestamp > 0) {
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
                    }
                    server.sendContent(jsonChunk);
                    yield();
                }
            }

            server.sendContent("]}");
            server.sendContent("");
        }

    } else if (source == "spiffs") {
        if (startDate.length() == 0 || endDate.length() == 0) {
            server.send(400, "application/json", "{\"error\":\"Потрібні параметри start та end\"}");
            return;
        }

        String data;

        Serial.printf("📥 SPIFFS read request: %s to %s (format: %s)\n",
                      startDate.c_str(), endDate.c_str(), format.c_str());

        if (format == "csv") {
            if (readSPIFFSDataCSV(startDate.c_str(), endDate.c_str(), data)) {
                Serial.printf("✅ SPIFFS CSV: %u bytes\n", data.length());
                server.sendHeader("Content-Disposition", "attachment; filename=klimat_archive.csv");
                server.send(200, "text/csv", data);
            } else {
                Serial.println("❌ SPIFFS CSV read failed");
                server.send(500, "application/json", "{\"error\":\"Помилка читання SPIFFS\"}");
            }
        } else {
            if (readSPIFFSData(startDate.c_str(), endDate.c_str(), data)) {
                Serial.printf("✅ SPIFFS JSON: %u bytes\n", data.length());
                server.send(200, "application/json", data);
            } else {
                Serial.println("❌ SPIFFS JSON read failed");
                server.send(500, "application/json", "{\"error\":\"Помилка читання SPIFFS\"}");
            }
        }
    } else {
        server.send(400, "application/json", "{\"error\":\"Невірний параметр source\"}");
    }
}

// ============================================================================
// API: СТАТИСТИКА ЛОГУВАННЯ
// ============================================================================

void handleHistoryClearLogs() {
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
    if (WiFi.status() != WL_CONNECTED) {
        server.send(503, "application/json", "{\"error\":\"WiFi не підключено\"}");
        return;
    }

    LoggerStats stats = getLoggerStats();

    String json = "{";
    json += "\"totalRecordsRAM\":" + String(stats.totalRecordsRAM) + ",";
    json += "\"totalRecordsSPIFFS\":" + String(stats.totalRecordsSPIFFS) + ",";
    json += "\"lastLogTimeRAM\":" + String(stats.lastLogTimeRAM) + ",";
    json += "\"lastLogTimeSPIFFS\":" + String(stats.lastLogTimeSPIFFS) + ",";
    json += "\"currentFileSize\":" + String(stats.currentFileSize) + ",";
    json += "\"archiveFilesCount\":" + String(stats.archiveFilesCount) + ",";
    json += "\"spiffsUsedBytes\":" + String(stats.spiffsUsedBytes) + ",";
    json += "\"spiffsTotalBytes\":" + String(stats.spiffsTotalBytes) + ",";
    json += "\"spiffsUsedPercent\":" + String((stats.spiffsUsedBytes * 100.0) / stats.spiffsTotalBytes, 1);
    json += "}";

    server.send(200, "application/json", json);
}

// ============================================================================
// API: ЕКСПОРТ ДАНИХ
// ============================================================================

void handleHistoryExport() {
    if (WiFi.status() != WL_CONNECTED) {
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

    String data;
    String contentType;
    String filename;

    if (format == "json") {
        data = exportToJSON(startDate.c_str(), endDate.c_str());
        contentType = "application/json";
        filename = "klimat_data_" + startDate + "_" + endDate + ".json";
    } else {
        data = exportToCSV(startDate.c_str(), endDate.c_str());
        contentType = "text/csv";
        filename = "klimat_data_" + startDate + "_" + endDate + ".csv";
    }

    server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
    server.send(200, contentType, data);
}
