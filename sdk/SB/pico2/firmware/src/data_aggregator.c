#include "data_aggregator.h"
#include "dma_spi_harvester.h"
#include "edge_compute.h"
#include "cluster_mgr.h"
#include "system_status.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

static aggregate_buf_t g_agg;
static edge_features_t g_features[DMA_SPI_MAX_PICO];
static edge_delta_t g_deltas[DMA_SPI_MAX_PICO][EDGE_DELTA_BUF_SIZE];
static uint16_t g_delta_counts[DMA_SPI_MAX_PICO];
static edge_delta_ctx_t g_delta_ctx[DMA_SPI_MAX_PICO];
static SemaphoreHandle_t g_agg_mutex = NULL;

void data_aggregator_init(void) {
    memset(&g_agg, 0, sizeof(g_agg));
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        edge_reset_features(&g_features[i]);
        edge_delta_ctx_init(&g_delta_ctx[i]);
        g_delta_counts[i] = 0;
        memset(g_deltas[i], 0, sizeof(g_deltas[i]));
    }
    
    g_agg_mutex = xSemaphoreCreateMutex();
    
    dma_spi_harvester_init();
}

bool data_aggregator_process_dma_data(void) {
    cluster_state_t* cluster = cluster_get_state();
    uint8_t raw_buf[DMA_SPI_FRAME_SIZE];
    edge_sample_t samples[EDGE_SAMPLES_PER_FRAME];
    uint16_t sample_count;
    bool got_data = false;
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        if (!dma_spi_harvester_get_node_online(i)) continue;
        
        dma_spi_event_t event = dma_spi_harvester_check_event(i);
        if (event != DMA_EVENT_FRAME_READY) continue;
        
        uint16_t raw_len = dma_spi_harvester_get_data(i, raw_buf, DMA_SPI_FRAME_SIZE);
        if (raw_len == 0) continue;
        
        uint32_t frame_ts = dma_spi_harvester_get_frame_timestamp(i);

        if (edge_parse_raw_data(raw_buf, raw_len, samples, &sample_count)) {
            for (uint16_t j = 0; j < sample_count; j++) {
                samples[j].timestamp = frame_ts;
                edge_update_features(&samples[j], &g_features[i]);
            }
            
            g_delta_counts[i] = edge_delta_compress(&g_delta_ctx[i], samples, sample_count, 
                                                    g_deltas[i], EDGE_DELTA_BUF_SIZE);
            
            got_data = true;
        }
        
        dma_spi_harvester_clear_event(i);
    }
    
    return got_data;
}

bool data_aggregator_poll_all(void) {
    return data_aggregator_process_dma_data();
}

bool data_aggregator_get_packet(uint8_t* data, uint16_t* len) {
    if (g_agg_mutex == NULL) return false;
    if (!xSemaphoreTake(g_agg_mutex, pdMS_TO_TICKS(10))) return false;
    
    bool result = false;
    *len = 0;
    
    if (g_agg.read_idx < g_agg.write_idx) {
        uint16_t pkt_len = 0;
        uint8_t* start = &g_agg.buffer[g_agg.read_idx];
        
        if (start[0] == DATA_AGGREGATED) {
            uint8_t node_count = start[1];
            pkt_len = 3;
            for (int i = 0; i < node_count; i++) {
                if (pkt_len + 3 > AGGREGATE_BUF_SIZE) break;
                uint16_t data_len = start[pkt_len + 1] | (start[pkt_len + 2] << 8);
                pkt_len += 3 + data_len;
            }
        }
        
        if (pkt_len > 0) {
            memcpy(data, start, pkt_len);
            *len = pkt_len;
            g_agg.read_idx += pkt_len;
            result = true;
        }
    }
    
    xSemaphoreGive(g_agg_mutex);
    return result;
}

bool data_aggregator_get_features_packet(uint8_t* data, uint16_t* len) {
    cluster_state_t* cluster = cluster_get_state();
    uint16_t offset = 0;
    
    if (!data || !len) return false;
    
    data[offset++] = DATA_AGGREGATED;
    data[offset++] = cluster->online_count;
    data[offset++] = cluster->total_nodes;
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        if (!dma_spi_harvester_get_node_online(i)) continue;
        
        if (offset + 40 >= AGGREGATE_BUF_SIZE) break;
        
        data[offset++] = i;
        
        uint16_t feat_len = edge_pack_features(&g_features[i], &data[offset], 
                                               AGGREGATE_BUF_SIZE - offset);
        data[offset++] = feat_len & 0xFF;
        data[offset++] = (feat_len >> 8) & 0xFF;
        offset += feat_len;
    }
    
    *len = offset;
    g_agg.total_packets++;
    
    return offset > 3;
}

bool data_aggregator_get_deltas_packet(uint8_t* data, uint16_t* len) {
    cluster_state_t* cluster = cluster_get_state();
    uint16_t offset = 0;
    
    if (!data || !len) return false;
    
    data[offset++] = DATA_AGGREGATED;
    data[offset++] = cluster->online_count;
    data[offset++] = cluster->total_nodes;
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        if (!dma_spi_harvester_get_node_online(i)) continue;
        if (g_delta_counts[i] == 0) continue;
        
        if (offset + 10 >= AGGREGATE_BUF_SIZE) break;
        
        data[offset++] = i;
        
        uint16_t delta_len = edge_pack_deltas(g_deltas[i], g_delta_counts[i], 
                                               &data[offset], AGGREGATE_BUF_SIZE - offset);
        data[offset++] = delta_len & 0xFF;
        data[offset++] = (delta_len >> 8) & 0xFF;
        offset += delta_len;
        
        g_delta_counts[i] = 0;
    }
    
    *len = offset;
    g_agg.total_packets++;
    
    return offset > 3;
}

uint32_t data_aggregator_available(void) {
    if (g_agg.write_idx >= g_agg.read_idx) {
        return g_agg.write_idx - g_agg.read_idx;
    }
    return 0;
}

void data_aggregator_task(void* pvParameters) {
    uint8_t pkt_buf[AGGREGATE_BUF_SIZE];
    uint16_t pkt_len = 0;
    uint32_t tick_count = 0;
    
    while (1) {
        system_status_t* st = system_status_get();
        
        if (st->run_status) {
            data_aggregator_process_dma_data();
            st->total_samples++;
            
            tick_count++;
            
            if (tick_count % 10 == 0) {
                if (data_aggregator_get_features_packet(pkt_buf, &pkt_len)) {
                    if (g_agg_mutex != NULL && xSemaphoreTake(g_agg_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        uint32_t end = g_agg.write_idx + pkt_len;
                        if (end < AGGREGATE_BUF_SIZE) {
                            memcpy(&g_agg.buffer[g_agg.write_idx], pkt_buf, pkt_len);
                            g_agg.write_idx = end;
                        } else {
                            g_agg.dropped_packets++;
                        }
                        xSemaphoreGive(g_agg_mutex);
                    }
                }
            }
            
            if (tick_count % 100 == 0) {
                for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
                    edge_reset_features(&g_features[i]);
                }
                tick_count = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(AGGREGATE_PERIOD_MS));
    }
}
