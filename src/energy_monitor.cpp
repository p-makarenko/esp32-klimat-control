#include "energy_monitor.h"
#include "system_core.h"
#include "config.h"
#include "web_common.h"
#include "google_sheets_sync.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

// Forward declaration
bool checkAuth();
extern String getUkraineMarquee();

// ============================================================================
// ГЛОБАЛЬНІ ЗМІННІ
// ============================================================================

EnergyMeasurements energyData;
unsigned long lastEnergyUpdate = 0;
unsigned long lastPowerRecord = 0;
int currentEnergyHour = -1;

// RAM буфер для потужності (зберігається на ESP32)
PowerRecord powerBuffer[ENERGY_POWER_BUFFER_SIZE];
uint16_t powerBufferIndex = 0;
uint16_t powerBufferCount = 0;

#ifndef ENABLE_ENERGY_MONITOR
#define ENABLE_ENERGY_MONITOR 0
#endif

#if ENABLE_ENERGY_MONITOR
// Raw Modbus RTU — без бібліотеки, бо PZEM004Tv30 не працює на цій платі
HardwareSerial& PZEMSerial = Serial2;

uint16_t crc16(uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
            else { crc >>= 1; }
        }
    }
    return crc;
}

bool pzemReadRegisters(uint8_t addr, uint8_t *resp, uint8_t respLen) {
    uint8_t cmd[8];
    cmd[0] = addr;
    cmd[1] = 0x04;  // Read Input Registers
    cmd[2] = 0x00; cmd[3] = 0x00;  // Register addr
    cmd[4] = 0x00; cmd[5] = 0x0A;  // 10 registers
    uint16_t crc = crc16(cmd, 6);
    cmd[6] = crc & 0xFF;
    cmd[7] = (crc >> 8) & 0xFF;

    while (PZEMSerial.available()) PZEMSerial.read();
    PZEMSerial.write(cmd, 8);
    PZEMSerial.flush();

    unsigned long start = millis();
    uint8_t idx = 0;
    while (idx < respLen && millis() - start < 500) {
        if (PZEMSerial.available()) {
            resp[idx++] = PZEMSerial.read();
        }
    }
    return (idx == respLen);
}
#endif

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

void initEnergyMonitor() {
#if ENABLE_ENERGY_MONITOR
    Serial.println("⚙️  Ініціалізація енергоконтролера...");

    // Ініціалізація LittleFS для історії
    if (!LittleFS.begin(true)) {
        Serial.println("⚠️  Помилка ініціалізації LittleFS");
    } else {
        Serial.println("✓ LittleFS ініціалізовано");
    }

    PZEMSerial.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
    Serial.printf("📌 PZEM піни: RX=%d, TX=%d\n", PZEM_RX_PIN, PZEM_TX_PIN);
    Serial.println("✅ Енергоконтролер готовий");
#else
    Serial.println("ℹ️  Енергоконтролер вимкнено (ENABLE_ENERGY_MONITOR=0)");
#endif
}

// ============================================================================
// ОНОВЛЕННЯ ДАНИХ
// ============================================================================

void updateEnergyData() {
#if ENABLE_ENERGY_MONITOR
    unsigned long now = millis();

    if (now - lastEnergyUpdate < 2000) return;
    lastEnergyUpdate = now;

    uint8_t resp[25];
    if (pzemReadRegisters(0x01, resp, 25)) {
        uint16_t recvCrc = resp[23] | (resp[24] << 8);
        uint16_t calcCrc = crc16(resp, 23);
        if (recvCrc == calcCrc && resp[1] == 0x04 && resp[2] == 20) {
            float v = (resp[3] << 8 | resp[4]) * 0.1f;
            uint32_t rawCurrent  = (resp[5] << 8 | resp[6]) | ((uint32_t)(resp[7] << 8 | resp[8]) << 16);
            float c = rawCurrent * 0.001f;
            uint32_t rawPower    = (resp[9] << 8 | resp[10]) | ((uint32_t)(resp[11] << 8 | resp[12]) << 16);
            float p = rawPower * 0.1f;
            uint32_t rawEnergy   = (resp[13] << 8 | resp[14]) | ((uint32_t)(resp[15] << 8 | resp[16]) << 16);
            float e = rawEnergy * 0.001f;
            float freq = (resp[17] << 8 | resp[18]) * 0.1f;
            float pf = (resp[19] << 8 | resp[20]) * 0.01f;

            // Санітарна фільтрація хибних пакетів PZEM
            // Напруга в мережі повинна бути в межах 0..350V, струм 0..100A, частота 0 або 40..70Hz
            if (v >= 0.0f && v <= 350.0f && c >= 0.0f && c <= 100.0f && p >= 0.0f && p <= 25000.0f &&
                (freq == 0.0f || (freq >= 40.0f && freq <= 70.0f))) {
                energyData.voltage   = v;
                energyData.current   = c;
                energyData.power     = p;
                energyData.energy    = e;
                energyData.frequency = freq;
                energyData.powerFactor = pf;
                energyData.error = false;
            } else {
                energyData.error = true;
            }
        } else {
            energyData.error = true;
        }
    } else {
        energyData.error = true;
    }

    // Запис в RAM буфер кожні 2 хвилини (навіть коли світло вимкнено - записуємо 0)
    if (now - lastPowerRecord >= 120000 || lastPowerRecord == 0) {
        lastPowerRecord = now;
        time_t t;
        time(&t);
        if (t > 1000000) {  // Час синхронізовано
            powerBuffer[powerBufferIndex].timestamp = (uint32_t)t;
            powerBuffer[powerBufferIndex].power   = energyData.error ? 0.0f : energyData.power;
            powerBuffer[powerBufferIndex].voltage = energyData.error ? 0.0f : energyData.voltage;
            powerBuffer[powerBufferIndex].current = energyData.error ? 0.0f : energyData.current;
            powerBufferIndex = (powerBufferIndex + 1) % ENERGY_POWER_BUFFER_SIZE;
            if (powerBufferCount < ENERGY_POWER_BUFFER_SIZE) powerBufferCount++;
        }
    }

    // Запис історії kWh щогодини
    time_t t;
    time(&t);
    struct tm* tm_info = localtime(&t);
    if (tm_info && t > 1000000) {  // Час синхронізовано
        if (tm_info->tm_hour != currentEnergyHour) {
            currentEnergyHour = tm_info->tm_hour;
            if (!energyData.error && energyData.energy > 0) {
                appendEnergyHistory(energyData.energy);
                Serial.printf("📊 Записана енергія щогодини: %.3f kWh\n", energyData.energy);
            }
        }
    }
#endif
}

// ============================================================================
// ЗАПИС ІСТОРІЇ
// ============================================================================

void appendEnergyHistory(float energy) {
#if ENABLE_ENERGY_MONITOR
    if (isnan(energy) || energy < 0) return;

    File f = LittleFS.open("/energy_history.csv", "a");
    if (f) {
        time_t now;
        time(&now);
        f.printf("%lu,%.3f\n", now, energy);
        f.close();
    }
#endif
}

// ============================================================================
// ОТРИМАННЯ ДАНИХ
// ============================================================================

EnergyMeasurements getEnergyMeasurements() {
    return energyData;
}

// ============================================================================
// ВЕБ-ОБРОБНИКИ
// ============================================================================

void handleEnergyAPI() {
    if (!checkAuth()) return;

    EnergyMeasurements data = getEnergyMeasurements();

    String json;
    JsonDocument doc;
    doc["v"] = data.voltage;
    doc["c"] = data.current;
    doc["p"] = data.power;
    doc["e"] = data.energy;
    doc["f"] = data.frequency;
    doc["pf"] = data.powerFactor;
    doc["err"] = data.error;

    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

// API: дані потужності з RAM буфера
void handleEnergyPowerData() {
    if (!checkAuth()) return;

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");
    server.sendContent("{\"data\":[");

    bool first = true;
    uint16_t count = powerBufferCount;
    uint16_t startIdx = (count >= ENERGY_POWER_BUFFER_SIZE)
        ? powerBufferIndex
        : 0;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (startIdx + i) % ENERGY_POWER_BUFFER_SIZE;
        if (powerBuffer[idx].timestamp < 1000000000) continue;
        if (!first) server.sendContent(",");
        String rec = "{\"t\":" + String(powerBuffer[idx].timestamp) +
                     ",\"p\":" + String(powerBuffer[idx].power, 1) +
                     ",\"v\":" + String(powerBuffer[idx].voltage, 1) +
                     ",\"c\":" + String(powerBuffer[idx].current, 3) + "}";
        server.sendContent(rec);
        first = false;
        if (i % 50 == 49) yield();
    }

    server.sendContent("]}");
    server.sendContent("");
}

void handleEnergyHistory() {
#if ENABLE_ENERGY_MONITOR
    if (!checkAuth()) return;

    File f = LittleFS.open("/energy_history.csv", "r");
    if (f) {
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/plain", "");
        while (f.available()) {
            char buf[256];
            int len = f.readBytes(buf, sizeof(buf));
            server.sendContent(buf, len);
        }
        f.close();
        server.sendContent("");
    } else {
        server.send(404, "text/plain", "No data");
    }
#else
    server.send(503, "text/plain", "Disabled");
#endif
}

void handleEnergyHistoryStats() {
    if (!checkAuth()) return;

#if ENABLE_ENERGY_MONITOR
    File f = LittleFS.open("/energy_history.csv", "r");
    if (!f) {
        server.send(404, "application/json", "{\"error\":\"No data\"}");
        return;
    }

    JsonDocument doc;
    float totalEnergy = 0;
    int recordCount = 0;
    String line;

    while (f.available()) {
        int b = f.read();
        if (b == '\n' || !f.available()) {
            if (line.length() > 0) {
                int commaPos = line.indexOf(',');
                if (commaPos > 0) {
                    float energy = line.substring(commaPos + 1).toFloat();
                    totalEnergy += energy;
                    recordCount++;
                }
            }
            line = "";
        } else {
            line += (char)b;
        }
    }
    f.close();

    doc["totalEnergy"] = totalEnergy;
    doc["recordCount"] = recordCount;
    doc["avgDaily"] = recordCount > 0 ? totalEnergy / (recordCount / 24.0) : 0;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
#else
    server.send(503, "application/json", "{\"error\":\"Disabled\"}");
#endif
}

void resetEnergyCounter() {
#if ENABLE_ENERGY_MONITOR
    uint8_t cmd[4];
    cmd[0] = 0x01; cmd[1] = 0x42;
    uint16_t crc = crc16(cmd, 2);
    cmd[2] = crc & 0xFF; cmd[3] = (crc >> 8) & 0xFF;
    PZEMSerial.write(cmd, 4);
    Serial.println("🔄 Лічильник енергії скинуто");
#endif
}

void handleEnergyReset() {
    if (!checkAuth()) return;
    resetEnergyCounter();
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleEnergyPage() {
    if (!checkAuth()) return;

    String html = getHtmlHead("⚡ Енергоконтролер", true);
    html += "<div class='container'>";
    html += getNavHeader("⚡ ЕНЕРГОКОНТРОЛЕР");

    // Статус
    html += "<div style='text-align:center;margin-bottom:15px'><span id='st' style='display:inline-block;padding:4px 12px;border-radius:12px;font-size:0.8rem;color:#fff;background:#f44336'>OFFLINE</span></div>";

    // Картки з даними
    html += "<style>.eg{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin:15px 0}";
    html += ".ek{background:#f9f9f9;padding:15px;border-radius:8px;text-align:center;border-left:4px solid #f59e0b}";
    html += ".ev{font-size:1.8rem;font-weight:700;color:#333;display:block;margin:5px 0}";
    html += ".eu{font-size:0.8rem;color:#888}";
    html += "@media(max-width:480px){.eg{grid-template-columns:repeat(2,1fr)}}</style>";

    html += "<div class='eg'>";
    html += "<div class='ek'><span class='ev' id='v'>--</span><span class='eu'>Напруга (V)</span></div>";
    html += "<div class='ek'><span class='ev' id='c'>--</span><span class='eu'>Струм (A)</span></div>";
    html += "<div class='ek'><span class='ev' id='p'>--</span><span class='eu'>Потужність (W)</span></div>";
    html += "<div class='ek'><span class='ev' id='e'>--</span><span class='eu'>Всього (kWh)</span></div>";
    html += "<div class='ek'><span class='ev' id='f'>--</span><span class='eu'>Частота (Hz)</span></div>";
    html += "<div class='ek'><span class='ev' id='pf'>--</span><span class='eu'>Cos &#966;</span></div>";
    html += "</div>";

    // Кнопка скидання — окрема
    html += "<div style='margin:15px 0;text-align:center'><button class='btn' id='rstBtn' style='background:#f44336;color:#fff;padding:10px 20px;border:none;border-radius:5px;cursor:pointer' onclick='resetKwh()'>🔄 Скинути лічильник kWh</button></div>";

    // Панель навігації по періодах
    html += "<div class='card' style='border-left-color: #f59e0b;'>";
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
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='ram' checked onchange='switchDataSource()'> 📊 RAM (24 год)</label>";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='spiffs' onchange='switchDataSource()'> 💾 Пристрій (архів)</label>";
    html += "<label style='margin-left: 8px;'><input type='radio' name='dataSource' value='sheets' onchange='switchDataSource()'> ☁️ Google Sheets</label>";
    html += "</div>";
    html += "<div style='display: flex; gap: 12px;'>";
    html += "<label style='font-size: 0.9rem;'><input type='checkbox' id='chkPower' checked onchange='updateDatasets()'> ⚡ Потужність (W)</label>";
    html += "<label style='font-size: 0.9rem;'><input type='checkbox' id='chkVoltage' checked onchange='updateDatasets()'> 🔌 Напруга (V)</label>";
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

    // Графік потужності та напруги
    html += "<div class='chart-wrapper'>";
    html += "<div class='chart-header'>";
    html += "<h2>⚡ Потужність та Напруга</h2>";
    html += "<button onclick='powerChart.resetZoom()' class='reset-btn'>🔄 Скинути масштаб</button>";
    html += "</div>";
    html += "<div style='display:flex;gap:15px;flex-wrap:wrap;background:#fff8e1;padding:12px 16px;border-radius:8px;border-left:4px solid #f59e0b;margin-bottom:15px;align-items:center;'>";
    html += "<div><span style='color:#666;font-size:0.85rem;'>⚡ Спожито за період:</span><br><strong id='periodEnergy' style='font-size:1.3rem;color:#d97706;'>--</strong> <span style='font-size:0.9rem;font-weight:600;'>kWh</span></div>";
    html += "<div style='border-left:1px solid #ffe082;padding-left:15px;'><span style='color:#666;font-size:0.85rem;'>📊 Сер. потужність:</span><br><strong id='periodAvgPower' style='font-size:1.3rem;color:#333;'>--</strong> <span style='font-size:0.9rem;font-weight:600;'>W</span></div>";
    html += "<div style='border-left:1px solid #ffe082;padding-left:15px;'><span style='color:#666;font-size:0.85rem;'>🔥 Пікова потужність:</span><br><strong id='periodMaxPower' style='font-size:1.3rem;color:#333;'>--</strong> <span style='font-size:0.9rem;font-weight:600;'>W</span></div>";
    html += "</div>";
    html += "<div class='chart-container' style='height:300px'><canvas id='powerCanvas'></canvas></div>";
    html += "</div>";

    html += "</div>"; // container

    html += "<script>";
    html += "var powerChart;";
    html += "var allPowerData = [];";
    html += "window.chartFullDates = [];";
    html += "var currentPeriod = 'day';";
    html += "const SHEETS_URL = '" + String(GOOGLE_SCRIPT_URL) + "';";

    html += "var zoomCfg = {";
    html += "  zoom: { wheel:{enabled:true,speed:0.1}, pinch:{enabled:true},";
    html += "    drag:{enabled:true,backgroundColor:'rgba(245,158,11,0.2)',borderColor:'rgba(245,158,11,0.8)',borderWidth:1,threshold:10}, mode:'x' },";
    html += "  pan:{enabled:true,mode:'x',threshold:5,modifierKey:null}";
    html += "};";

    html += "function initCharts(){";
    html += "  powerChart = new Chart(document.getElementById('powerCanvas'),{";
    html += "    type:'line',";
    html += "    data:{labels:[],datasets:[";
    html += "      { label:'⚡ Потужність (W)',data:[], borderColor:'#f59e0b',backgroundColor:'rgba(245,158,11,0.1)', borderWidth:2,fill:true,tension:0.3,pointRadius:0,yAxisID:'y',hidden:false },";
    html += "      { label:'🔌 Напруга (V)',data:[], borderColor:'#3498db',backgroundColor:'rgba(52,152,219,0.1)', borderWidth:2,fill:true,tension:0.3,pointRadius:0,yAxisID:'y1',hidden:false }";
    html += "    ]},";
    html += "    options:{";
    html += "      responsive:true,maintainAspectRatio:false,animation:{duration:0},";
    html += "      interaction:{mode:'index',intersect:false},";
    html += "      scales:{";
    html += "        x:{ticks:{maxTicksLimit:12,font:{size:11}},grid:{display:false}},";
    html += "        y:{position:'left',beginAtZero:true,title:{display:true,text:'W'},ticks:{font:{size:11}},grid:{color:'#eee'}},";
    html += "        y1:{position:'right',beginAtZero:false,title:{display:true,text:'V'},ticks:{font:{size:11}},grid:{display:false}}";
    html += "      },";
    html += "      plugins:{";
    html += "        legend:{display:true,position:'top'},";
    html += "        tooltip:{callbacks:{";
    html += "          title:function(c){ return (window.chartFullDates && window.chartFullDates[c[0].dataIndex]) ? window.chartFullDates[c[0].dataIndex] : c[0].label; },";
    html += "          label:function(c){ return c.dataset.label+': '+c.parsed.y.toFixed(1); }";
    html += "        }},";
    html += "        zoom:zoomCfg";
    html += "      }";
    html += "    }";
    html += "  });";
    html += "  document.getElementById('powerCanvas').ondblclick=function(){powerChart.resetZoom();};";
    html += "}";

    html += "function selectPeriod(period){";
    html += "  currentPeriod=period;";
    html += "  document.querySelectorAll('.tab-btn').forEach(function(b){b.classList.remove('active');});";
    html += "  var tabBtn=document.getElementById('tab'+period.charAt(0).toUpperCase()+period.slice(1));";
    html += "  if(tabBtn) tabBtn.classList.add('active');";
    html += "  var today=new Date().toISOString().split('T')[0];";
    html += "  var start=today;";
    html += "  if(period==='day'){";
    html += "    document.getElementById('startDate').value=today;";
    html += "    document.getElementById('endDate').value=today;";
    html += "    document.getElementById('dateSelector').style.display='none';";
    html += "    var src=document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "    if(src==='ram'){ loadRAMData(); return; }";
    html += "  } else if(period==='week'){";
    html += "    start=new Date(Date.now()-7*24*60*60*1000).toISOString().split('T')[0];";
    html += "  } else if(period==='month'){";
    html += "    start=new Date(Date.now()-30*24*60*60*1000).toISOString().split('T')[0];";
    html += "  } else if(period==='year'){";
    html += "    start=new Date(Date.now()-365*24*60*60*1000).toISOString().split('T')[0];";
    html += "  } else if(period==='custom'){";
    html += "    document.getElementById('dateSelector').style.display='block';";
    html += "    document.querySelector('input[name=\"dataSource\"][value=\"sheets\"]').checked=true;";
    html += "    return;";
    html += "  }";
    html += "  document.getElementById('startDate').value=start;";
    html += "  document.getElementById('endDate').value=today;";
    html += "  document.getElementById('dateSelector').style.display='block';";
    html += "  document.querySelector('input[name=\"dataSource\"][value=\"sheets\"]').checked=true;";
    html += "  loadDataForCurrentSelection();";
    html += "}";

    html += "function switchDataSource(){";
    html += "  var source=document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "  var dateSelector=document.getElementById('dateSelector');";
    html += "  if(source==='ram'){";
    html += "    selectPeriod('day');";
    html += "  } else {";
    html += "    var today=new Date().toISOString().split('T')[0];";
    html += "    if(!document.getElementById('startDate').value) document.getElementById('startDate').value=today;";
    html += "    if(!document.getElementById('endDate').value) document.getElementById('endDate').value=today;";
    html += "    dateSelector.style.display='block';";
    html += "    var status=document.getElementById('loadStatus');";
    html += "    if(source==='sheets'){";
    html += "      if(status) status.textContent='ℹ️ Оберіть дати та натисніть «Завантажити»';";
    html += "    } else {";
    html += "      loadDataForCurrentSelection();";
    html += "    }";
    html += "  }";
    html += "}";

    html += "function loadDataForCurrentSelection(){";
    html += "  var source=document.querySelector('input[name=\"dataSource\"]:checked').value;";
    html += "  var startDate=document.getElementById('startDate').value;";
    html += "  var endDate=document.getElementById('endDate').value;";
    html += "  if(source==='ram'){ loadRAMData(); }";
    html += "  else if(source==='spiffs'){ loadDeviceEnergyData(startDate, endDate); }";
    html += "  else { loadSheetsData(startDate, endDate); }";
    html += "}";

    html += "function updateDatasets(){";
    html += "  var showPower=document.getElementById('chkPower').checked;";
    html += "  var showVoltage=document.getElementById('chkVoltage').checked;";
    html += "  powerChart.data.datasets[0].hidden=!showPower;";
    html += "  powerChart.data.datasets[1].hidden=!showVoltage;";
    html += "  powerChart.update('none');";
    html += "}";

    html += "function updatePeriodSummary(kwh, avgW, maxW){";
    html += "  document.getElementById('periodEnergy').textContent = (kwh !== null && !isNaN(kwh)) ? kwh.toFixed(3) : '--';";
    html += "  document.getElementById('periodAvgPower').textContent = (avgW !== null && !isNaN(avgW)) ? Math.round(avgW) : '--';";
    html += "  document.getElementById('periodMaxPower').textContent = (maxW !== null && !isNaN(maxW)) ? Math.round(maxW) : '--';";
    html += "}";
    html += "";
    html += "async function loadRAMData(){";
    html += "  var status=document.getElementById('loadStatus');";
    html += "  if(status) status.textContent='⏳ Оновлення...';";
    html += "  try{";
    html += "    var r=await fetch('/energy/power');";
    html += "    var j=await r.json();";
    html += "    var data=j.data||[];";
    html += "    data=data.filter(function(d){ return d && d.t > 1000000000; });";
    html += "    data.sort(function(a,b){ return a.t - b.t; });";
    html += "    var labels=[], fullDates=[], power=[], voltage=[];";
    html += "    var sumKwh=0, sumW=0, maxW=0;";
    html += "    for(var i=0;i<data.length;i++){";
    html += "      var d=new Date(data[i].t*1000);";
    html += "      var day=('0'+d.getDate()).slice(-2);";
    html += "      var month=('0'+(d.getMonth()+1)).slice(-2);";
    html += "      var year=d.getFullYear();";
    html += "      var hours=('0'+d.getHours()).slice(-2);";
    html += "      var minutes=('0'+d.getMinutes()).slice(-2);";
    html += "      labels.push(day+'.'+month+' '+hours+':'+minutes);";
    html += "      fullDates.push(day+'.'+month+'.'+year+' '+hours+':'+minutes);";
    html += "      var p = data[i].p || 0;";
    html += "      power.push(p);";
    html += "      voltage.push(data[i].v);";
    html += "      if(p > maxW) maxW = p;";
    html += "      sumW += p;";
    html += "      if(i > 0){";
    html += "        var dt = (data[i].t - data[i-1].t) / 3600;";
    html += "        if(dt > 0 && dt < 2){";
    html += "          sumKwh += ((p + (data[i-1].p||0)) / 2 * dt) / 1000;";
    html += "        }";
    html += "      }";
    html += "    }";
    html += "    window.chartFullDates=fullDates;";
    html += "    powerChart.data.labels=labels;";
    html += "    powerChart.data.datasets[0].data=power;";
    html += "    powerChart.data.datasets[1].data=voltage;";
    html += "    powerChart.resetZoom();";
    html += "    powerChart.update('none');";
    html += "    var avgW = data.length > 0 ? (sumW / data.length) : 0;";
    html += "    updatePeriodSummary(sumKwh, avgW, maxW);";
    html += "    if(status) status.textContent='✅ '+data.length+' записів';";
    html += "  }catch(e){";
    html += "    if(status) status.textContent='❌ Помилка RAM';";
    html += "  }";
    html += "}";

    html += "function loadDeviceEnergyData(startDate, endDate){";
    html += "  var status=document.getElementById('loadStatus');";
    html += "  var btn=document.getElementById('loadBtn');";
    html += "  if(!startDate || !endDate){ alert('Оберіть дати!'); return; }";
    html += "  if(btn) btn.disabled=true;";
    html += "  if(status) status.textContent='⏳ Завантаження архіву...';";
    html += "  fetch('/energy/history')";
    html += "  .then(function(r){ if(!r.ok) throw new Error('Немає файлу архіву'); return r.text(); })";
    html += "  .then(function(text){";
    html += "    var lines=text.trim().split('\\n');";
    html += "    var labels=[], fullDates=[], power=[], voltage=[];";
    html += "    var startTs=new Date(startDate+'T00:00:00').getTime()/1000;";
    html += "    var endTs=new Date(endDate+'T23:59:59').getTime()/1000;";
    html += "    var prevEnergy=null, firstEnergy=null, lastEnergy=null;";
    html += "    var sumW=0, maxW=0;";
    html += "    lines.forEach(function(l){";
    html += "      var parts=l.split(',');";
    html += "      if(parts.length<2) return;";
    html += "      var ts=parseInt(parts[0]);";
    html += "      var kwh=parseFloat(parts[1]);";
    html += "      if(isNaN(ts) || isNaN(kwh) || ts<startTs || ts>endTs) return;";
    html += "      if(firstEnergy===null) firstEnergy=kwh;";
    html += "      lastEnergy=kwh;";
    html += "      var d=new Date(ts*1000);";
    html += "      var day=('0'+d.getDate()).slice(-2);";
    html += "      var month=('0'+(d.getMonth()+1)).slice(-2);";
    html += "      var hours=('0'+d.getHours()).slice(-2);";
    html += "      var minutes=('0'+d.getMinutes()).slice(-2);";
    html += "      labels.push(day+'.'+month+' '+hours+':'+minutes);";
    html += "      fullDates.push(day+'.'+month+'.'+d.getFullYear()+' '+hours+':'+minutes);";
    html += "      var pEst=(prevEnergy!==null && kwh>=prevEnergy) ? (kwh-prevEnergy)*1000 : 0;";
    html += "      prevEnergy=kwh;";
    html += "      power.push(pEst);";
    html += "      voltage.push(230.0);";
    html += "      if(pEst>maxW) maxW=pEst;";
    html += "      sumW+=pEst;";
    html += "    });";
    html += "    if(labels.length===0){";
    html += "      if(status) status.textContent='⚠️ Немає даних за період';";
    html += "      updatePeriodSummary(null, null, null);";
    html += "      if(btn) btn.disabled=false; return;";
    html += "    }";
    html += "    window.chartFullDates=fullDates;";
    html += "    powerChart.data.labels=labels;";
    html += "    powerChart.data.datasets[0].data=power;";
    html += "    powerChart.data.datasets[1].data=voltage;";
    html += "    powerChart.resetZoom();";
    html += "    powerChart.update('none');";
    html += "    var sumKwh=(firstEnergy!==null && lastEnergy!==null && lastEnergy>=firstEnergy) ? (lastEnergy-firstEnergy) : 0;";
    html += "    var avgW=power.length>0 ? (sumW/power.length) : 0;";
    html += "    updatePeriodSummary(sumKwh, avgW, maxW);";
    html += "    if(status) status.textContent='✅ '+labels.length+' записів';";
    html += "    if(btn) btn.disabled=false;";
    html += "  })";
    html += "  .catch(function(err){";
    html += "    if(status) status.textContent='❌ '+err.message;";
    html += "    updatePeriodSummary(null, null, null);";
    html += "    if(btn) btn.disabled=false;";
    html += "  });";
    html += "}";
    html += "";
    html += "function loadSheetsData(startDate, endDate){";
    html += "  var status=document.getElementById('loadStatus');";
    html += "  var btn=document.getElementById('loadBtn');";
    html += "  if(!startDate || !endDate){ alert('Оберіть дати!'); return; }";
    html += "  if(btn) btn.disabled=true;";
    html += "  if(status) status.textContent='⏳ Завантаження Sheets...';";
    html += "  var controller=new AbortController();";
    html += "  var timeoutId=setTimeout(function(){ controller.abort(); }, 35000);";
    html += "  var url=SHEETS_URL+'?action=read&from='+startDate+'&to='+endDate;";
    html += "  fetch(url, { signal: controller.signal })";
    html += "  .then(function(r){ clearTimeout(timeoutId); if(!r.ok) throw new Error('HTTP '+r.status); return r.text(); })";
    html += "  .then(function(text){";
    html += "    var rows;";
    html += "    try{ rows=JSON.parse(text); }catch(e){";
    html += "      if(text.indexOf('<html')>=0 || text.indexOf('<!DOCTYPE')>=0){";
    html += "        throw new Error('Google Script вимагає авторизації (доступ: Anyone)');";
    html += "      }";
    html += "      throw new Error('Відповідь: '+text.substring(0,60));";
    html += "    }";
    html += "    if(!Array.isArray(rows) || rows.length===0){";
    html += "      if(status) status.textContent='⚠️ Немає даних за період';";
    html += "      updatePeriodSummary(null, null, null);";
    html += "      return;";
    html += "    }";
    html += "    rows.sort(function(a,b){ return (a[0]>b[0]?1:(a[0]<b[0]?-1:0)); });";
    html += "    var labels=[], fullDates=[], power=[], voltage=[];";
    html += "    var sumKwh=0, sumW=0, maxW=0;";
    html += "    var prevD=null, prevP=0;";
    html += "    rows.forEach(function(r){";
    html += "      if(!r[0]) return;";
    html += "      var dt=String(r[0]);";
    html += "      var d = new Date(dt.indexOf('T') >= 0 ? dt : dt.replace(' ', 'T'));";
    html += "      var lbl = dt.substring(5,16);";
    html += "      if(!isNaN(d.getTime())){";
    html += "        var day=('0'+d.getDate()).slice(-2);";
    html += "        var month=('0'+(d.getMonth()+1)).slice(-2);";
    html += "        var hours=('0'+d.getHours()).slice(-2);";
    html += "        var minutes=('0'+d.getMinutes()).slice(-2);";
    html += "        lbl = day + '.' + month + ' ' + hours + ':' + minutes;";
    html += "      }";
    html += "      labels.push(lbl);";
    html += "      fullDates.push(dt);";
    html += "      var v = parseFloat(r[9])||0;";
    html += "      var p = parseFloat(r[10])||0;";
    html += "      voltage.push(v);";
    html += "      power.push(p);";
    html += "      if(p>maxW) maxW=p;";
    html += "      sumW+=p;";
    html += "      if(!isNaN(d.getTime())){";
    html += "        if(prevD!==null){";
    html += "          var dtHours=(d.getTime()-prevD.getTime())/3600000;";
    html += "          if(dtHours>0 && dtHours<24){";
    html += "            sumKwh+=((p+prevP)/2*dtHours)/1000;";
    html += "          }";
    html += "        }";
    html += "        prevD=d; prevP=p;";
    html += "      }";
    html += "    });";
    html += "    window.chartFullDates=fullDates;";
    html += "    powerChart.data.labels=labels;";
    html += "    powerChart.data.datasets[0].data=power;";
    html += "    powerChart.data.datasets[1].data=voltage;";
    html += "    powerChart.resetZoom();";
    html += "    powerChart.update('none');";
    html += "    var avgW=power.length>0 ? (sumW/power.length) : 0;";
    html += "    updatePeriodSummary(sumKwh, avgW, maxW);";
    html += "    if(status) status.textContent='✅ '+rows.length+' записів';";
    html += "  })";
    html += "  .catch(function(err){";
    html += "    clearTimeout(timeoutId);";
    html += "    if(err.name==='AbortError'){";
    html += "      if(status) status.textContent='❌ Таймаут Google Sheets (перевірте зв\\'язок)';";
    html += "    } else {";
    html += "      if(status) status.textContent='❌ '+err.message;";
    html += "    }";
    html += "    updatePeriodSummary(null, null, null);";
    html += "  })";
    html += "  .finally(function(){";
    html += "    if(btn) btn.disabled=false;";
    html += "  });";
    html += "}";

    html += "async function upd(){";
    html += "  try{";
    html += "    var r=await fetch('/energy/api');var d=await r.json();";
    html += "    if(!d.err){";
    html += "      document.getElementById('v').innerText=d.v.toFixed(1);";
    html += "      document.getElementById('c').innerText=d.c.toFixed(2);";
    html += "      document.getElementById('p').innerText=d.p.toFixed(0);";
    html += "      document.getElementById('e').innerText=d.e.toFixed(2);";
    html += "      document.getElementById('f').innerText=d.f.toFixed(1);";
    html += "      document.getElementById('pf').innerText=d.pf.toFixed(2);";
    html += "      document.getElementById('st').style.background='#4CAF50';";
    html += "      document.getElementById('st').innerText='ONLINE';";
    html += "    } else {";
    html += "      document.getElementById('st').style.background='#f44336';";
    html += "      document.getElementById('st').innerText='ПОМИЛКА';";
    html += "    }";
    html += "  }catch(e){";
    html += "    document.getElementById('st').style.background='#f44336';";
    html += "    document.getElementById('st').innerText='OFFLINE';";
    html += "  }";
    html += "}";

    html += "async function resetKwh(){";
    html += "  if(!confirm('Скинути лічильник kWh на 0?')) return;";
    html += "  if(!confirm('Ви впевнені? Дані буде втрачено!')) return;";
    html += "  var r=await fetch('/energy/reset',{method:'POST'});";
    html += "  if(r.ok){";
    html += "    document.getElementById('rstBtn').innerText='Скинуто!';";
    html += "    setTimeout(function(){document.getElementById('rstBtn').innerText='🔄 Скинути лічильник kWh';},2000);";
    html += "  }";
    html += "}";

    html += "initCharts();";
    html += "loadRAMData();";
    html += "setInterval(upd,2000); upd();";
    html += "setInterval(function(){ if(currentPeriod==='day' && document.querySelector('input[name=\"dataSource\"]:checked').value==='ram') loadRAMData(); }, 120000);";
    html += "</script>";

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();
    server.send(200, "text/html", html);
}
