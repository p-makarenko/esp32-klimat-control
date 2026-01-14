#include "energy_monitor.h"
#include "system_core.h"
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
int currentEnergyHour = -1;

// Умовна флаг для активації цього модуля
// Встановіть ENABLE_ENERGY_MONITOR = 1 у platformio.ini або config.h якщо у вас є PZEM004Tv30
#ifndef ENABLE_ENERGY_MONITOR
#define ENABLE_ENERGY_MONITOR 0
#endif

#if ENABLE_ENERGY_MONITOR
#include <PZEM004Tv30.h>
// Налаштування пінів для PZEM004Tv30
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
PZEM004Tv30 pzem(&Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
#endif

// ============================================================================
// ІНІЦІАЛІЗАЦІЯ
// ============================================================================

void initEnergyMonitor() {
#if ENABLE_ENERGY_MONITOR
    Serial.println("⚙️  Ініціалізація енергоконтролера...");

    // Ініціалізуємо LittleFS для історії
    if (!LittleFS.begin(true)) {
        Serial.println("❌ Помилка: не вдалося ініціалізувати LittleFS");
        return;
    }

    // Перевіряємо чи існує файл історії
    if (!LittleFS.exists("/energy_history.csv")) {
        File f = LittleFS.open("/energy_history.csv", "w");
        if (f) {
            f.println("timestamp,energy");
            f.close();
            Serial.println("✅ Файл історії енергії створено");
        }
    }

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

    // Оновлюємо кожні 2 секунди
    if (now - lastEnergyUpdate < 2000) {
        return;
    }
    lastEnergyUpdate = now;

    // Читаємо дані з PZEM004Tv30
    float v = pzem.voltage();

    if (!isnan(v)) {
        energyData.voltage = v;
        energyData.current = pzem.current();
        energyData.power = pzem.power();
        energyData.energy = pzem.energy();
        energyData.frequency = pzem.frequency();
        energyData.powerFactor = pzem.pf();
        energyData.error = false;

        // Логіка запису історії - щогодини
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            if (currentEnergyHour != timeinfo.tm_hour) {
                if (currentEnergyHour != -1) {
                    appendEnergyHistory(energyData.energy);
                }
                currentEnergyHour = timeinfo.tm_hour;
            }
        }
    } else {
        energyData.error = true;
        Serial.println("⚠️  Помилка читання енергоконтролера");
    }
#endif
}

// ============================================================================
// ЗАПИС ІСТОРІЇ
// ============================================================================

void appendEnergyHistory(float energy) {
#if ENABLE_ENERGY_MONITOR
    if (isnan(energy) || energy <= 0) return;

    File f = LittleFS.open("/energy_history.csv", "a");
    if (f) {
        time_t now;
        time(&now);
        f.printf("%lu,%.3f\n", now, energy);
        f.close();
        Serial.printf("📊 Енергія записана: %.3f kWh\n", energy);
    } else {
        Serial.println("❌ Помилка запису історії енергії");
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

void handleEnergyHistory() {
#if ENABLE_ENERGY_MONITOR
    if (!checkAuth()) return;

    File f = LittleFS.open("/energy_history.csv", "r");
    if (f) {
        String content = "";
        while (f.available()) {
            content += (char)f.read();
        }
        f.close();
        server.send(200, "text/plain", content);
    } else {
        server.send(404, "text/plain", "Файл не знайдено");
    }
#else
    server.send(503, "text/plain", "Енергоконтролер вимкнено");
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
    bool firstLine = true;

    while (f.available()) {
        int b = f.read();
        if (b == '\n' || !f.available()) {
            if (!firstLine && line.length() > 0) {
                int commaPos = line.indexOf(',');
                if (commaPos > 0) {
                    String valStr = line.substring(commaPos + 1);
                    float energy = valStr.toFloat();
                    totalEnergy += energy;
                    recordCount++;
                }
            }
            firstLine = false;
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

void handleEnergyPage() {
    if (!checkAuth()) return;

    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Енергоконтролер</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/moment@2.29.4/moment.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-moment@1.0.1/dist/chartjs-adapter-moment.min.js"></script>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root { --bg: #0f172a; --card: #1e293b; --accent: #38bdf8; --text: #f1f5f9; }
        body { font-family: 'Inter', system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 15px; }
        .container { max-width: 900px; margin: 0 auto; }
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; }
        .status { font-size: 0.8rem; padding: 5px 12px; border-radius: 20px; background: #ef4444; }
        .online { background: #22c55e; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 15px; margin-bottom: 25px; }
        .card { background: var(--card); padding: 20px; border-radius: 16px; border: 1px solid #334155; text-align: center; }
        .card i { color: var(--accent); font-size: 1.2rem; margin-bottom: 10px; }
        .val { font-size: 1.6rem; font-weight: 700; display: block; margin: 5px 0; }
        .unit { font-size: 0.75rem; color: #94a3b8; }
        .chart-container { background: var(--card); padding: 20px; border-radius: 20px; border: 1px solid #334155; }
        .tabs { display: flex; gap: 8px; margin-bottom: 20px; }
        .tab { padding: 8px 16px; border-radius: 8px; cursor: pointer; border: none; background: transparent; color: #94a3b8; }
        .tab.active { background: var(--accent); color: var(--bg); }
        canvas { max-height: 350px; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div><h2 style="margin:0">Energy<span style="color:var(--accent)">Pro</span></h2></div>
            <div id="statusBtn" class="status">OFFLINE</div>
        </header>

        <div class="grid">
            <div class="card"><i class="fas fa-bolt"></i><span class="val" id="v">0</span><span class="unit">Напруга (V)</span></div>
            <div class="card"><i class="fas fa-microchip"></i><span class="val" id="c">0</span><span class="unit">Струм (A)</span></div>
            <div class="card"><i class="fas fa-fire"></i><span class="val" id="p">0</span><span class="unit">Потужність (W)</span></div>
            <div class="card"><i class="fas fa-chart-line"></i><span class="val" id="e">0</span><span class="unit">Всього (kWh)</span></div>
        </div>

        <div class="chart-container">
            <div class="tabs">
                <button class="tab active" onclick="changeMode('live', this)">LIVE</button>
                <button class="tab" onclick="changeMode('day', this)">ДЕНЬ</button>
                <button class="tab" onclick="changeMode('month', this)">МІСЯЦЬ</button>
            </div>
            <canvas id="energyChart"></canvas>
        </div>
    </div>

    <script>
        let chart;
        let mode = 'live';
        let livePoints = [];

        function initChart() {
            const ctx = document.getElementById('energyChart').getContext('2d');
            chart = new Chart(ctx, {
                type: 'line',
                data: { datasets: [] },
                options: {
                    responsive: true,
                    scales: {
                        x: { type: 'time', grid: { display: false } },
                        y: { grid: { color: '#334155' } }
                    },
                    plugins: { legend: { display: false } }
                }
            });
        }

        async function updateLive() {
            if(mode !== 'live') return;
            try {
                const r = await fetch('/energy/api');
                const d = await r.json();
                if(d.err) return;

                document.getElementById('v').innerText = d.v.toFixed(1);
                document.getElementById('c').innerText = d.c.toFixed(2);
                document.getElementById('p').innerText = d.p.toFixed(0);
                document.getElementById('e').innerText = d.e.toFixed(2);
                document.getElementById('statusBtn').className = "status online";
                document.getElementById('statusBtn').innerText = "ONLINE";

                const now = Date.now();
                livePoints.push({x: now, y: d.p});
                if(livePoints.length > 40) livePoints.shift();

                chart.data.datasets = [{
                    label: 'W',
                    data: livePoints,
                    borderColor: '#38bdf8',
                    borderWidth: 3,
                    tension: 0.4,
                    fill: true,
                    backgroundColor: 'rgba(56, 189, 248, 0.1)',
                    pointRadius: 0
                }];
                chart.update('none');
            } catch(e) {
                document.getElementById('statusBtn').className = "status";
                document.getElementById('statusBtn').innerText = "ERROR";
            }
        }

        function changeMode(m, el) {
            mode = m;
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            el.classList.add('active');
        }

        initChart();
        setInterval(updateLive, 2000);
    </script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}
