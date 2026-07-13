#ifndef _CRACK_ENGINE_H
#define _CRACK_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    bool     running;
    uint32_t attempts;
    uint8_t  key_length;
    char     result[32];
    bool     found;
    uint32_t progress;
} crack_state_t;

void crack_init(void);
void crack_start(const char* target_hash, uint8_t key_len, const char* charset);
void crack_stop(void);
crack_state_t* crack_get_state(void);
void crack_task(void* pvParameters);

#endif
