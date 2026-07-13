#include "adc_sample.h"
#include "config.h"

static uint16_t adc_buffer[ADC_BUFFER_SIZE];
static volatile uint32_t adc_write_idx = 0;
static volatile uint32_t adc_read_idx = 0;
static volatile bool adc_running = false;

void adc_sample_init(void) {
    adc_init();
    for (int i = 0; i < ADC_CHANNELS; i++) {
        adc_gpio_init(26 + i);
    }
    adc_set_temp_sensor_enabled(true);
    
    adc_write_idx = 0;
    adc_read_idx = 0;
    adc_running = false;
}

void adc_sample_start(uint32_t rate) {
    if (rate > MAX_SAMPLE_RATE) rate = MAX_SAMPLE_RATE;
    
    adc_write_idx = 0;
    adc_read_idx = 0;
    
    float div = (float)48000000.0 / (float)(rate * 96.0);
    adc_set_clkdiv(div);
    
    adc_run(false);
    adc_fifo_setup(true, false, 1, false, false);
    adc_set_round_robin(0x0F);
    adc_run(true);
    
    adc_running = true;
}

void adc_sample_stop(void) {
    adc_run(false);
    adc_fifo_drain();
    adc_running = false;
}

bool adc_sample_read(uint16_t* values, int count) {
    if (!adc_running || count > ADC_CHANNELS) return false;
    
    for (int i = 0; i < count; i++) {
        while (!adc_fifo_is_empty()) {
            values[i] = adc_fifo_get();
            break;
        }
    }
    
    return true;
}

float adc_get_temp(void) {
    bool was_running = adc_running;
    if (was_running) {
        adc_run(false);
        adc_fifo_drain();
    }
    
    adc_select_input(4);
    delayMicroseconds(10);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / 4095.0f;
    float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
    
    if (was_running) {
        adc_set_round_robin(0x0F);
        adc_run(true);
    }
    
    return temp;
}