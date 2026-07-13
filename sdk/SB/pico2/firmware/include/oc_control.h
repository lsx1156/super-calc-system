#ifndef _OC_CONTROL_H
#define _OC_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

void oc_control_init(void);
bool oc_control_set(bool enable);
bool oc_control_is_active(void);
uint32_t oc_control_get_freq(void);
float oc_control_get_vcore(void);
bool oc_control_check_temp(float temp);
void oc_control_dynamic_adjust(float temp);
bool oc_control_set_all_pico(bool enable);

#endif
