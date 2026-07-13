#ifndef DATA_AGGREGATOR_H
#define DATA_AGGREGATOR_H

#include <Arduino.h>

#define AGGREGATE_BUF_SIZE  8192

typedef struct {
    uint8_t  buffer[AGGREGATE_BUF_SIZE];
    uint32_t write_idx;
    uint32_t read_idx;
    uint32_t total_packets;
    uint32_t dropped_packets;
} aggregate_buf_t;

void data_aggregator_init(void);
bool data_aggregator_poll_all(void);
bool data_aggregator_process_dma_data(void);
bool data_aggregator_get_packet(uint8_t* data, uint16_t* len);
bool data_aggregator_get_features_packet(uint8_t* data, uint16_t* len);
bool data_aggregator_get_deltas_packet(uint8_t* data, uint16_t* len);
uint32_t data_aggregator_available(void);

#endif