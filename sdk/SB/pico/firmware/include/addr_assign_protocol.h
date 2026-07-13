#ifndef _ADDR_ASSIGN_PROTOCOL_H
#define _ADDR_ASSIGN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// ============================================
// SPI片选线自动地址分配协议
// ============================================
// 原理：
//   Pico2通过16根独立CS线依次选中Pico，
//   通过SPI发送地址分配帧，
//   Pico接收后设置自身I2C从机地址。
//
// 物理槽位 = CS线编号 (0~15)
// I2C地址  = 0x40 + 槽位编号
// ============================================

// -------------------- 协议帧格式 --------------------
//
// 地址分配帧 (Pico2 -> Pico):
// ┌──────────┬──────────┬──────────┬──────────┬──────────┐
// │ SYNC0    │ SYNC1    │ CMD      │ SLOT_ID  │ CHECKSUM │
// │ (1B)     │ (1B)     │ (1B)     │ (1B)     │ (1B)     │
// └──────────┴──────────┴──────────┴──────────┴──────────┘
//
// 地址确认帧 (Pico -> Pico2):
// ┌──────────┬──────────┬──────────┬──────────┬──────────┐
// │ SYNC0    │ SYNC1    │ ACK      │ I2C_ADDR │ CHECKSUM │
// │ (1B)     │ (1B)     │ (1B)     │ (1B)     │ (1B)     │
// └──────────┴──────────┴──────────┴──────────┴──────────┘
//
// 心跳/状态帧:
// ┌──────────┬──────────┬──────────┬──────────┬──────────┐
// │ SYNC0    │ SYNC1    │ STATUS   │ NODE_ID  │ CHECKSUM │
// │ (1B)     │ (1B)     │ (1B)     │ (1B)     │ (1B)     │
// └──────────┴──────────┴──────────┴──────────┴──────────┘

#define ADDR_ASSIGN_SYNC0       0xA5
#define ADDR_ASSIGN_SYNC1       0x5A
#define ADDR_ASSIGN_FRAME_LEN   5

// -------------------- 命令码 --------------------
#define CMD_ADDR_ASSIGN         0x10
#define CMD_ADDR_QUERY          0x11
#define CMD_ADDR_CLEAR          0x12
#define CMD_STATUS_QUERY        0x13
#define CMD_SLOT_ID_QUERY       0x14

// -------------------- 应答码 --------------------
#define ACK_OK                  0x06
#define ACK_ERROR               0x15
#define ACK_ALREADY_ASSIGNED    0x10

// -------------------- 地址定义 --------------------
#define I2C_ADDR_UNASSIGNED     0xFF
#define I2C_ADDR_BASE           0x40
#define I2C_ADDR_MAX            0x4F
#define MAX_SLOT_COUNT          16

#define SLOT_TO_I2C_ADDR(slot)  (I2C_ADDR_BASE + (slot))
#define I2C_ADDR_TO_SLOT(addr)  ((addr) - I2C_ADDR_BASE)

// -------------------- 状态码 --------------------
#define NODE_STATE_UNASSIGNED   0x00
#define NODE_STATE_ASSIGNED    0x01
#define NODE_STATE_RUNNING     0x02
#define NODE_STATE_FAULT       0xFF

// -------------------- 帧结构 --------------------
typedef struct {
    uint8_t sync0;
    uint8_t sync1;
    uint8_t cmd;
    uint8_t data;
    uint8_t checksum;
} addr_assign_frame_t;

// -------------------- 槽位映射 --------------------
typedef struct {
    uint8_t slot_id;
    uint8_t i2c_addr;
    uint8_t node_state;
    bool    online;
    uint8_t firmware_ver;
} slot_mapping_t;

// -------------------- 计算校验和 --------------------
static inline uint8_t addr_assign_calc_checksum(const addr_assign_frame_t* frame) {
    uint8_t sum = 0;
    sum ^= frame->sync0;
    sum ^= frame->sync1;
    sum ^= frame->cmd;
    sum ^= frame->data;
    return sum;
}

// -------------------- 验证帧 --------------------
static inline bool addr_assign_validate_frame(const addr_assign_frame_t* frame) {
    if (frame->sync0 != ADDR_ASSIGN_SYNC0) return false;
    if (frame->sync1 != ADDR_ASSIGN_SYNC1) return false;
    if (frame->checksum != addr_assign_calc_checksum(frame)) return false;
    return true;
}

// -------------------- 构造分配帧 --------------------
static inline void addr_assign_build_assign_frame(addr_assign_frame_t* frame, uint8_t slot_id) {
    frame->sync0 = ADDR_ASSIGN_SYNC0;
    frame->sync1 = ADDR_ASSIGN_SYNC1;
    frame->cmd = CMD_ADDR_ASSIGN;
    frame->data = slot_id;
    frame->checksum = addr_assign_calc_checksum(frame);
}

// -------------------- 构造查询帧 --------------------
static inline void addr_assign_build_query_frame(addr_assign_frame_t* frame) {
    frame->sync0 = ADDR_ASSIGN_SYNC0;
    frame->sync1 = ADDR_ASSIGN_SYNC1;
    frame->cmd = CMD_ADDR_QUERY;
    frame->data = 0;
    frame->checksum = addr_assign_calc_checksum(frame);
}

// -------------------- 构造清除帧 --------------------
static inline void addr_assign_build_clear_frame(addr_assign_frame_t* frame) {
    frame->sync0 = ADDR_ASSIGN_SYNC0;
    frame->sync1 = ADDR_ASSIGN_SYNC1;
    frame->cmd = CMD_ADDR_CLEAR;
    frame->data = 0;
    frame->checksum = addr_assign_calc_checksum(frame);
}

// -------------------- 构造应答帧 --------------------
static inline void addr_assign_build_ack_frame(addr_assign_frame_t* frame, 
                                                uint8_t ack_code, uint8_t i2c_addr) {
    frame->sync0 = ADDR_ASSIGN_SYNC0;
    frame->sync1 = ADDR_ASSIGN_SYNC1;
    frame->cmd = ack_code;
    frame->data = i2c_addr;
    frame->checksum = addr_assign_calc_checksum(frame);
}

#endif
