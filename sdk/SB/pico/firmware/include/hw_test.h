#ifndef _HW_TEST_H
#define _HW_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum {
    HW_TEST_VOLTAGE = 0,
    HW_TEST_CLOCK = 1,
    HW_TEST_TEMPERATURE = 2,
    HW_TEST_MEMORY = 3,
    HW_TEST_GLITCH = 4
} hw_test_type_t;

void hw_test_init(void);
bool hw_test_run(uint8_t test_type, uint8_t* result, uint16_t* result_len);
void hw_test_glitch(uint16_t width_us, uint8_t count);

#endif
