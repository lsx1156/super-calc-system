#ifndef USB_CDC_COMM_H
#define USB_CDC_COMM_H

#include <Arduino.h>
#include "config.h"

typedef struct {
    uint8_t cmd;
    uint8_t param_len;
    uint8_t params[32];
    bool cmd_ready;
} usb_cmd_t;

void usb_cdc_init(void);
void usb_cdc_task(void);
bool usb_cdc_get_cmd(usb_cmd_t* cmd);
void usb_cdc_send_data(const uint8_t* data, uint16_t len);

#endif