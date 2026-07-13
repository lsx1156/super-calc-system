#ifndef DIGITAL_CAPTURE_H
#define DIGITAL_CAPTURE_H

#include <Arduino.h>

#define DIGITAL_CHANNELS    8
#define DIGITAL_PIN_START_SPI  4
#define DIGITAL_PIN_START_CRACK 0

void digital_capture_init(void);
void digital_capture_start(uint32_t rate);
void digital_capture_stop(void);
void digital_capture_set_mode(bool crack_mode);
uint32_t digital_capture_available(void);
bool digital_capture_read(uint8_t* value);
uint8_t digital_read_all(void);
bool digital_read_pin(uint8_t pin);

#endif