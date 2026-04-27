// ============================================================================
// DMX_CONTROLLER.CPP - Відправка DMX512 через ESP-IDF UART driver
// ============================================================================

#include "dmx_controller.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static uint8_t _dmxBuffer[513];
static RGBWColor _current = {0, 0, 0, 0};
TaskHandle_t dmxTaskHandle = nullptr;

void dmxInit() {
    if (DMX_DE_RE_PIN >= 0) {
        gpio_set_direction((gpio_num_t)DMX_DE_RE_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)DMX_DE_RE_PIN, 1); // завжди TX
    }

    uart_config_t uart_config = {
        .baud_rate  = 250000,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_2,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_driver_install((uart_port_t)DMX_UART_NUM, 1024, 0, 0, nullptr, 0);
    uart_param_config((uart_port_t)DMX_UART_NUM, &uart_config);
    uart_set_pin((uart_port_t)DMX_UART_NUM, DMX_TX_PIN, DMX_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    memset(_dmxBuffer, 0, sizeof(_dmxBuffer));

    xTaskCreatePinnedToCore(dmxTask, "DMXTask", 2048, nullptr, 2, &dmxTaskHandle, 0);

    Serial.printf("[DMX] init=OK TX=%d DE=%d addr=%d UART=%d\n",
        DMX_TX_PIN, DMX_DE_RE_PIN, DMX_START_ADDRESS, DMX_UART_NUM);
}

void dmxSetRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t master) {
    _current = {r, g, b, master};
    uint16_t base = DMX_START_ADDRESS;
    _dmxBuffer[base + DMX_CH_MASTER] = master;
    _dmxBuffer[base + DMX_CH_RED]    = r;
    _dmxBuffer[base + DMX_CH_GREEN]  = g;
    _dmxBuffer[base + DMX_CH_BLUE]   = b;
    _dmxBuffer[base + DMX_CH_STROBE] = 0;
}

void dmxSetChannel(uint16_t channel, uint8_t value) {
    if (channel >= 1 && channel < 513)
        _dmxBuffer[channel] = value;
}

RGBWColor dmxGetCurrent() { return _current; }

void dmxSendFrame() {
    // BREAK через GPIO напряму
    gpio_set_direction((gpio_num_t)DMX_TX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)DMX_TX_PIN, 0);
    ets_delay_us(176);
    gpio_set_level((gpio_num_t)DMX_TX_PIN, 1);
    ets_delay_us(16); // MAB
    // Повертаємо пін UART
    uart_set_pin((uart_port_t)DMX_UART_NUM, DMX_TX_PIN, DMX_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Відправляємо START code (0x00) + 512 каналів
    uart_write_bytes((uart_port_t)DMX_UART_NUM, (const char*)_dmxBuffer, 513);
    uart_wait_tx_done((uart_port_t)DMX_UART_NUM, pdMS_TO_TICKS(50));
}

void dmxTask(void* parameter) {
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t count = 0;
    while (true) {
        dmxSendFrame();
        count++;
        if (count % 100 == 0)
            Serial.printf("[DMX] %u M=%d R=%d\n", count, _dmxBuffer[DMX_START_ADDRESS], _dmxBuffer[DMX_START_ADDRESS+1]);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DMX_UPDATE_INTERVAL));
    }
}
