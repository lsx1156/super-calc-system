#ifndef OVERCLOCK_H
#define OVERCLOCK_H

#include <Arduino.h>

#define OC_SOURCE_NONE       0
#define OC_SOURCE_SPI_CMD    1
#define OC_SOURCE_SPI_MODE   2
#define OC_SOURCE_I2C_REG    3

void overclock_init(void);
void overclock_set(bool enable);
void overclock_set_ex(bool enable, int source);
uint32_t overclock_get_freq(void);
void overclock_check_temp(float temp);

#endif