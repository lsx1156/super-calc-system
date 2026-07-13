#ifndef OC_CONTROL_H
#define OC_CONTROL_H

#include <Arduino.h>

void oc_control_init(void);
void oc_control_set(bool enable);
void oc_control_set_all_pico(bool enable);
void oc_control_dynamic_adjust(float temp);
uint32_t oc_control_get_freq(void);

#endif