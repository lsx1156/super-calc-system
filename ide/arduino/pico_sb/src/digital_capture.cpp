#include "digital_capture.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "i2c_sched_slave.h"

static uint8_t digital_buffer[4096];
static volatile uint32_t write_idx = 0;
static volatile uint32_t read_idx = 0;
static volatile bool running = false;
static bool g_crack_mode = false;
static PIO g_pio = pio0;
static int g_sm = 0;
static uint offset;

static const uint16_t digital_capture_program_instructions[] = {
    0xe020,
    0xa022,
    0x40e0,
    0x4040,
    0x6040,
    0xe001,
    0x1003,
};

static const struct pio_program digital_capture_program = {
    .instructions = digital_capture_program_instructions,
    .length = 7,
    .origin = -1,
};

void digital_capture_init(void) {
    offset = pio_add_program(g_pio, &digital_capture_program);
    
    digital_capture_set_mode(false);
    
    write_idx = 0;
    read_idx = 0;
    running = false;
}

void digital_capture_set_mode(bool crack_mode) {
    if (g_crack_mode == crack_mode) return;
    
    if (running) {
        pio_sm_set_enabled(g_pio, g_sm, false);
    }
    
    int old_start = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    
    if (crack_mode && old_start != DIGITAL_PIN_START_CRACK) {
        i2c_sched_slave_clear_address();
    }
    
    g_crack_mode = crack_mode;
    int new_start = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        pio_gpio_init(g_pio, new_start + i);
        gpio_set_dir(new_start + i, GPIO_IN);
        gpio_pull_down(new_start + i);
    }
    
    if (old_start != new_start) {
        if (!g_crack_mode) {
            i2c_sched_slave_init();
            for (int i = 0; i < 4; i++) {
                gpio_set_function(old_start + i, GPIO_FUNC_SPI);
            }
        }
    }
    
    if (running) {
        pio_sm_config c = pio_get_default_sm_config();
        sm_config_set_in_pins(&c, new_start);
        sm_config_set_clkdiv(&c, 1.f);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
        pio_sm_init(g_pio, g_sm, offset, &c);
        pio_sm_set_enabled(g_pio, g_sm, true);
    }
}

void digital_capture_start(uint32_t rate) {
    write_idx = 0;
    read_idx = 0;
    
    pio_sm_config c = pio_get_default_sm_config();
    int start_pin = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    sm_config_set_in_pins(&c, start_pin);
    sm_config_set_clkdiv(&c, 1.f);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(g_pio, g_sm, offset, &c);
    pio_sm_set_enabled(g_pio, g_sm, true);
    
    running = true;
}

void digital_capture_stop(void) {
    pio_sm_set_enabled(g_pio, g_sm, false);
    running = false;
}

uint32_t digital_capture_available(void) {
    if (!running) return 0;
    
    while (pio_sm_is_rx_fifo_not_empty(g_pio, g_sm) && write_idx - read_idx < 4096) {
        uint32_t raw = pio_sm_get(g_pio, g_sm);
        digital_buffer[write_idx++] = (uint8_t)(raw & 0xFF);
        if (write_idx >= 4096) write_idx = 0;
    }
    
    uint32_t available = write_idx - read_idx;
    if (available > 4096) available = 4096;
    return available;
}

bool digital_capture_read(uint8_t* value) {
    if (!running || write_idx == read_idx) return false;
    
    *value = digital_buffer[read_idx++];
    if (read_idx >= 4096) read_idx = 0;
    return true;
}

uint8_t digital_read_all(void) {
    uint8_t val = 0;
    int start_pin = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        if (digitalRead(start_pin + i)) {
            val |= (1 << i);
        }
    }
    return val;
}

bool digital_read_pin(uint8_t pin) {
    if (pin >= DIGITAL_CHANNELS) return false;
    int start_pin = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    return digitalRead(start_pin + pin);
}