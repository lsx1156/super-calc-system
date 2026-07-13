#include "hw_test.h"
#include "config.h"

void hw_test_init(void) {
}

void hw_test_run(uint8_t test_type, uint8_t* resp, uint16_t* resp_len) {
    *resp_len = 0;
    resp[(*resp_len)++] = 0x01;
    
    switch (test_type) {
        case 0: {
            float vcore = 3.3f;
            memcpy(&resp[*resp_len], &vcore, sizeof(float));
            *resp_len += sizeof(float);
            break;
        }
        case 1: {
            adc_select_input(4);
            delayMicroseconds(10);
            uint16_t raw = adc_read();
            float voltage = raw * 3.3f / 4095.0f;
            float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
            memcpy(&resp[*resp_len], &temp, sizeof(float));
            *resp_len += sizeof(float);
            break;
        }
        case 2: {
            uint32_t freq = SystemCoreClock;
            memcpy(&resp[*resp_len], &freq, sizeof(uint32_t));
            *resp_len += sizeof(uint32_t);
            break;
        }
        default:
            break;
    }
}

void hw_test_glitch(uint16_t width, uint8_t count) {
    (void)width;
    (void)count;
}