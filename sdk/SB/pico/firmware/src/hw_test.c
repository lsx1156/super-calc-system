#include "hw_test.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <stdlib.h>
#include <string.h>
#include "adc_sample.h"

#define GLITCH_OUT_PIN 15

void hw_test_init(void) {
    gpio_init(GLITCH_OUT_PIN);
    gpio_set_dir(GLITCH_OUT_PIN, GPIO_OUT);
    gpio_put(GLITCH_OUT_PIN, 0);
}

bool hw_test_run(uint8_t test_type, uint8_t* result, uint16_t* result_len) {
    uint16_t idx = 0;
    
    switch (test_type) {
        case HW_TEST_VOLTAGE: {
            float v = vreg_get_voltage();
            result[idx++] = 0x01;
            memcpy(&result[idx], &v, sizeof(float));
            idx += sizeof(float);
            *result_len = idx;
            return true;
        }
            
        case HW_TEST_CLOCK: {
            uint32_t freq = clock_get_hz(clk_sys);
            result[idx++] = 0x01;
            memcpy(&result[idx], &freq, sizeof(uint32_t));
            idx += sizeof(uint32_t);
            *result_len = idx;
            return true;
        }
            
        case HW_TEST_TEMPERATURE: {
            float temp = adc_get_temp();
            result[idx++] = 0x01;
            memcpy(&result[idx], &temp, sizeof(float));
            idx += sizeof(float);
            *result_len = idx;
            return true;
        }
            
        case HW_TEST_MEMORY: {
            uint32_t test_pattern = 0xAA55AA55;
            uint32_t* test_buf = (uint32_t*)malloc(1024 * sizeof(uint32_t));
            if (!test_buf) {
                result[idx++] = 0x00;
                *result_len = idx;
                return false;
            }
            
            bool pass = true;
            for (int i = 0; i < 1024; i++) {
                test_buf[i] = test_pattern ^ i;
            }
            for (int i = 0; i < 1024; i++) {
                if (test_buf[i] != (test_pattern ^ i)) {
                    pass = false;
                    break;
                }
            }
            
            free(test_buf);
            result[idx++] = pass ? 0x01 : 0x00;
            *result_len = idx;
            return true;
        }
            
        default:
            result[idx++] = 0xFF;
            *result_len = idx;
            return false;
    }
}

void hw_test_glitch(uint16_t width_us, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        gpio_put(GLITCH_OUT_PIN, 1);
        busy_wait_us(width_us);
        gpio_put(GLITCH_OUT_PIN, 0);
        sleep_ms(10);
    }
}
