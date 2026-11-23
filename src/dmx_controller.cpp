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

    xTaskCreatePinnedToCore(dmxTask, "DMXTask", 4096, nullptr, 2, &dmxTaskHandle, 0);
}

void dmxSetRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t master) {
    _current = {r, g, b, master};
    _dmxBuffer[1] = r;       // CH1=червоний
    _dmxBuffer[2] = g;       // CH2=зелений
    _dmxBuffer[3] = b;       // CH3=синій
    _dmxBuffer[4] = master;  // CH4=dimmer (загальна яскравість)
}

void dmxSetChannel(uint16_t channel, uint8_t value) {
    if (channel >= 1 && channel < 513)
        _dmxBuffer[channel] = value;
}

RGBWColor dmxGetCurrent() { return _current; }

void dmxSendFrame() {
    // Апаратна генерація BREAK (~225 мкс LOW + MAB) без перестворення UART-драйвера
    DmxSerial.flush();
    DmxSerial.updateBaudRate(40000);
    DmxSerial.write((uint8_t)0x00);
    DmxSerial.flush();
    DmxSerial.updateBaudRate(250000);
    DmxSerial.write(_dmxBuffer, 513);
    DmxSerial.flush();
}

void dmxTask(void* parameter) {
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
        dmxSendFrame();
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DMX_UPDATE_INTERVAL));
    }
}
