#ifndef _OVERCLOCK_H
#define _OVERCLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define OC_SOURCE_NONE        0x00
#define OC_SOURCE_SPI_CMD     0x01
#define OC_SOURCE_SPI_MODE    0x02
#define OC_SOURCE_I2C_REG     0x03
#define OC_SOURCE_TEMP_PROTECT 0x04
#define OC_SOURCE_UNKNOWN     0xFF

void overclock_init(void);
bool overclock_set(bool enable);
bool overclock_set_ex(bool enable, uint8_t source);
bool overclock_is_active(void);
uint32_t overclock_get_freq(void);
float overclock_get_vcore(void);
uint8_t overclock_get_source(void);
bool overclock_acquire(void);
void overclock_release(void);
bool overclock_check_temp(float temp);

#endif
