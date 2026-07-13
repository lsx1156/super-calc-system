#ifndef HW_TEST_H
#define HW_TEST_H

#include <Arduino.h>

void hw_test_init(void);
void hw_test_run(uint8_t test_type, uint8_t* resp, uint16_t* resp_len);
void hw_test_glitch(uint16_t width, uint8_t count);

#endif