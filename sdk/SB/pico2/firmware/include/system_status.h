#ifndef _SYSTEM_STATUS_H
#define _SYSTEM_STATUS_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    uint8_t  work_mode;
    uint8_t  run_status;
    uint32_t sample_rate;
    uint32_t total_samples;
    uint32_t overclock_freq;
    float    core_temp;
    float    vcore;
    uint8_t  fault_code;
    uint32_t uptime_ms;
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint8_t  pico_count;
    uint8_t  online_count;
} system_status_t;

void system_status_init(void);
void system_status_set_mode(uint8_t mode);
void system_status_set_running(bool running);
void system_status_update_temp(float temp);
system_status_t* system_status_get(void);

#endif
