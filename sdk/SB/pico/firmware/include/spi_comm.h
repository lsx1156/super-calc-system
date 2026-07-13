#ifndef _SPI_COMM_H
#define _SPI_COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "addr_assign_protocol.h"

typedef struct {
    uint8_t  cmd;
    uint8_t  params[64];
    uint8_t  param_len;
    uint8_t  resp[128];
    uint16_t resp_len;
    bool     cmd_ready;
} spi_cmd_t;

void spi_comm_init(void);
void spi_comm_task(void* pvParameters);
bool spi_comm_get_cmd(spi_cmd_t* cmd);
void spi_comm_send_resp(const uint8_t* data, uint16_t len);
uint16_t crc16(const uint8_t* data, uint16_t len);

#endif
