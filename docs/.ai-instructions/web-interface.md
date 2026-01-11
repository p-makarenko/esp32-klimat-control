# Web Interface & API Patterns

## Architecture

**Server:** ESPAsyncWebServer
**Frontend:** HTML/CSS/JS (served from SPIFFS)
**Data exchange:** JSON over REST API
**Real-time:** AJAX polling (or WebSocket if needed)

---

## File Structure

```
data/web/
├── index.html          # Main dashboard
├── settings.html       # Configuration page
├── style.css           # Shared styles
├── script.js           # Shared JavaScript
└── update.html         # OTA update page
```

---

## API Endpoints Pattern

### RESTful Structure

```cpp
// GET - retrieve data
server.on("/api/data", HTTP_GET, handleGetData);
server.on("/api/config", HTTP_GET, handleGetConfig);
server.on("/api/status", HTTP_GET, handleGetStatus);

// POST - update/control
server.on("/api/control", HTTP_POST, handleControl);
server.on("/api/config", HTTP_POST, handleUpdateConfig);

// Specific resources
server.on("/api/sensors/temperature", HTTP_GET, handleGetTemp);
server.on("/api/actuators/pump", HTTP_POST, handlePumpControl);
```

### Response Format

**Success:**
```json
{
  "status": "ok",
  "data": {
    "temperature": 22.5,
    "humidity": 65.3
  },
  "timestamp": 1704988800
}
```

**Error:**
```json
{
  "status": "error",
  "code": "INVALID_TEMP",
  "message": "Температура має бути між 5°C та 40°C",
  "details": {
    "provided": 45.0,
    "min": 5.0,
    "max": 40.0
  }
}
```

---

## Data Endpoint

### Current Readings

```cpp
void handleGetData(AsyncWebServerRequest *request) {
    // Gather all sensor data
    float temp = sensors.getTemperature();
    float humidity = sensors.getHumidity();
    
    // Build JSON response
    StaticJsonDocument<512> doc;
    doc["status"] = "ok";
    
    JsonObject data = doc.createNestedObject("data");
    data["temperature"] = round(temp * 10) / 10.0;  // 1 decimal
    data["humidity"] = round(humidity * 10) / 10.0;
    data["pressure"] = sensors.getPressure();
    
    // System info
    JsonObject system = data.createNestedObject("system");
    system["mode"] = getModeString();
    system["uptime"] = millis() / 1000;
    system["wifi_rssi"] = WiFi.RSSI();
    system["free_heap"] = ESP.getFreeHeap();
    
    // Actuator states
    JsonObject actuators = data.createNestedObject("actuators");
    actuators["pump"] = currentPumpPWM;
    actuators["fan"] = currentFanPWM;
    actuators["heating"] = isHeatingActive;
    
    doc["timestamp"] = time(nullptr);
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
```

**Response example:**
```json
{
  "status": "ok",
  "data": {
    "temperature": 22.5,
    "humidity": 65.3,
    "pressure": 1013.2,
    "system": {
      "mode": "AUTO",
      "uptime": 86400,
      "wifi_rssi": -45,
      "free_heap": 245760
    },
    "actuators": {
      "pump": 180,
      "fan": 120,
      "heating": true
    }
  },
  "timestamp": 1704988800
}
```

---

## Control Endpoint

### Manual Actuator Control

```cpp
void handleControl(AsyncWebServerRequest *request) {
    // Validate request
    if (!request->hasParam("actuator", true) || 
        !request->hasParam("value", true)) {
        request->send(400, "application/json", 
                     "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
        return;
    }
    
    String actuator = request->getParam("actuator", true)->value();
    int value = request->getParam("value", true)->value().toInt();
    
    // Validate value range
    if (value < 0 || value > 255) {
        request->send(400, "application/json",
                     "{\"status\":\"error\",\"message\":\"Value must be 0-255\"}");
        return;
    }
    
    // Apply control
    bool success = false;
    if (actuator == "pump") {
        ledcWrite(PUMP_PWM_CHANNEL, value);
        currentPumpPWM = value;
        success = true;
    } else if (actuator == "fan") {
        ledcWrite(FAN_PWM_CHANNEL, value);
        currentFanPWM = value;
        success = true;
    } else {
        request->send(400, "application/json",
                     "{\"status\":\"error\",\"message\":\"Unknown actuator\"}");
        return;
    }
    
    if (success) {
        StaticJsonDocument<128> doc;
        doc["status"] = "ok";
        doc["actuator"] = actuator;
        doc["value"] = value;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
        
        Serial.printf("Manual control: %s = %d\n", actuator.c_str(), value);
    }
}
```

---

## Configuration Endpoints

### Get Configuration

```cpp
void handleGetConfig(AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> doc;
    doc["status"] = "ok";
    
    JsonObject cfg = doc.createNestedObject("config");
    
    // Temperature settings
    JsonObject temp = cfg.createNestedObject("temperature");
    temp["target"] = config.targetTemp;
    temp["min"] = config.minTemp;
    temp["max"] = config.maxTemp;
    temp["emergency"] = config.emergencyTemp;
    temp["hysteresis"] = config.hysteresis;
    
    // PID coefficients
    JsonObject pid = cfg.createNestedObject("pid");
    pid["kp"] = config.pidKp;
    pid["ki"] = config.pidKi;
    pid["kd"] = config.pidKd;
    
    // Network (passwords hidden)
    JsonArray networks = cfg.createNestedArray("networks");
    for (int i = 0; i < config.wifiCount; i++) {
        JsonObject net = networks.createObject();
        net["ssid"] = config.wifiSSID[i];
        net["password"] = "********";  // Hidden
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
```

### Update Configuration

```cpp
void handleUpdateConfig(AsyncWebServerRequest *request) {
    // Parse JSON body (requires AsyncJson library or manual parsing)
    
    if (request->hasParam("target_temp", true)) {
        float newTemp = request->getParam("target_temp", true)->value().toFloat();
        
        if (validateTemperature(newTemp)) {
            config.targetTemp = newTemp;
            saveConfig();
            
            request->send(200, "application/json", 
                         "{\"status\":\"ok\",\"message\":\"Temperature updated\"}");
        } else {
            request->send(400, "application/json",
                         "{\"status\":\"error\",\"message\":\"Invalid temperature\"}");
        }
    }
    
    // Similar handlers for other config parameters
}
```

---

## Frontend Integration

### Dashboard (index.html)

```html
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Klimat Control</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <header>
        <h1>🌱 Klimat Control</h1>
        <div id="status">Підключення...</div>
    </header>
    
    <main>
        <section class="readings">
            <div class="card">
                <h2>🌡️ Температура</h2>
                <div class="value" id="temperature">--</div>
                <div class="unit">°C</div>
            </div>
            
            <div class="card">
                <h2>💧 Вологість</h2>
                <div class="value" id="humidity">--</div>
                <div class="unit">%</div>
            </div>
            
            <div class="card">
                <h2>⚙️ Режим</h2>
                <div class="value" id="mode">--</div>
            </div>
        </section>
        
        <section class="controls">
            <h2>Керування</h2>
            <div class="control-group">
                <label>Насос (0-255):</label>
                <input type="range" id="pumpSlider" min="0" max="255" value="0">
                <span id="pumpValue">0</span>
            </div>
            <div class="control-group">
                <label>Вентилятор (0-255):</label>
                <input type="range" id="fanSlider" min="0" max="255" value="0">
                <span id="fanValue">0</span>
            </div>
        </section>
    </main>
    
    <script src="script.js"></script>
</body>
</html>
```

### JavaScript (script.js)

```javascript
// Update interval
const UPDATE_INTERVAL = 2000;  // 2 seconds

// Fetch data from ESP32
async function fetchData() {
    try {
        const response = await fetch('/api/data');
        const json = await response.json();
        
        if (json.status === 'ok') {
            updateUI(json.data);
            document.getElementById('status').textContent = 'Онлайн ✓';
            document.getElementById('status').className = 'online';
        }
    } catch (error) {
        console.error('Помилка отримання даних:', error);
        document.getElementById('status').textContent = 'Офлайн ✗';
        document.getElementById('status').className = 'offline';
    }
}

// Update UI elements
function updateUI(data) {
    document.getElementById('temperature').textContent = data.temperature.toFixed(1);
    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
    document.getElementById('mode').textContent = data.system.mode;
    
    // Update sliders if not being dragged
    if (!document.getElementById('pumpSlider').matches(':active')) {
        document.getElementById('pumpSlider').value = data.actuators.pump;
        document.getElementById('pumpValue').textContent = data.actuators.pump;
    }
    
    if (!document.getElementById('fanSlider').matches(':active')) {
        document.getElementById('fanSlider').value = data.actuators.fan;
        document.getElementById('fanValue').textContent = data.actuators.fan;
    }
}

// Send control command
async function sendControl(actuator, value) {
    try {
        const formData = new FormData();
        formData.append('actuator', actuator);
        formData.append('value', value);
        
        const response = await fetch('/api/control', {
            method: 'POST',
            body: formData
        });
        
        const json = await response.json();
        if (json.status !== 'ok') {
            console.error('Control error:', json.message);
            alert('Помилка: ' + json.message);
        }
    } catch (error) {
        console.error('Network error:', error);
    }
}

// Setup slider event handlers
document.getElementById('pumpSlider').addEventListener('change', (e) => {
    const value = parseInt(e.target.value);
    sendControl('pump', value);
    document.getElementById('pumpValue').textContent = value;
});

document.getElementById('fanSlider').addEventListener('change', (e) => {
    const value = parseInt(e.target.value);
    sendControl('fan', value);
    document.getElementById('fanValue').textContent = value;
});

// Auto-refresh data
setInterval(fetchData, UPDATE_INTERVAL);
fetchData();  // Initial fetch
```

---

## Styling (style.css)

```css
:root {
    --primary: #4CAF50;
    --danger: #f44336;
    --warning: #ff9800;
    --bg: #f5f5f5;
    --card-bg: white;
    --text: #333;
}

* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background: var(--bg);
    color: var(--text);
}

header {
    background: var(--primary);
    color: white;
    padding: 1rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
}

#status {
    padding: 0.5rem 1rem;
    border-radius: 4px;
    font-weight: bold;
}

#status.online {
    background: rgba(255,255,255,0.2);
}

#status.offline {
    background: var(--danger);
}

main {
    max-width: 1200px;
    margin: 2rem auto;
    padding: 0 1rem;
}

.readings {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 1rem;
    margin-bottom: 2rem;
}

.card {
    background: var(--card-bg);
    padding: 1.5rem;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    text-align: center;
}

.card h2 {
    font-size: 1rem;
    margin-bottom: 1rem;
    color: #666;
}

.card .value {
    font-size: 3rem;
    font-weight: bold;
    color: var(--primary);
}

.card .unit {
    font-size: 1rem;
    color: #999;
}

.controls {
    background: var(--card-bg);
    padding: 1.5rem;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.control-group {
    margin: 1rem 0;
    display: grid;
    grid-template-columns: 150px 1fr 50px;
    gap: 1rem;
    align-items: center;
}

input[type="range"] {
    width: 100%;
}

/* Mobile responsive */
@media (max-width: 600px) {
    .control-group {
        grid-template-columns: 1fr;
    }
}
```

---

## WebSocket (Optional for Real-Time)

**When to use:**
- Very frequent updates (< 1s interval)
- Bidirectional communication needed
- Multiple clients need sync

**Implementation:**
```cpp
#include <AsyncWebSocket.h>

AsyncWebSocket ws("/ws");

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Handle incoming data
        String message = String((char*)data).substring(0, len);
        Serial.printf("Received: %s\n", message.c_str());
    }
}

void setupWebSocket() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
}

void broadcastSensorData() {
    StaticJsonDocument<256> doc;
    doc["temperature"] = sensors.getTemperature();
    doc["humidity"] = sensors.getHumidity();
    
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

// In main loop
void loop() {
    static unsigned long lastBroadcast = 0;
    if (millis() - lastBroadcast > 1000) {
        broadcastSensorData();
        lastBroadcast = millis();
    }
    ws.cleanupClients();
}
```

**Client side:**
```javascript
const ws = new WebSocket('ws://' + location.hostname + '/ws');

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    updateUI(data);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};
```

---

## Security Headers

```cpp
// Add security headers to all responses
server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not Found");
});

// CORS headers (if needed for development)
DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

// Security headers
DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
DefaultHeaders::Instance().addHeader("X-XSS-Protection", "1; mode=block");
```

---

## Error Handling Pattern

```cpp
enum ErrorCode {
    ERR_OK = 0,
    ERR_INVALID_PARAM = 400,
    ERR_UNAUTHORIZED = 401,
    ERR_NOT_FOUND = 404,
    ERR_INTERNAL = 500
};

void sendError(AsyncWebServerRequest *request, ErrorCode code, String message) {
    StaticJsonDocument<256> doc;
    doc["status"] = "error";
    doc["code"] = code;
    doc["message"] = message;
    
    String response;
    serializeJson(doc, response);
    request->send(code, "application/json", response);
}

// Usage
if (!isValid) {
    sendError(request, ERR_INVALID_PARAM, "Температура поза межами");
    return;
}
```

This pattern ensures consistent, Ukrainian-language web interface with robust API design.
