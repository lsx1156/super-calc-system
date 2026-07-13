#include "crack_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"

static crack_state_t g_state;
static char g_target[64];
static char g_charset[64];
static uint8_t g_key_len;

static inline uint32_t rotl(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}

static inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void md5_hash(const char* input, int len, char* output) {
    uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };
    
    uint32_t s[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };
    
    uint32_t a0 = 0x67452301;
    uint32_t b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe;
    uint32_t d0 = 0x10325476;
    
    uint8_t msg[64];
    memset(msg, 0, 64);
    memcpy(msg, input, len);
    msg[len] = 0x80;
    msg[56] = (len * 8) & 0xFF;
    msg[57] = ((len * 8) >> 8) & 0xFF;
    msg[58] = ((len * 8) >> 16) & 0xFF;
    msg[59] = ((len * 8) >> 24) & 0xFF;
    
    uint32_t* M = (uint32_t*)msg;
    
    uint32_t A = a0, B = b0, C = c0, D = d0;
    
    for (int i = 0; i < 64; i++) {
        uint32_t F, g;
        if (i < 16) {
            F = (B & C) | ((~B) & D);
            g = i;
        } else if (i < 32) {
            F = (D & B) | ((~D) & C);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            F = B ^ C ^ D;
            g = (3 * i + 5) % 16;
        } else {
            F = C ^ (B | (~D));
            g = (7 * i) % 16;
        }
        uint32_t temp = D;
        D = C;
        C = B;
        B = B + rotl((A + F + K[i] + M[g]), s[i]);
        A = temp;
    }
    
    a0 += A; b0 += B; c0 += C; d0 += D;
    
    uint8_t* p = (uint8_t*)&a0;
    for (int i = 0; i < 4; i++) sprintf(output + i*2, "%02x", p[i]);
    p = (uint8_t*)&b0;
    for (int i = 0; i < 4; i++) sprintf(output + 8 + i*2, "%02x", p[i]);
    p = (uint8_t*)&c0;
    for (int i = 0; i < 4; i++) sprintf(output + 16 + i*2, "%02x", p[i]);
    p = (uint8_t*)&d0;
    for (int i = 0; i < 4; i++) sprintf(output + 24 + i*2, "%02x", p[i]);
}

static bool brute_force(char* prefix, int depth) {
    if (depth == g_key_len) {
        g_state.attempts++;
        
        char hash[33];
        md5_hash(prefix, g_key_len, hash);
        hash[32] = '\0';
        
        if (strcmp(hash, g_target) == 0) {
            strcpy(g_state.result, prefix);
            g_state.found = true;
            return true;
        }
        
        if (g_state.attempts % 10000 == 0) {
            watchdog_update();
        }
        
        return false;
    }
    
    for (int i = 0; g_charset[i]; i++) {
        prefix[depth] = g_charset[i];
        if (brute_force(prefix, depth + 1)) return true;
        if (!g_state.running) return false;
    }
    
    return false;
}

void crack_init(void) {
    memset(&g_state, 0, sizeof(g_state));
}

void crack_start(const char* target_hash, uint8_t key_len, const char* charset) {
    strncpy(g_target, target_hash, sizeof(g_target) - 1);
    strncpy(g_charset, charset, sizeof(g_charset) - 1);
    g_key_len = key_len;
    
    g_state.running = true;
    g_state.attempts = 0;
    g_state.found = false;
    g_state.progress = 0;
    g_state.key_length = key_len;
    memset(g_state.result, 0, sizeof(g_state.result));
}

void crack_stop(void) {
    g_state.running = false;
}

crack_state_t* crack_get_state(void) {
    return &g_state;
}

void crack_task(void* pvParameters) {
    char key[32] = {0};
    
    while (1) {
        if (g_state.running && !g_state.found) {
            brute_force(key, 0);
            g_state.running = false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
