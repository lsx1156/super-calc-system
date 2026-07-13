#include "i2c_sched_regs.h"
#include <string.h>

uint8_t i2c_regs_calculate_checksum(const i2c_regs_t* regs) {
    uint8_t checksum = 0;
    const uint8_t* ptr = (const uint8_t*)regs;
    
    for (int i = 0; i < REG_CHECKSUM; i++) {
        checksum ^= ptr[i];
    }
    for (int i = REG_CHECKSUM + 1; i < sizeof(i2c_regs_t); i++) {
        checksum ^= ptr[i];
    }
    
    return checksum;
}

bool i2c_regs_validate_checksum(const i2c_regs_t* regs) {
    return regs->checksum == i2c_regs_calculate_checksum(regs);
}

void i2c_regs_reset(i2c_regs_t* regs) {
    memset(regs, 0, sizeof(i2c_regs_t));
}

void i2c_regs_init(i2c_regs_t* regs, uint8_t node_id, uint8_t cluster_id) {
    i2c_regs_reset(regs);
    regs->node_id = node_id;
    regs->cluster_id = cluster_id;
    regs->status = STATUS_INIT_COMPLETE;
    regs->checksum = i2c_regs_calculate_checksum(regs);
}