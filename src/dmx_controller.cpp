// ============================================================================
// DMX_CONTROLLER.CPP - Відправка DMX512 через HardwareSerial
// ============================================================================

#include "dmx_controller.h"

static HardwareSerial DmxSerial(DMX_UART_NUM);
static uint8_t _dmxBuffer[513];
static RGBWColor _current = {0, 0, 0, 0};
TaskHandle_t dmxTaskHandle = nullptr;

void dmxInit() {
    if (DMX_DE_RE_PIN >= 0) {
        pinMode(DMX_DE_RE_PIN, OUTPUT);
        digitalWrite(DMX_DE_RE_PIN, HIGH); // завжди TX
    }

    DmxSerial.begin(250000, SERIAL_8N2, DMX_RX_PIN, DMX_TX_PIN);
    memset(_dmxBuffer, 0, sizeof(_dmxBuffer));

    // Loopback тест
    DmxSerial.write(0xAA);
    delay(10);
    uint8_t rx = 0;
    int len = DmxSerial.readBytes(&rx, 1);
    Serial.printf("[DMX] UART тест: відправив=0xAA отримав=0x%02X len=%d\n", rx, len);

    xTaskCreatePinnedToCore(dmxTask, "DMXTask", 2048, nullptr, 2, &dmxTaskHandle, 0);

    Serial.printf("[DMX] init=OK TX=%d DE=%d addr=%d UART=%d\n",
        DMX_TX_PIN, DMX_DE_RE_PIN, DMX_START_ADDRESS, DMX_UART_NUM);
}

void dmxSetRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t master) {
    _current = {r, g, b, master};
    _dmxBuffer[1] = r;       // CH1=червоний
    _dmxBuffer[2] = g;       // CH2=зелений
    _dmxBuffer[3] = b;       // CH3=синій
    _dmxBuffer[4] = master;  // CH4=dimmer (загальна яскравість)
    Serial.printf("[DMX] R=%d G=%d B=%d M=%d\n", r, g, b, master);
}

void dmxSetChannel(uint16_t channel, uint8_t value) {
    if (channel >= 1 && channel < 513)
        _dmxBuffer[channel] = value;
}

RGBWColor dmxGetCurrent() { return _current; }

void dmxSendFrame() {
    DmxSerial.end();
    pinMode(DMX_TX_PIN, OUTPUT);
    digitalWrite(DMX_TX_PIN, LOW);
    delayMicroseconds(300);  // BREAK >250мкс
    digitalWrite(DMX_TX_PIN, HIGH);
    delayMicroseconds(16);   // MAB
    DmxSerial.begin(250000, SERIAL_8N2, DMX_RX_PIN, DMX_TX_PIN);
    DmxSerial.write(_dmxBuffer, 513);
    DmxSerial.flush();
}

void dmxTask(void* parameter) {
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t count = 0;
    while (true) {
        dmxSendFrame();
        count++;
        if (count % 100 == 0)
            Serial.printf("[DMX] %u M=%d R=%d\n", count,
                _dmxBuffer[DMX_START_ADDRESS], _dmxBuffer[DMX_START_ADDRESS + 1]);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DMX_UPDATE_INTERVAL));
    }
}
