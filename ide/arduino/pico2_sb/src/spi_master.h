#ifndef SPI_MASTER_H
#define SPI_MASTER_H

#include <Arduino.h>
#include "config.h"

bool spi_master_init(void);
bool spi_master_send_cmd(uint8_t slave_id, uint8_t cmd, const uint8_t* params, uint8_t param_len,
                         uint8_t* resp_buf, uint16_t* resp_len);
void spi_master_broadcast(uint8_t cmd, const uint8_t* params, uint8_t param_len);

#endif