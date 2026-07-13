#ifndef _EDGE_COMPUTE_H
#define _EDGE_COMPUTE_H

#include <stdint.h>
#include <stdbool.h>

// ==================== 配置参数 ====================
#define EDGE_SAMPLES_PER_FRAME  64
#define EDGE_ADC_CHANNELS       4
#define EDGE_DIGITAL_CHANNELS   8
#define EDGE_DELTA_BUF_SIZE     512
#define EDGE_FEATURE_BUF_SIZE   256

// ==================== 数据类型 ====================
typedef struct {
    uint16_t adc[EDGE_ADC_CHANNELS];
    uint8_t digital;
    uint32_t timestamp;
} edge_sample_t;

typedef struct {
    int16_t adc_delta[EDGE_ADC_CHANNELS];
    uint8_t digital_diff;
    uint8_t changed_mask;
} edge_delta_t;

typedef struct {
    edge_sample_t last_sample;
    bool has_last_sample;
} edge_delta_ctx_t;

typedef struct {
    uint16_t min[EDGE_ADC_CHANNELS];
    uint16_t max[EDGE_ADC_CHANNELS];
    uint32_t sum[EDGE_ADC_CHANNELS];
    uint16_t avg[EDGE_ADC_CHANNELS];
    uint16_t peak_to_peak[EDGE_ADC_CHANNELS];
    
    uint32_t edge_count[EDGE_DIGITAL_CHANNELS];
    uint8_t digital;
    uint8_t last_digital;
    uint8_t active_channels;
    uint8_t dominant_pattern;
    
    uint32_t sample_count;
} edge_features_t;

// ==================== 函数声明 ====================

// Delta压缩

void edge_delta_ctx_init(edge_delta_ctx_t* ctx);

uint16_t edge_delta_compress(edge_delta_ctx_t* ctx, const edge_sample_t* samples, uint16_t count, 
                             edge_delta_t* deltas, uint16_t max_deltas);

uint16_t edge_delta_decompress(const edge_delta_t* deltas, uint16_t count,
                               edge_sample_t* samples, uint16_t max_samples,
                               const edge_sample_t* base);

// 特征提取

void edge_extract_features(const edge_sample_t* samples, uint16_t count,
                           edge_features_t* features);

void edge_update_features(const edge_sample_t* new_sample, edge_features_t* features);

void edge_reset_features(edge_features_t* features);

// 数据转换

bool edge_parse_raw_data(const uint8_t* raw, uint16_t raw_len,
                         edge_sample_t* samples, uint16_t* sample_count);

uint16_t edge_pack_features(const edge_features_t* features, uint8_t* buf, uint16_t max_len);

uint16_t edge_pack_deltas(const edge_delta_t* deltas, uint16_t count,
                          uint8_t* buf, uint16_t max_len);

#endif
