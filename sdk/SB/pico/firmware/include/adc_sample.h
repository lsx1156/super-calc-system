#ifndef _ADC_SAMPLE_H
#define _ADC_SAMPLE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    uint16_t buffer[ADC_BUFFER_SIZE];
    uint32_t write_idx;
    uint32_t read_idx;
    bool     running;
    uint32_t sample_rate;
} adc_sample_t;

void adc_sample_init(void);
void adc_sample_start(uint32_t rate);
void adc_sample_stop(void);
uint32_t adc_sample_available(void);
bool adc_sample_read(uint16_t* values, uint8_t channels);
float adc_to_voltage(uint16_t val);
float adc_get_temp(void);

#endif
