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

    // Вибір джерела даних
    html += "<div class='card' style='border-left-color: #ffc107;'>";
    html += "<strong>📁 Джерело даних:</strong><br>";
    html += "<div style='margin-top: 10px;'>";
    html += "<label style='margin-right: 20px;'><input type='radio' name='dataSource' value='ram' checked onchange='switchDataSource()'> 📊 RAM (останні 24 години)</label>";
    html += "<label><input type='radio' name='dataSource' value='spiffs' onchange='switchDataSource()'> 💾 SPIFFS (архів)</label>";
    html += "</div>";
    html += "<div id='dateSelector' style='display: none; margin-top: 15px;'>";
    html += "<label style='margin-right: 10px;'>Від: <input type='date' id='startDate' style='padding: 8px; border-radius: 4px; border: 1px solid #ddd;'></label>";
    html += "<label style='margin-right: 10px;'>До: <input type='date' id='endDate' style='padding: 8px; border-radius: 4px; border: 1px solid #ddd;'></label>";
    html += "<button onclick='loadArchiveData()' class='btn' style='margin-top: 10px;'>📥 Завантажити</button>";
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

    html += "</div>"; // container

    // JavaScript для графіків
    html += "<script>";
    html += "let tempChart, humChart;";

    // Конфігурація zoom з оптимізованими threshold для touch
    // (Критика Крок 2: покращено touch events)
    html += "const zoomConfig = {";
    html += "  zoom: {";
    html += "    wheel: { enabled: true, speed: 0.1 },";
    html += "    pinch: { enabled: true },";  // Pinch zoom для мобільних
    html += "    drag: {";
    html += "      enabled: true,";
    html += "      backgroundColor: 'rgba(66, 133, 244, 0.2)',";
    html += "      borderColor: 'rgba(66, 133, 244, 0.8)',";
    html += "      borderWidth: 1,";
    html += "      threshold: 10";  // Менший threshold для touch
    html += "    },";
    html += "    mode: 'x'";
    html += "  },";
    html += "  pan: {";
    html += "    enabled: true,";
    html += "    mode: 'x',";
    html += "    threshold: 5,";  // Менший threshold для плавного panning
    html += "    modifierKey: null";  // Працює без Ctrl на touch
    html += "  }";
    html += "};";

    // Завантаження даних
    html += "fetch('/history/data?source=ram&format=json')";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "const data = result.data || [];";
    html += "const labels = [];";
    html += "const tempCarrier = [];";
    html += "const tempRoom = [];";
    html += "const tempBME = [];";
    html += "const humidity = [];";

    html += "data.forEach(record => {";
    html += "if (record.timestamp > 0) {";
    html += "const date = new Date(record.timestamp * 1000);";
    html += "const hours = String(date.getHours()).padStart(2, '0');";
    html += "const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "labels.push(hours + ':' + minutes);";
    html += "tempCarrier.push(record.tempCarrier);";
    html += "tempRoom.push(record.tempRoom);";
    html += "tempBME.push(record.tempBME);";
    html += "humidity.push(record.humidity);";
    html += "}";
    html += "});";

    // Графік температури
    html += "tempChart = new Chart(document.getElementById('tempChart'), {";
    html += "type: 'line',";
    html += "data: {";
    html += "labels: labels,";
    html += "datasets: [{";
    html += "label: '🔥 Теплоносій',";
    html += "data: tempCarrier,";
    html += "borderColor: '#e74c3c',";
    html += "backgroundColor: 'rgba(231, 76, 60, 0.1)',";
    html += "borderWidth: 2,";
    html += "pointRadius: 0,";  // Без точок для швидшого рендерингу
    html += "tension: 0.4";
    html += "}, {";
    html += "label: '🏠 Кімната',";
    html += "data: tempRoom,";
    html += "borderColor: '#3498db',";
    html += "backgroundColor: 'rgba(52, 152, 219, 0.1)',";
    html += "borderWidth: 2,";
    html += "pointRadius: 0,";
    html += "tension: 0.4";
    html += "}, {";
    html += "label: '🌡️ BME280',";
    html += "data: tempBME,";
    html += "borderColor: '#2ecc71',";
    html += "backgroundColor: 'rgba(46, 204, 113, 0.1)',";
    html += "borderWidth: 2,";
    html += "pointRadius: 0,";
    html += "tension: 0.4";
    html += "}]},";
    html += "options: {";
    html += "responsive: true,";
    html += "maintainAspectRatio: false,";
    html += "interaction: { mode: 'index', intersect: false },";
    html += "plugins: {";
    html += "legend: { display: true, position: 'top' },";
    html += "tooltip: {";
    html += "callbacks: {";
    html += "title: function(ctx) { return ctx[0].label; },";
    html += "label: function(ctx) { return ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(1) + '°C'; }";
    html += "}},";
    html += "zoom: zoomConfig";
    html += "},";
    html += "scales: { y: { beginAtZero: false, title: { display: true, text: '°C' } } }";
    html += "}});";

    // Графік вологості
    html += "humChart = new Chart(document.getElementById('humChart'), {";
    html += "type: 'line',";
    html += "data: {";
    html += "labels: labels,";
    html += "datasets: [{";
    html += "label: '💧 Вологість',";
    html += "data: humidity,";
    html += "borderColor: '#3498db',";
    html += "backgroundColor: 'rgba(52, 152, 219, 0.2)',";
    html += "fill: true,";
    html += "borderWidth: 2,";
    html += "pointRadius: 0,";
    html += "tension: 0.4";
    html += "}]},";
    html += "options: {";
    html += "responsive: true,";
    html += "maintainAspectRatio: false,";
    html += "interaction: { mode: 'index', intersect: false },";
    html += "plugins: {";
    html += "legend: { display: true, position: 'top' },";
    html += "tooltip: {";
    html += "callbacks: {";
    html += "title: function(ctx) { return ctx[0].label; },";
    html += "label: function(ctx) { return ctx.parsed.y.toFixed(1) + '%'; }";
    html += "}},";
    html += "zoom: zoomConfig";
    html += "},";
    html += "scales: { y: { beginAtZero: false, max: 100, title: { display: true, text: '%' } } }";
    html += "}});";

    // Подвійний клік для скидання масштабу
    html += "document.getElementById('tempChart').ondblclick = function() { tempChart.resetZoom(); };";
    html += "document.getElementById('humChart').ondblclick = function() { humChart.resetZoom(); };";

    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка завантаження даних:', error);";
    html += "alert('Помилка завантаження історичних даних. Перезавантажте сторінку.');";
    html += "});";

    // Функція переключення джерела даних
    html += "function switchDataSource() {";
    html += "const source = document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "const dateSelector = document.getElementById('dateSelector');";
    html += "if (source === 'spiffs') {";
    html += "dateSelector.style.display = 'block';";
    html += "const today = new Date().toISOString().split('T')[0];";
    html += "const weekAgo = new Date(Date.now() - 7*24*60*60*1000).toISOString().split('T')[0];";
    html += "document.getElementById('endDate').value = today;";
    html += "document.getElementById('startDate').value = weekAgo;";
    html += "} else {";
    html += "dateSelector.style.display = 'none';";
    html += "location.reload();";
    html += "}";
    html += "}";

    // Функція завантаження архівних даних
    html += "function loadArchiveData() {";
    html += "const startDate = document.getElementById('startDate').value;";
    html += "const endDate = document.getElementById('endDate').value;";
    html += "if (!startDate || !endDate) {";
    html += "alert('Оберіть дати!');";
    html += "return;";
    html += "}";
    html += "fetch(`/history/data?source=spiffs&start=${startDate}&end=${endDate}&format=json`)";
    html += ".then(response => response.json())";
    html += ".then(result => {";
    html += "const data = result.data || [];";
    html += "if (data.length === 0) {";
    html += "alert('Немає даних за вибраний період');";
    html += "return;";
    html += "}";
    html += "const labels = [];";
    html += "const tempCarrier = [];";
    html += "const tempRoom = [];";
    html += "const tempBME = [];";
    html += "const humidity = [];";
    html += "data.forEach(record => {";
    html += "if (record.timestamp > 0) {";
    html += "const date = new Date(record.timestamp * 1000);";
    html += "const day = String(date.getDate()).padStart(2, '0');";
    html += "const month = String(date.getMonth() + 1).padStart(2, '0');";
    html += "const hours = String(date.getHours()).padStart(2, '0');";
    html += "const minutes = String(date.getMinutes()).padStart(2, '0');";
    html += "labels.push(day + '/' + month + ' ' + hours + ':' + minutes);";
    html += "tempCarrier.push(record.tempCarrier);";
    html += "tempRoom.push(record.tempRoom);";
    html += "tempBME.push(record.tempBME);";
    html += "humidity.push(record.humidity);";
    html += "}";
    html += "});";
    html += "tempChart.data.labels = labels;";
    html += "tempChart.data.datasets[0].data = tempCarrier;";
    html += "tempChart.data.datasets[1].data = tempRoom;";
    html += "tempChart.data.datasets[2].data = tempBME;";
    html += "tempChart.update();";
    html += "humChart.data.labels = labels;";
    html += "humChart.data.datasets[0].data = humidity;";
    html += "humChart.update();";
    html += "})";
    html += ".catch(error => {";
    html += "console.error('Помилка:', error);";
    html += "alert('Помилка завантаження архівних даних');";
    html += "});";
    html += "}";

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

            server.sendContent("timestamp,tempCarrier,tempRoom,tempBME,humidity,pumpPower,fanPower,extractorPower,mode\n");

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
                            csvChunk += String(chunk[i].tempCarrier, 1) + ",";
                            csvChunk += String(chunk[i].tempRoom, 1) + ",";
                            csvChunk += String(chunk[i].tempBME, 1) + ",";
                            csvChunk += String(chunk[i].humidity, 1) + ",";
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
                            jsonChunk += "\"tempCarrier\":" + String(chunk[i].tempCarrier, 1) + ",";
                            jsonChunk += "\"tempRoom\":" + String(chunk[i].tempRoom, 1) + ",";
                            jsonChunk += "\"tempBME\":" + String(chunk[i].tempBME, 1) + ",";
                            jsonChunk += "\"humidity\":" + String(chunk[i].humidity, 1) + ",";
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
