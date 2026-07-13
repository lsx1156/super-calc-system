#ifndef _SPI_MASTER_H
#define _SPI_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

void spi_master_init(void);
bool spi_master_send_cmd(uint8_t node_id, uint8_t cmd, 
                         const uint8_t* params, uint8_t param_len,
                         uint8_t* resp, uint16_t* resp_len);
bool spi_master_broadcast(uint8_t cmd, const uint8_t* params, uint8_t param_len);
void spi_master_select(uint8_t node_id);
void spi_master_deselect(uint8_t node_id);
uint16_t crc16(const uint8_t* data, uint16_t len);

#endif
