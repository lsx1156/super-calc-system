#ifndef _USB_CDC_COMM_H
#define _USB_CDC_COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    uint8_t  cmd;
    uint8_t  params[256];
    uint16_t param_len;
    bool     cmd_ready;
} usb_cmd_t;

void usb_cdc_init(void);
void usb_cdc_task(void* pvParameters);
bool usb_cdc_get_cmd(usb_cmd_t* cmd);
void usb_cdc_send_data(const uint8_t* data, uint16_t len);
void usb_cdc_send_status(void);
uint32_t crc32(const uint8_t* data, uint32_t len);

#endif
