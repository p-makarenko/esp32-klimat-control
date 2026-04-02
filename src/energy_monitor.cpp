#include "energy_monitor.h"
#include "system_core.h"
#include "config.h"
#include "web_common.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

// Forward declaration
bool checkAuth();

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
            energyData.voltage   = (resp[3] << 8 | resp[4]) * 0.1f;
            uint32_t rawCurrent  = (resp[5] << 8 | resp[6]) | ((uint32_t)(resp[7] << 8 | resp[8]) << 16);
            energyData.current   = rawCurrent * 0.001f;
            uint32_t rawPower    = (resp[9] << 8 | resp[10]) | ((uint32_t)(resp[11] << 8 | resp[12]) << 16);
            energyData.power     = rawPower * 0.1f;
            uint32_t rawEnergy   = (resp[13] << 8 | resp[14]) | ((uint32_t)(resp[15] << 8 | resp[16]) << 16);
            energyData.energy    = rawEnergy * 0.001f;
            energyData.frequency = (resp[17] << 8 | resp[18]) * 0.1f;
            energyData.powerFactor = (resp[19] << 8 | resp[20]) * 0.01f;
            energyData.error = false;
        } else {
            energyData.error = true;
        }
    } else {
        energyData.error = true;
    }

    // Запис в RAM буфер кожні 2 хвилини
    if (!energyData.error && (now - lastPowerRecord >= 120000 || lastPowerRecord == 0)) {
        lastPowerRecord = now;
        time_t t;
        time(&t);
        if (t > 1000000) {  // Час синхронізовано
            powerBuffer[powerBufferIndex].timestamp = (uint32_t)t;
            powerBuffer[powerBufferIndex].power = energyData.power;
            powerBuffer[powerBufferIndex].voltage = energyData.voltage;
            powerBuffer[powerBufferIndex].current = energyData.current;
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
        if (powerBuffer[idx].timestamp == 0) continue;
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

    // Вкладки для графіка потужності
    html += "<div class='chart-wrapper'>";
    html += "<div class='chart-header'>";
    html += "<h2>⚡ Потужність та Напруга</h2>";
    html += "<button onclick='powerChart.resetZoom()' class='reset-btn'>🔄 Скинути масштаб</button>";
    html += "</div>";

    // Вкладки день / тиждень / місяць / рік
    html += "<div style='display:flex;gap:5px;margin-bottom:10px;flex-wrap:wrap'>";
    html += "<button class='tab-btn active' id='tabDay' onclick='switchTab(\"day\")'>День</button>";
    html += "<button class='tab-btn' id='tabWeek' onclick='switchTab(\"week\")'>Тиждень</button>";
    html += "<button class='tab-btn' id='tabMonth' onclick='switchTab(\"month\")'>Місяць</button>";
    html += "<button class='tab-btn' id='tabYear' onclick='switchTab(\"year\")'>Рік</button>";
    html += "<div style='margin-left:auto;display:flex;gap:10px;align-items:center'>";
    html += "<label style='font-size:0.9rem'><input type='checkbox' id='chkPower' checked onchange='updateDatasets()'>Потужність (W)</label>";
    html += "<label style='font-size:0.9rem'><input type='checkbox' id='chkVoltage' checked onchange='updateDatasets()'>Напруга (V)</label>";
    html += "</div></div>";

    html += "<style>.tab-btn{padding:8px 16px;border:1px solid #ddd;background:#f5f5f5;border-radius:5px;cursor:pointer;font-size:0.9rem}";
    html += ".tab-btn.active{background:#f59e0b;color:#fff;border-color:#f59e0b}</style>";

    html += "<div class='chart-container' style='height:300px'><canvas id='powerCanvas'></canvas></div>";
    html += "</div>";

    // Графік енергії kWh
    html += "<div class='chart-wrapper'>";
    html += "<div class='chart-header'>";
    html += "<h2>📊 Енергія (kWh)</h2>";
    html += "<button onclick='energyChart.resetZoom()' class='reset-btn'>🔄 Скинути масштаб</button>";
    html += "</div>";
    html += "<div class='chart-container' style='height:300px'><canvas id='energyCanvas'></canvas></div>";
    html += "</div>";

    html += "</div>"; // container

    html += R"rawliteral(<script>
var powerChart, energyChart;
var allPowerData = [];
var currentTab = 'day';

var zoomCfg = {
  zoom: {
    wheel:{enabled:true,speed:0.1},
    pinch:{enabled:true},
    drag:{enabled:true,backgroundColor:'rgba(245,158,11,0.2)',borderColor:'rgba(245,158,11,0.8)',borderWidth:1,threshold:10},
    mode:'x'
  },
  pan:{enabled:true,mode:'x',threshold:5}
};

function fmtTime(ts){
  var d=new Date(ts*1000);
  return ('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2);
}
function fmtDate(ts){
  var d=new Date(ts*1000);
  return ('0'+d.getDate()).slice(-2)+'.'+('0'+(d.getMonth()+1)).slice(-2)+' '+('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2);
}
function fmtDateYear(ts){
  var d=new Date(ts*1000);
  return ('0'+d.getDate()).slice(-2)+'.'+('0'+(d.getMonth()+1)).slice(-2)+'.'+d.getFullYear();
}

function initCharts(){
  powerChart = new Chart(document.getElementById('powerCanvas'),{
    type:'line',
    data:{labels:[],datasets:[
      {
        label:'Потужність (W)',data:[],
        borderColor:'#f59e0b',backgroundColor:'rgba(245,158,11,0.1)',
        borderWidth:2,fill:true,tension:0.3,pointRadius:0,yAxisID:'y',hidden:false
      },
      {
        label:'Напруга (V)',data:[],
        borderColor:'#3498db',backgroundColor:'rgba(52,152,219,0.1)',
        borderWidth:2,fill:true,tension:0.3,pointRadius:0,yAxisID:'y1',hidden:false
      }
    ]},
    options:{
      responsive:true,maintainAspectRatio:false,animation:{duration:0},
      interaction:{mode:'index',intersect:false},
      scales:{
        x:{ticks:{maxTicksLimit:10,font:{size:11}},grid:{display:false}},
        y:{position:'left',beginAtZero:true,title:{display:true,text:'W'},ticks:{font:{size:11}},grid:{color:'#eee'}},
        y1:{position:'right',beginAtZero:true,title:{display:true,text:'V'},ticks:{font:{size:11}},grid:{display:false}}
      },
      plugins:{
        legend:{display:true,position:'top'},
        tooltip:{callbacks:{
          title:function(c){return c[0].label;},
          label:function(c){
            var ds=c.dataset.label;
            return ds+': '+c.parsed.y.toFixed(c.datasetIndex===0?1:1);
          }
        }},
        zoom:zoomCfg
      }
    }
  });

  energyChart = new Chart(document.getElementById('energyCanvas'),{
    type:'bar',
    data:{labels:[],datasets:[{
      label:'Енергія (kWh)',data:[],
      backgroundColor:'rgba(33,150,243,0.6)',borderColor:'#2196F3',borderWidth:1
    }]},
    options:{
      responsive:true,maintainAspectRatio:false,animation:{duration:300},
      scales:{
        x:{ticks:{font:{size:11}},grid:{display:false}},
        y:{beginAtZero:true,title:{display:true,text:'kWh'},ticks:{font:{size:11}},grid:{color:'#eee'}}
      },
      plugins:{
        legend:{display:false},
        tooltip:{callbacks:{
          label:function(c){return c.parsed.y.toFixed(3)+' kWh';}
        }},
        zoom:zoomCfg
      }
    }
  });

  document.getElementById('powerCanvas').ondblclick=function(){powerChart.resetZoom();};
  document.getElementById('energyCanvas').ondblclick=function(){energyChart.resetZoom();};

  loadPowerData();
  loadEnergyHistory();
}

function switchTab(tab){
  currentTab=tab;
  document.querySelectorAll('.tab-btn').forEach(function(b){b.classList.remove('active');});
  var tabId='tab'+tab.charAt(0).toUpperCase()+tab.slice(1);
  if(document.getElementById(tabId)) document.getElementById(tabId).classList.add('active');
  filterPowerData(tab);
}

function updateDatasets(){
  var showPower=document.getElementById('chkPower').checked;
  var showVoltage=document.getElementById('chkVoltage').checked;
  powerChart.data.datasets[0].hidden=!showPower;
  powerChart.data.datasets[1].hidden=!showVoltage;
  powerChart.update('none');
}

function filterPowerData(period){
  var now=Math.floor(Date.now()/1000);
  var cutoff=now;
  if(period=='day') cutoff=now-86400;
  else if(period=='week') cutoff=now-604800;
  else if(period=='month') cutoff=now-2592000;
  else if(period=='year') cutoff=now-31536000;

  var labels=[],power=[],voltage=[];
  for(var i=0;i<allPowerData.length;i++){
    if(allPowerData[i].t>=cutoff){
      if(period=='day') labels.push(fmtTime(allPowerData[i].t));
      else if(period=='year') labels.push(fmtDateYear(allPowerData[i].t));
      else labels.push(fmtDate(allPowerData[i].t));
      power.push(allPowerData[i].p);
      voltage.push(allPowerData[i].v);
    }
  }
  powerChart.data.labels=labels;
  powerChart.data.datasets[0].data=power;
  powerChart.data.datasets[1].data=voltage;
  powerChart.resetZoom();
  powerChart.update('none');
}

async function loadPowerData(){
  try{
    var r=await fetch('/energy/power');
    var j=await r.json();
    allPowerData=j.data||[];
    filterPowerData('day');
  }catch(e){}
}

async function loadEnergyHistory(){
  try{
    var r=await fetch('/energy/history');
    var txt=await r.text();
    var lines=txt.trim().split('\n');
    var hourly={};
    for(var i=0;i<lines.length;i++){
      var parts=lines[i].split(',');
      if(parts.length<2) continue;
      var ts=parseInt(parts[0]),val=parseFloat(parts[1]);
      if(isNaN(ts)||isNaN(val)) continue;
      var d=new Date(ts*1000);
      var key=d.getFullYear()+'-'+('0'+(d.getMonth()+1)).slice(-2)+'-'+('0'+d.getDate()).slice(-2)+' '+('0'+d.getHours()).slice(-2)+':00';
      hourly[key]=val;
    }
    var labels=Object.keys(hourly);
    var data=labels.map(function(k){return hourly[k];});
    // Показуємо дату і час
    var fmtLabels=labels.map(function(k){
      var p=k.split(' ');
      var dp=p[0].split('-');
      return dp[2]+'.'+dp[1]+' '+p[1];
    });
    energyChart.data.labels=fmtLabels;
    energyChart.data.datasets[0].data=data;
    energyChart.update();
  }catch(e){}
}

async function upd(){
  try{
    var r=await fetch('/energy/api');var d=await r.json();
    if(!d.err){
      document.getElementById('v').innerText=d.v.toFixed(1);
      document.getElementById('c').innerText=d.c.toFixed(2);
      document.getElementById('p').innerText=d.p.toFixed(0);
      document.getElementById('e').innerText=d.e.toFixed(2);
      document.getElementById('f').innerText=d.f.toFixed(1);
      document.getElementById('pf').innerText=d.pf.toFixed(2);
      document.getElementById('st').style.background='#4CAF50';
      document.getElementById('st').innerText='ONLINE';
    } else {
      document.getElementById('st').style.background='#f44336';
      document.getElementById('st').innerText='ПОМИЛКА';
    }
  }catch(e){
    document.getElementById('st').style.background='#f44336';
    document.getElementById('st').innerText='OFFLINE';
  }
}

async function resetKwh(){
  if(!confirm('Скинути лічильник kWh на 0?')) return;
  if(!confirm('Ви впевнені? Дані буде втрачено!')) return;
  var r=await fetch('/energy/reset',{method:'POST'});
  if(r.ok){
    document.getElementById('rstBtn').innerText='Скинуто!';
    setTimeout(function(){document.getElementById('rstBtn').innerText='🔄 Скинути лічильник kWh';},2000);
  }
}

initCharts();
setInterval(upd,2000); upd();
// Оновлюємо графік потужності з RAM кожні 2 хв
setInterval(loadPowerData,120000);
</script>)rawliteral";

    html += getHtmlFooter();
    server.send(200, "text/html", html);
}
