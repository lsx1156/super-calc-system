#ifndef _DIGITAL_CAPTURE_H
#define _DIGITAL_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    uint8_t buffer[DIGITAL_BUFFER_SIZE];
    uint32_t write_idx;
    uint32_t read_idx;
    bool     running;
    uint32_t capture_rate;
} digital_capture_t;

void digital_capture_init(void);
void digital_capture_set_mode(bool crack_mode);
void digital_capture_start(uint32_t rate);
void digital_capture_stop(void);
uint32_t digital_capture_available(void);
bool digital_capture_read(uint32_t* value);
uint32_t digital_read_all(void);
bool digital_read_pin(uint8_t pin);
void digital_capture_sync_trigger(void);

#endif
