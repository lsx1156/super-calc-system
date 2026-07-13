#ifndef SPI_COMM_H
#define SPI_COMM_H

#include <Arduino.h>
#include "config.h"

typedef struct {
    uint8_t cmd;
    uint8_t param_len;
    uint8_t params[32];
    bool cmd_ready;
    uint8_t resp[128];
    uint16_t resp_len;
} spi_cmd_t;

void spi_comm_init(void);
void spi_comm_task(void);
bool spi_comm_get_cmd(spi_cmd_t* cmd);
void spi_comm_send_resp(const uint8_t* data, uint16_t len);

#endif