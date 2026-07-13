#ifndef FOOLPROOF_H
#define FOOLPROOF_H

#include <Arduino.h>

#define PRSNT_PIN            18
#define FAULT_LED_PIN        13

typedef enum {
    FAULT_NONE = 0,
    FAULT_NODE_OFFLINE = 1,
    FAULT_OVER_TEMP = 2,
    FAULT_OVERCURRENT = 3,
    FAULT_SPI_ERROR = 4,
    FAULT_USB_ERROR = 5,
    FAULT_CLOCK_ERROR = 6,
    FAULT_WATCHDOG = 7,
    FAULT_INTERFACE = 8
} fault_code_t;

void foolproof_init(void);
void foolproof_task(void);
bool foolproof_check_all(void);
bool foolproof_check_temperature(void);
bool foolproof_check_nodes(void);
bool foolproof_check_spi(void);
bool foolproof_check_interface(void);
void foolproof_trigger_emergency(void);
void foolproof_clear_fault(uint8_t fault_code);
uint8_t foolproof_get_fault_count(void);
bool foolproof_has_fault(void);

#endif