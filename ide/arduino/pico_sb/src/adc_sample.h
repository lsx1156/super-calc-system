#ifndef ADC_SAMPLE_H
#define ADC_SAMPLE_H

#include <Arduino.h>

void adc_sample_init(void);
void adc_sample_start(uint32_t rate);
void adc_sample_stop(void);
bool adc_sample_read(uint16_t* values, int count);
float adc_get_temp(void);

#endif