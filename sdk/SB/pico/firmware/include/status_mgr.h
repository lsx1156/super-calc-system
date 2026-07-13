#ifndef _STATUS_MGR_H
#define _STATUS_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    uint8_t  work_mode;
    uint8_t  run_status;
    uint8_t  node_id;
    uint32_t sample_rate;
    uint32_t total_samples;
    uint32_t overclock_freq;
    float    core_temp;
    float    vcore;
    uint32_t error_count;
    uint8_t  fault_code;
    uint32_t uptime_ms;
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
} device_status_t;

void status_init(void);
void status_update_temp(float temp);
void status_set_mode(uint8_t mode);
void status_set_running(bool running);
void status_add_error(void);
void status_reset_counts(void);
void status_reset(void);
device_status_t* status_get(void);
bool status_acquire_mutex(void);
void status_release_mutex(void);

#endif
