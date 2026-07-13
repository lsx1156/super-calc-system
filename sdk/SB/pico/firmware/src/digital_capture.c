#include "digital_capture.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "digital_capture.pio.h"
#include "i2c_sched_slave.h"
#include "spi_comm.h"
#include <string.h>

static digital_capture_t g_digital;
static PIO g_pio;
static uint g_sm;
static uint g_offset;
static int g_dma_chan = -1;
static dma_channel_config g_dma_cfg;
static uint32_t g_dma_block_size = 512;
static bool g_crack_mode = false;

#define DIGITAL_PIN_START_SPI 4
#define DIGITAL_PIN_START_CRACK 0

static void digital_capture_dma_handler(void) {
    dma_hw->ints0 = 1u << g_dma_chan;
    
    g_digital.write_idx = (g_digital.write_idx + g_dma_block_size) % DIGITAL_BUFFER_SIZE;
    
    dma_channel_configure(
        g_dma_chan,
        &g_dma_cfg,
        &g_digital.buffer[g_digital.write_idx],
        &g_pio->rxf[g_sm],
        g_dma_block_size,
        true
    );
}

void digital_capture_init(void) {
    memset(&g_digital, 0, sizeof(g_digital));
    
    g_pio = pio0;
    g_offset = pio_add_program(g_pio, &digital_capture_program);
    g_sm = pio_claim_unused_sm(g_pio, true);
    g_crack_mode = false;
    
    int pin_start = DIGITAL_PIN_START_SPI;
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        pio_gpio_init(g_pio, pin_start + i);
        gpio_set_dir(pin_start + i, GPIO_IN);
        gpio_pull_down(pin_start + i);
    }
    
    pio_sm_config c = digital_capture_program_get_default_config(g_offset);
    
    sm_config_set_in_pins(&c, pin_start);
    sm_config_set_in_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    pio_sm_init(g_pio, g_sm, g_offset, &c);
    
    g_dma_chan = dma_claim_unused_channel(true);
    g_dma_cfg = dma_channel_get_default_config(g_dma_chan);
    channel_config_set_transfer_data_size(&g_dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&g_dma_cfg, false);
    channel_config_set_write_increment(&g_dma_cfg, true);
    channel_config_set_dreq(&g_dma_cfg, pio_get_dreq(g_pio, g_sm, false));
    channel_config_set_irq_quiet(&g_dma_cfg, false);
    dma_channel_set_irq0_enabled(g_dma_chan, true);
    
    irq_set_exclusive_handler(DMA_IRQ_0, digital_capture_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    pio_sm_set_enabled(g_pio, g_sm, false);
}

void digital_capture_set_mode(bool crack_mode) {
    if (g_crack_mode == crack_mode) return;
    
    pio_sm_set_enabled(g_pio, g_sm, false);
    
    int old_start = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    
    if (crack_mode && old_start != DIGITAL_PIN_START_CRACK) {
        i2c_sched_slave_deinit();
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
    
    pio_sm_config c = digital_capture_program_get_default_config(g_offset);
    
    sm_config_set_in_pins(&c, new_start);
    sm_config_set_in_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    pio_sm_init(g_pio, g_sm, g_offset, &c);
}

void digital_capture_start(uint32_t rate) {
    g_digital.capture_rate = rate;
    g_digital.running = true;
    g_digital.write_idx = 0;
    g_digital.read_idx = 0;
    
    float div = (float)clock_get_hz(clk_sys) / (float)rate;
    if (div < 1.0f) div = 1.0f;
    
    pio_sm_set_clkdiv(g_pio, g_sm, div);
    
    int pin_start = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    
    dma_channel_configure(
        g_dma_chan,
        &g_dma_cfg,
        g_digital.buffer,
        &g_pio->rxf[g_sm],
        g_dma_block_size,
        true
    );
    
    pio_sm_clear_fifos(g_pio, g_sm);
    
    pio_sm_put_blocking(g_pio, g_sm, 0);
    
    pio_sm_set_enabled(g_pio, g_sm, true);
}

void digital_capture_stop(void) {
    pio_sm_set_enabled(g_pio, g_sm, false);
    
    if (g_dma_chan >= 0) {
        dma_channel_abort(g_dma_chan);
    }
    
    g_digital.running = false;
}

uint32_t digital_capture_available(void) {
    if (!g_digital.running) return 0;
    
    uint32_t dma_written = sizeof(g_digital.buffer) - dma_channel_hw_addr(g_dma_chan)->transfer_count;
    uint32_t actual_write = g_digital.write_idx + dma_written;
    
    if (actual_write >= g_digital.read_idx) {
        return actual_write - g_digital.read_idx;
    }
    
    return actual_write + DIGITAL_BUFFER_SIZE - g_digital.read_idx;
}

bool digital_capture_read(uint32_t* value) {
    if (digital_capture_available() == 0) return false;
    
    *value = g_digital.buffer[g_digital.read_idx++];
    if (g_digital.read_idx >= DIGITAL_BUFFER_SIZE) {
        g_digital.read_idx = 0;
    }
    return true;
}

uint32_t digital_read_all(void) {
    uint32_t val = 0;
    int pin_start = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        if (gpio_get(pin_start + i)) {
            val |= (1 << i);
        }
    }
    return val;
}

bool digital_read_pin(uint8_t pin) {
    if (pin >= DIGITAL_CHANNELS) return false;
    int pin_start = g_crack_mode ? DIGITAL_PIN_START_CRACK : DIGITAL_PIN_START_SPI;
    return gpio_get(pin_start + pin);
}

void digital_capture_sync_trigger(void) {
    g_pio->irq_force = 1u << 0;
}