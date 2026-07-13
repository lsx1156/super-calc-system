#include "adc_sample.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include <string.h>

static adc_sample_t g_adc;
static int g_dma_chan = -1;
static dma_channel_config g_dma_cfg;
static volatile bool g_dma_done = false;

static void adc_dma_handler(void) {
    dma_hw->ints0 = 1u << g_dma_chan;
    g_dma_done = true;
}

void adc_sample_init(void) {
    memset(&g_adc, 0, sizeof(g_adc));
    
    adc_init();
    for (int i = 0; i < ADC_CHANNELS; i++) {
        adc_gpio_init(26 + i);
    }
    adc_set_temp_sensor_enabled(true);
    
    g_dma_chan = dma_claim_unused_channel(true);
    g_dma_cfg = dma_channel_get_default_config(g_dma_chan);
    channel_config_set_transfer_data_size(&g_dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&g_dma_cfg, false);
    channel_config_set_write_increment(&g_dma_cfg, true);
    channel_config_set_dreq(&g_dma_cfg, DREQ_ADC);
    
    dma_channel_set_irq0_enabled(g_dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, adc_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

static void adc_start_dma_block(void) {
    uint32_t remaining = ADC_BUFFER_SIZE - (g_adc.write_idx % ADC_BUFFER_SIZE);
    uint32_t transfer_size = (remaining > ADC_DMA_BLOCK_SIZE) ? ADC_DMA_BLOCK_SIZE : remaining;
    
    if (transfer_size == 0) {
        transfer_size = ADC_DMA_BLOCK_SIZE;
        g_adc.write_idx = (g_adc.write_idx / ADC_BUFFER_SIZE) * ADC_BUFFER_SIZE;
    }
    
    dma_channel_configure(
        g_dma_chan,
        &g_dma_cfg,
        &g_adc.buffer[g_adc.write_idx % ADC_BUFFER_SIZE],
        &adc_hw->fifo,
        transfer_size,
        true
    );
}

void adc_sample_start(uint32_t rate) {
    if (rate > MAX_SAMPLE_RATE) rate = MAX_SAMPLE_RATE;
    
    g_adc.sample_rate = rate;
    g_adc.running = true;
    g_adc.write_idx = 0;
    g_adc.read_idx = 0;
    g_dma_done = false;
    
    adc_set_round_robin(0x0F);
    adc_fifo_setup(true, true, 4, false, false);
    
    float div = 48000000.0f / rate - 1.0f;
    if (div < 0) div = 0;
    adc_set_clkdiv(div);
    
    adc_start_dma_block();
    
    adc_run(true);
}

void adc_sample_stop(void) {
    adc_run(false);
    dma_channel_abort(g_dma_chan);
    g_adc.running = false;
}

static void adc_check_dma_done(void) {
    if (!g_adc.running) return;
    
    if (g_dma_done) {
        g_dma_done = false;
        
        uint32_t remaining = ADC_BUFFER_SIZE - (g_adc.write_idx % ADC_BUFFER_SIZE);
        uint32_t transfer_size = (remaining > ADC_DMA_BLOCK_SIZE) ? ADC_DMA_BLOCK_SIZE : remaining;
        
        g_adc.write_idx += transfer_size;
        
        if (g_adc.write_idx - g_adc.read_idx > ADC_BUFFER_SIZE) {
            g_adc.read_idx = g_adc.write_idx - ADC_BUFFER_SIZE + 1;
        }
        
        if (g_adc.write_idx % ADC_BUFFER_SIZE == 0) {
            g_adc.write_idx = (g_adc.write_idx / ADC_BUFFER_SIZE) * ADC_BUFFER_SIZE;
        }
        
        adc_start_dma_block();
    }
}

uint32_t adc_sample_available(void) {
    adc_check_dma_done();
    
    if (!g_adc.running) return 0;
    
    uint32_t write_pos = g_adc.write_idx;
    uint32_t read_pos = g_adc.read_idx;
    
    if (write_pos >= read_pos) {
        return write_pos - read_pos;
    }
    return (ADC_BUFFER_SIZE - read_pos) + write_pos;
}

bool adc_sample_read(uint16_t* values, uint8_t channels) {
    adc_check_dma_done();
    
    if (adc_sample_available() < channels) return false;
    
    for (int i = 0; i < channels; i++) {
        values[i] = g_adc.buffer[g_adc.read_idx % ADC_BUFFER_SIZE];
        g_adc.read_idx++;
    }
    return true;
}

float adc_to_voltage(uint16_t val) {
    return (val * 3.3f) / 4095.0f;
}

float adc_get_temp(void) {
    bool was_running = g_adc.running;
    if (was_running) {
        adc_run(false);
    }
    
    adc_select_input(4);
    uint16_t raw = adc_read();
    
    const float conversion_factor = 3.3f / (1 << 12);
    float voltage = raw * conversion_factor;
    float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
    
    if (was_running) {
        adc_set_round_robin(0x0F);
        adc_start_dma_block();
        adc_run(true);
    }
    
    return temp;
}
