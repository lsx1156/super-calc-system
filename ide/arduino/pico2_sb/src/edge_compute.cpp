#include "edge_compute.h"
#include <string.h>

void edge_delta_ctx_init(edge_delta_ctx_t* ctx) {
    if (!ctx) return;
    memset(&ctx->last_sample, 0, sizeof(edge_sample_t));
    ctx->has_last_sample = false;
}

uint16_t edge_delta_compress(edge_delta_ctx_t* ctx, const edge_sample_t* samples, uint16_t count, 
                             edge_delta_t* deltas, uint16_t max_deltas) {
    if (!ctx || !samples || !deltas || count == 0 || max_deltas == 0) return 0;
    
    uint16_t delta_count = 0;
    edge_sample_t base = ctx->has_last_sample ? ctx->last_sample : samples[0];
    
    for (uint16_t i = 0; i < count && delta_count < max_deltas; i++) {
        edge_delta_t* d = &deltas[delta_count];
        uint8_t changed = 0;
        
        for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
            d->adc_delta[j] = (int16_t)samples[i].adc[j] - (int16_t)base.adc[j];
            if (d->adc_delta[j] != 0) changed |= (1 << j);
        }
        
        d->digital_diff = samples[i].digital ^ base.digital;
        if (d->digital_diff != 0) changed |= 0xF0;
        
        d->changed_mask = changed;
        
        if (changed != 0) {
            base = samples[i];
            delta_count++;
        }
    }
    
    if (count > 0) {
        ctx->last_sample = samples[count - 1];
        ctx->has_last_sample = true;
    }
    
    return delta_count;
}

uint16_t edge_delta_decompress(const edge_delta_t* deltas, uint16_t count,
                               edge_sample_t* samples, uint16_t max_samples,
                               const edge_sample_t* base) {
    if (!deltas || !samples || !base || count == 0 || max_samples == 0) return 0;
    
    uint16_t sample_count = 0;
    edge_sample_t current = *base;
    
    samples[0] = current;
    sample_count++;
    
    for (uint16_t i = 0; i < count && sample_count < max_samples; i++) {
        const edge_delta_t* d = &deltas[i];
        
        for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
            if (d->changed_mask & (1 << j)) {
                current.adc[j] = (uint16_t)((int16_t)current.adc[j] + d->adc_delta[j]);
            }
        }
        
        if (d->changed_mask & 0xF0) {
            current.digital ^= d->digital_diff;
        }
        
        current.timestamp++;
        samples[sample_count++] = current;
    }
    
    return sample_count;
}

void edge_extract_features(const edge_sample_t* samples, uint16_t count,
                           edge_features_t* features) {
    if (!samples || !features || count == 0) return;
    
    edge_reset_features(features);
    
    for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
        features->min[j] = samples[0].adc[j];
        features->max[j] = samples[0].adc[j];
    }
    
    for (uint16_t i = 0; i < count; i++) {
        for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
            if (samples[i].adc[j] < features->min[j]) {
                features->min[j] = samples[i].adc[j];
            }
            if (samples[i].adc[j] > features->max[j]) {
                features->max[j] = samples[i].adc[j];
            }
            features->sum[j] += samples[i].adc[j];
        }
        
        if (i > 0) {
            uint8_t diff = samples[i].digital ^ samples[i-1].digital;
            for (int j = 0; j < EDGE_DIGITAL_CHANNELS; j++) {
                if (diff & (1 << j)) {
                    features->edge_count[j]++;
                }
            }
        }
        
        features->digital = samples[i].digital;
        features->last_digital = samples[i].digital;
        features->sample_count++;
    }
    
    for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
        if (features->sample_count > 0) {
            features->avg[j] = (uint16_t)(features->sum[j] / features->sample_count);
        }
        features->peak_to_peak[j] = features->max[j] - features->min[j];
    }
}

void edge_update_features(const edge_sample_t* new_sample, edge_features_t* features) {
    if (!new_sample || !features) return;
    
    for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
        if (new_sample->adc[j] < features->min[j]) {
            features->min[j] = new_sample->adc[j];
        }
        if (new_sample->adc[j] > features->max[j]) {
            features->max[j] = new_sample->adc[j];
        }
        features->sum[j] += new_sample->adc[j];
    }
    
    for (int j = 0; j < EDGE_DIGITAL_CHANNELS; j++) {
        uint8_t current_bit = (new_sample->digital >> j) & 1;
        uint8_t last_bit = (features->last_digital >> j) & 1;
        
        if (current_bit != last_bit) {
            features->edge_count[j]++;
        }
    }
    
    features->last_digital = new_sample->digital;
    features->digital = new_sample->digital;
    features->sample_count++;
    
    for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
        features->avg[j] = (uint16_t)(features->sum[j] / features->sample_count);
        features->peak_to_peak[j] = features->max[j] - features->min[j];
    }
}

void edge_reset_features(edge_features_t* features) {
    if (!features) return;
    
    memset(features, 0, sizeof(edge_features_t));
    
    for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
        features->min[j] = 0xFFFF;
    }
}

bool edge_parse_raw_data(const uint8_t* raw, uint16_t raw_len,
                         edge_sample_t* samples, uint16_t* sample_count) {
    if (!raw || !samples || !sample_count) return false;
    
    *sample_count = 0;
    uint16_t offset = 0;
    uint16_t sample_size = EDGE_ADC_CHANNELS * 2 + 1;
    
    while (offset + sample_size <= raw_len && *sample_count < EDGE_SAMPLES_PER_FRAME) {
        edge_sample_t* s = &samples[*sample_count];
        
        for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
            s->adc[j] = raw[offset++] | (raw[offset++] << 8);
        }
        
        s->digital = raw[offset++];
        s->timestamp = 0;
        
        (*sample_count)++;
    }
    
    return *sample_count > 0;
}

uint16_t edge_pack_features(const edge_features_t* features, uint8_t* buf, uint16_t max_len) {
    if (!features || !buf || max_len < 32) return 0;
    
    uint16_t offset = 0;
    
    buf[offset++] = 'F';
    buf[offset++] = features->sample_count & 0xFF;
    buf[offset++] = (features->sample_count >> 8) & 0xFF;
    
    for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
        buf[offset++] = features->min[j] & 0xFF;
        buf[offset++] = (features->min[j] >> 8) & 0xFF;
        buf[offset++] = features->max[j] & 0xFF;
        buf[offset++] = (features->max[j] >> 8) & 0xFF;
        buf[offset++] = features->avg[j] & 0xFF;
        buf[offset++] = (features->avg[j] >> 8) & 0xFF;
    }
    
    for (int j = 0; j < EDGE_DIGITAL_CHANNELS; j++) {
        buf[offset++] = features->edge_count[j] & 0xFF;
        buf[offset++] = (features->edge_count[j] >> 8) & 0xFF;
    }
    
    buf[offset++] = features->digital;
    
    return offset;
}

uint16_t edge_pack_deltas(const edge_delta_t* deltas, uint16_t count,
                          uint8_t* buf, uint16_t max_len) {
    if (!deltas || !buf || max_len < 10 || count == 0) return 0;
    
    uint16_t offset = 0;
    
    buf[offset++] = 'D';
    buf[offset++] = count & 0xFF;
    buf[offset++] = (count >> 8) & 0xFF;
    
    for (uint16_t i = 0; i < count && offset + 10 <= max_len; i++) {
        buf[offset++] = deltas[i].changed_mask;
        
        for (int j = 0; j < EDGE_ADC_CHANNELS; j++) {
            buf[offset++] = deltas[i].adc_delta[j] & 0xFF;
            buf[offset++] = (deltas[i].adc_delta[j] >> 8) & 0xFF;
        }
        
        buf[offset++] = deltas[i].digital_diff;
    }
    
    return offset;
}